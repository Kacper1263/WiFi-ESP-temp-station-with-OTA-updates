# WiFi Weather Station with OTA Updates + Supla Cloud integration

This project is a WiFi-enabled ESP32 weather station that displays indoor and outdoor temperature and humidity, as well as outdoor air quality (PM2.5 and PM10) and pressure. The ESP32 fetches outdoor data from a local air quality station near my location and displays it on an OLED screen. The device also supports OTA (Over-The-Air) updates, making it easy to deploy updates without requiring a physical connection.

> **❗ WARNING ❗**: I highly recommend using PlatformIO with VS Code for this branch. Build with Supla library is taking a lot of Flash space. In Arduino IDE you will need to change partition scheme because sketch is too big for default partition scheme. However, build from PlatformIO is takeing around 92% of Flash memory so you should be fine.

## Features

- **WiFi Connectivity**: Connects to a WiFi network using credentials stored in a separate `config.h` file.
- **OTA (Over-The-Air) Updates**: Allows for remote firmware updates via WiFi.
- **Indoor Temperature and Humidity**: Reads data from a DHT22 sensor.
- **Outdoor Data**: Fetches temperature, humidity, PM2.5, PM10 levels, and air pressure from a local air quality station.
- **OLED Display**: Displays data on a 128x64 OLED screen, alternating between indoor and outdoor data every 5 seconds.
- **Supla Integration**: Sends indoor temperature/humidity and API data to the Supla cloud platform.

## Libraries Used

- **WiFi.h**: For WiFi connectivity.
- **HTTPClient.h**: For making HTTP requests to fetch outdoor weather data.
- **Arduino_JSON.h**: For parsing JSON data from the API.
- **Adafruit_SSD1306**: For controlling the OLED screen.
- **DHT.h**: For interfacing with the DHT22 temperature and humidity sensor.
- **ArduinoOTA**: For enabling OTA updates.
- **SuplaDevice.h**: For sending data to the Supla cloud platform - [supla-device repo](https://github.com/SUPLA/supla-device/)

## Hardware Requirements

- **ESP32**: Microcontroller with WiFi capabilities.
- **DHT22 Sensor**: Measures indoor temperature and humidity.
- **128x64 OLED Display**: Displays weather data.

## Configuration

To use this project, you need to create a `config.h` file in the project directory. This file should contain your WiFi SSID and password.


## Installation and Usage

1. **Clone this repository**.
2. **Install the required libraries** (listed above).
3. **Create the `config.h` file** with your WiFi credentials as described in the Configuration section.
4. **Upload the code to your ESP32** using the Arduino IDE or VS Code with PlatformIO.
5. 🟢**SUPLA CONFIGURATION**🟢: If you didn't configured Supla yet on this device, it will start in configuration mode. Connect to the WiFi network named something like "SUPLA-ESP...", open your browser and go to `192.168.4.1` and then, follow the instructions on the website. Configuration will be saved in device memory and will be used on next boot.<br/><br/>**BEFORE SAVING** configuration, make sure you have enabled adding new devices in you Supla cloud panel. You can also view your server address on Supla cloud panel
6. **View data on the OLED screen** once the ESP32 connects to your WiFi network and retrieves weather data.
7. Now you should see all data on the OLED and also in Supla App.

## OTA (Over-The-Air) Updates

The ESP32 is set up for OTA updates. To update the firmware:

Arduino IDE:
1. Connect the ESP32 to your WiFi network - WiFi network must be the same as your computer's network.
2. In the Arduino IDE, select the ESP32's IP address from the **Port** menu.
3. **Upload** new firmware directly over WiFi.

VS Code with PlatformIO:
1. ~onnect the ESP32 to your WiFi network - WiFi network must be the same as your computer's network.
2. Change the `upload_port` in the `platformio.ini` file to the ESP32's IP address (displayed on startup on the OLED screen).
3. **Upload** new firmware using the PlatformIO upload button with OTA environment selected.


During an OTA update, progress is displayed on the OLED screen, and normal operation resumes after the update completes.

## Functionality Overview

- **Setup**: Initializes the WiFi, OLED display, DHT sensor, and Supla.
- **Loop**: 
  - **OTA Handling**: Checks for OTA updates.
  - **Weather Data Fetching**: Calls `fetchWeatherData()` every 5 minutes to get updated outdoor data.
  - **DHT Sensor Reading**: Reads indoor temperature and humidity every 1 second from Supla DHT.
  - **Display Toggle**: Switches between indoor and outdoor data every 5 seconds.

## API Endpoint

The code fetches outdoor weather data from the API endpoint (thanks to "InPost Green City" ❤) `https://greencity.pl/shipx-point-data/317/KRA357M/air_index_level`. Replace this URL in the code if needed.

## Example Display Output

Indoor: <br />
Temp: 23.5°C <br />
Humidity: 45%

Outdoor: GOOD <br />
Temp: 22.8°C <br />
Humidity: 50% <br />
PM2.5: 10% <br />
PM10: 20% <br />
Pressure: 1013 hPa