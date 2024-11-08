#include <Arduino.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//#include <DHT.h>
#include <config.h> // contains WIFI_SSID and WIFI_PASS - create your own config.h from example file
#include <ArduinoOTA.h>

// Supla
#include <SuplaDevice.h>
#include <supla/network/esp_wifi.h>
#include <supla/device/status_led.h>
#include <supla/storage/littlefs_config.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/device/supla_ca_cert.h>
#include <supla/sensor/general_purpose_measurement.h>
#include <supla/sensor/virtual_thermometer.h>
#include <supla/sensor/DHT.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

const String apiUrl = "https://greencity.pl/shipx-point-data/317/KRA357M/air_index_level";

#define DHTPIN 4      // Define the pin connected to the DHT sensor
#define DHTTYPE DHT22 // Define sensor type (DHT22)

float lastTemperature = -1, lastHumidity = -1, lastPM25Percent = -1, lastPM10Percent = -1, lastPressure = -1;
String lastQuality = "";
float indoorTemperature = -1, indoorHumidity = -1;

unsigned long previousAPIMillis = 0, previousDHTMillis = 0, previousDisplayMillis = 0;
const long apiInterval = 300000;      // 5 minutes in milliseconds
const long dhtInterval = 1000;        // 5 seconds in milliseconds
const long displayInterval = 5000;    // 5 seconds display switch interval
bool displayOutdoor = false;           // Toggle display between indoor and outdoor

bool wifiError = false;

#pragma region Supla
Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(LED_BUILTIN, false); 
Supla::EspWebServer suplaServer;

// HTML www component (they appear in sections according to creation sequence)
Supla::Html::DeviceInfo htmlDeviceInfo(&SuplaDevice);
Supla::Html::WifiParameters htmlWifi;
Supla::Html::ProtocolParameters htmlProto;
Supla::Html::StatusLedParameters htmlStatusLed;

Supla::Sensor::DHT *dht = nullptr;
Supla::Sensor::VirtualThermometer *apiTemp = nullptr;
Supla::Sensor::GeneralPurposeMeasurement *apiHumi = nullptr;
Supla::Sensor::GeneralPurposeMeasurement *apiPM25 = nullptr;
Supla::Sensor::GeneralPurposeMeasurement *apiPM10 = nullptr;
Supla::Sensor::GeneralPurposeMeasurement *apiPres = nullptr;
#pragma endregion

void setup() {
  Serial.begin(115200);
  pinMode(DHTPIN, INPUT_PULLUP);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
  display.setCursor(0, 0);
  display.println("Starting...");
  display.display();

  dht = new Supla::Sensor::DHT(DHTPIN, DHTTYPE);
  apiTemp = new Supla::Sensor::VirtualThermometer();
  apiHumi = new Supla::Sensor::GeneralPurposeMeasurement();
  apiPM25 = new Supla::Sensor::GeneralPurposeMeasurement();
  apiPM10 = new Supla::Sensor::GeneralPurposeMeasurement();
  apiPres = new Supla::Sensor::GeneralPurposeMeasurement();

#pragma region Supla default setup
  apiHumi->setDefaultValueDivider(0);
  apiHumi->setDefaultValueMultiplier(0);
  apiHumi->setDefaultValueAdded(0);
  apiHumi->setDefaultValuePrecision(2);
  apiPM25->setDefaultValueDivider(0);
  apiPM25->setDefaultValueMultiplier(0);
  apiPM25->setDefaultValueAdded(0);
  apiPM25->setDefaultValuePrecision(2);
  apiPM10->setDefaultValueDivider(0);
  apiPM10->setDefaultValueMultiplier(0);
  apiPM10->setDefaultValueAdded(0);
  apiPM10->setDefaultValuePrecision(2);
  apiPres->setDefaultValueDivider(0);
  apiPres->setDefaultValueMultiplier(0);
  apiPres->setDefaultValueAdded(0);
  apiPres->setDefaultValuePrecision(2);

  apiTemp->setValue(-1);
  apiHumi->setValue(-1);
  apiPM25->setValue(-1);
  apiPM10->setValue(-1);
  apiPres->setValue(-1);
#pragma endregion

  display.println("Supla configured");
  display.display();

  SuplaDevice.setSuplaCACert(suplaCACert);
  SuplaDevice.setSupla3rdPartyCACert(supla3rdCACert);

  SuplaDevice.setAutomaticResetOnConnectionProblem(60);
  SuplaDevice.begin();

  delay(2000);
}

bool otaReady = false;

void loop() {
  SuplaDevice.iterate();
  if(WiFi.status() == WL_CONNECTED && !otaReady){
    // OTA
    ArduinoOTA.setPassword("wifi-temp-ota");
    ArduinoOTA
      .onStart([]() {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("OTA update started");
        display.display();
      })
      .onEnd([]() {
        display.println("\nUpdate complete");
        display.display();
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("OTA update started");
        display.printf("Progress: %u%%\r", (progress / (total / 100)));
        display.display();
      })
      .onError([](ota_error_t error) {
        display.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) {display.println("Begin Failed"); display.display();}
        else if (error == OTA_CONNECT_ERROR) {display.println("Connect Failed"); display.display();}
        else if (error == OTA_RECEIVE_ERROR) {display.println("Receive Failed"); display.display();}
        else if (error == OTA_END_ERROR) {display.println("End Failed"); display.display();}
      });
    ArduinoOTA.begin();
    otaReady = true;

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi and OTA ready");
    display.println(WiFi.localIP());
    display.display();
    delay(5000);
  }

  if(otaReady){
    ArduinoOTA.handle();
  }


  unsigned long currentMillis = millis();

  // Fetch API data every 5 minutes
  if (currentMillis - previousAPIMillis >= apiInterval || previousAPIMillis == 0) {
    fetchWeatherData();
    previousAPIMillis = currentMillis;
  }

  // Read DHT22 sensor data every 1 second
  if (currentMillis - previousDHTMillis >= dhtInterval || previousDHTMillis == 0) {
    indoorTemperature = dht->getTemp();
    indoorHumidity = dht->getHumi();
    Serial.print("Indoor Temperature: ");
    Serial.print(indoorTemperature);
    Serial.print("°C, Indoor Humidity: ");
    Serial.print(indoorHumidity);
    Serial.println("%");
    previousDHTMillis = currentMillis;
  }

  // Switch display between indoor and outdoor data every 5 seconds
  if (currentMillis - previousDisplayMillis >= displayInterval || previousDisplayMillis == 0) {
    displayOutdoor = !displayOutdoor;

    if (displayOutdoor) {
      displayWeatherData("Outdoor", lastTemperature, lastHumidity, lastPM25Percent, lastPM10Percent, lastPressure, lastQuality);
    } else {
      displayWeatherData("Indoor", indoorTemperature, indoorHumidity, -1, -1, -1, "");
    }
    previousDisplayMillis = currentMillis;
  }
}

void fetchWeatherData() {
  // check is wifi connected
  if (WiFi.status() != WL_CONNECTED) {
    lastQuality = "DISCONNECTED"; // set quality to disconnected - this will be displayed on the screen every 5 seconds
    displayError("WiFi connection error");
    return;
  }

  HTTPClient http;
  http.begin(apiUrl);
  http.addHeader("X-Requested-With", "XMLHttpRequest");

  int httpResponseCode = http.POST("");
  if (httpResponseCode == 200) {
    String payload = http.getString();
    JSONVar response = JSON.parse(payload);

    if (JSON.typeof(response) == "undefined") {
      Serial.println("Parsing failed!");
      displayError("API parse error");
    } else {
      lastTemperature = parseSensorValue(response, "TEMPERATURE");
      lastHumidity = parseSensorValue(response, "HUMIDITY");
      lastPM25Percent = parseSensorPercentage(response, "PM25");
      lastPM10Percent = parseSensorPercentage(response, "PM10");
      lastPressure = parseSensorValue(response, "PRESSURE");
      if (response.hasOwnProperty("air_index_level") && JSON.typeof(response["air_index_level"]) != "null") {
        lastQuality = (const char*)response["air_index_level"];
      } else {
        Serial.println("Air Index Level is not available.");
      }

      // Set Supla sensors values
      apiTemp->setValue(lastTemperature);
      apiHumi->setValue(lastHumidity);
      apiPM25->setValue(lastPM25Percent);
      apiPM10->setValue(lastPM10Percent);
      apiPres->setValue(lastPressure);
    }
  } else {
    displayError("API request error");
  }

  http.end();
}

float parseSensorValue(JSONVar response, String sensorName) {
  JSONVar sensors = response["air_sensors"];
  for (int i = 0; i < sensors.length(); i++) {
    String sensorData = sensors[i];
    int pos = sensorData.indexOf(":");
    String name = sensorData.substring(0, pos);
    if (name == sensorName) {
      String value = sensorData.substring(pos + 1, sensorData.indexOf(":", pos + 1));
      return value.toFloat();
    }
  }
  return -1;  // return -1 if sensor not found
}

float parseSensorPercentage(JSONVar response, String sensorName) {
  JSONVar sensors = response["air_sensors"];
  for (int i = 0; i < sensors.length(); i++) {
    String sensorData = sensors[i];
    int firstColon = sensorData.indexOf(":");
    int secondColon = sensorData.indexOf(":", firstColon + 1);
    String name = sensorData.substring(0, firstColon);
    if (name == sensorName && secondColon != -1) {
      String percentage = sensorData.substring(secondColon + 1);
      return percentage.toFloat();
    }
  }
  return -1;  // return -1 if sensor not found
}

void displayWeatherData(String source, float temperature, float humidity, float pm25Percent, float pm10Percent, float pressure, String quality) {
  display.clearDisplay();
  int yPos = 0; 
  int lineSpacing = 10; 

  display.setCursor(0, yPos);
  display.print(source);
  if (source == "Outdoor") {
    display.print(": ");
    display.print(quality);
  }
  yPos += lineSpacing;

  display.setCursor(0, yPos);
  display.print("Temp: ");
  display.print(temperature);
  display.print((char)247);
  display.println("C");
  yPos += lineSpacing;

  display.setCursor(0, yPos);
  display.print("Humidity: ");
  display.print(humidity);
  display.println("%");
  yPos += lineSpacing;

  if (source == "Outdoor") {
    display.setCursor(0, yPos);
    display.print("PM2.5: ");
    display.print(pm25Percent);
    display.println("%");
    yPos += lineSpacing;

    display.setCursor(0, yPos);
    display.print("PM10: ");
    display.print(pm10Percent);
    display.println("%");
    yPos += lineSpacing;

    display.setCursor(0, yPos);
    display.print("Pressure: ");
    display.print(pressure);
    display.println(" hPa");
  }
  
  display.display();
}

void displayError(String message) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(message);
  display.display();
}
