#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <config.h> // contains WIFI_SSID and WIFI_PASS - create your own config.h from example file
#include <ArduinoOTA.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const bool flipDisplay = true;
unsigned long lastDisplayModeChange = 0;
bool isInvertedDisplay = false;
const unsigned long displayInvertInterval = 60 * 60 * 1000; 
const unsigned long displayInvertDuration = 5 * 60 * 1000; 

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

const String apiUrl = "https://greencity.pl/shipx-point-data/317/KRA357M/air_index_level";

#define DHTPIN 4      // Define the pin connected to the DHT sensor
#define DHTTYPE DHT22 // Define sensor type (DHT22)
DHT dht(DHTPIN, DHTTYPE);

float lastTemperature = -1, lastHumidity = -1, lastPM25Percent = -1, lastPM10Percent = -1, lastPressure = -1;
String lastQuality = "";
float indoorTemperature = -1, indoorHumidity = -1;

unsigned long previousAPIMillis = 0, previousDHTMillis = 0, previousDisplayMillis = 0;
const long apiInterval = 300000;      // 5 minutes in milliseconds
const long dhtInterval = 5000;        // 5 seconds in milliseconds
const long displayInterval = 5000;    // 5 seconds display switch interval
bool displayOutdoor = false;           // Toggle display between indoor and outdoor

bool wifiError = false;

void setup() {
  Serial.begin(115200);
  pinMode(DHTPIN, INPUT_PULLUP);
  pinMode(BUILTIN_LED, OUTPUT);
  dht.begin();
  
  WiFi.mode(WIFI_OFF);
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
  display.setCursor(0, 0);
  display.println("Starting...");
  display.println("Connecting to WiFi...");
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    displayError("Err. while connecting");
    display.println("L:" + String(WIFI_SSID));
    display.println("H:" + String(WIFI_PASS));
    display.display();
    WiFi.disconnect(true);
    delay(5000);
    return;
  }

  display.println(WiFi.localIP());
  display.display();

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
}

void loop() {
  ArduinoOTA.handle();
  unsigned long currentMillis = millis();

  if(WiFi.status() != WL_CONNECTED) {
    digitalWrite(BUILTIN_LED, HIGH);
  }
  else {
    digitalWrite(BUILTIN_LED, LOW);
  }

  // flip screen to prevent burn-in
  if(flipDisplay){
    if ((currentMillis - lastDisplayModeChange) >= displayInvertInterval + displayInvertDuration) {
      display.invertDisplay(true);
      isInvertedDisplay = true;
      lastDisplayModeChange = currentMillis;
    } else if (isInvertedDisplay && (currentMillis - lastDisplayModeChange) >= displayInvertDuration) {
      display.invertDisplay(false);
      isInvertedDisplay = false;
    }
  } 

  // Fetch API data every 5 minutes
  if (currentMillis - previousAPIMillis >= apiInterval || previousAPIMillis == 0) {
    fetchWeatherData();
    previousAPIMillis = currentMillis;
  }

  // Read DHT22 sensor data every 5 seconds
  if (currentMillis - previousDHTMillis >= dhtInterval || previousDHTMillis == 0) {
    // wait 2s on first run to let the sensor initialize
    if (previousDHTMillis == 0) {
      // print initializing message
      display.println("Initializing sensor..");
      display.display();
      delay(2000);
    }

    indoorTemperature = dht.readTemperature();
    indoorHumidity = dht.readHumidity();
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
    // try to reconnect
    WiFi.reconnect();
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      displayError("WiFi connection error");
      return;
    }
  }

  // show fetching message on first run
  if(previousAPIMillis == 0){
    display.println("Fetching data...");
    display.display();
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
