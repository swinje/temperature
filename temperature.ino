
#include <OneWire.h>
#include <DallasTemperature.h>
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"
#include <stdint.h>
#include <WiFiS3.h>
#include <stdarg.h>
#include <ArduinoBLE.h>
#include <EEPROM.h>
#include <aWOT.h>


// Buzzer Arduino digital pin
#define BUZZER_PIN 9

int status = WL_IDLE_STATUS;
WiFiServer server(80);
bool reconnect = false;
Application app;

ArduinoLEDMatrix matrix;

// Sensor Arduino digital pin 
#define ONE_WIRE_BUS 4

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// Bluetooth service and read/writable charcteristic
const int characteristicSize = 128;
BLEService tempService("180A"); 
BLEStringCharacteristic tempCharacteristic("2A58",
    BLERead | BLENotify | BLEWriteWithoutResponse, characteristicSize);

String ssid; 
String pass; 

void setup(void)
{

  // Start serial communication for debugging purposes
  Serial.begin(9600);
  delay(1500);
  Serial.println("Startup");

  BLESetup();
  sensors.begin();

  matrix.loadSequence(LEDMATRIX_ANIMATION_WIFI_SEARCH);
  matrix.begin();
  matrix.play(true);


  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION)
    Serial.println("Please upgrade the firmware");
 
  getStoredWifiCredentials();

}

void playBuzzer() {
  int melody[] = {
    262, 196, 196, 220, 196, 0, 247, 262
  };

  // note durations: 4 = quarter note, 8 = eighth note, etc.:
  int noteDurations[] = {
    4, 8, 8, 4, 4, 4, 4, 4
  };
  
  for (int thisNote = 0; thisNote < 8; thisNote++) {

    // to calculate the note duration, take one second divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 1000 / noteDurations[thisNote];
    tone(BUZZER_PIN, melody[thisNote], noteDuration);

    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    // stop the tone playing:
    noTone(BUZZER_PIN);
  }
}

/*
    WL_NO_SHIELD        = 255, 
    WL_IDLE_STATUS      = 0,
    WL_NO_SSID_AVAIL    = 1,
    WL_SCAN_COMPLETED   = 2,
    WL_CONNECTED        = 3,
    WL_CONNECT_FAILED   = 4,
    WL_CONNECTION_LOST  = 5,
    WL_DISCONNECTED     = 6
*/

void connectWifi() {
  // Clear connection status
  Serial.println("Connecting to WIFI");
  status = WL_IDLE_STATUS;
  status = WiFi.begin(ssid.c_str(), pass.c_str());
  // wait 10 seconds for connection and print a dot for each second on serial
  for (int x = 0; x < 10; x++)
  {
    delay(1000); 
    Serial.print(".");  
  }
  Serial.println("");

  // If connection failed
  if(status != WL_CONNECTED) {
    Serial.print("Connect failed: ");
    Serial.println(status);
    Serial.println(ssid.c_str());
    Serial.println(pass.c_str());
  } 

  // If connection was a success
  if(status == WL_CONNECTED) {
    // Print WIFI status til serial
    printWifiStatus();
    // set up web routing
    app.get("/", &index);
    app.get("/ws", &ws);
    app.notFound(&notFound);
    server.begin();
    // Inform IP on BLE
    tempCharacteristic.writeValue(WiFi.localIP().toString());
    // Play buzzer to announce we are open for business
    playBuzzer();
  } else {
    // Inform IP on BLE 0.0.0.0 is no IP
    tempCharacteristic.writeValue("0.0.0.0");
  }
}

// Enable and announce Bluetooth
void BLESetup()  {
  // Bluetooth
   if (!BLE.begin()) {
    Serial.println("Bluetooth® Low Energy module failed!");
    while (1);
  }
  Serial.println("Bluetooth® Low Energy module enabled");

  BLE.setDeviceName("TEMP");
  BLE.setLocalName("TEMP");
  BLE.setAdvertisedService(tempService);
  tempService.addCharacteristic(tempCharacteristic);
  BLE.setEventHandler(BLEConnected, connectHandler);
  BLE.setEventHandler(BLEDisconnected, disconnectHandler);
  tempCharacteristic.setEventHandler(BLEWritten, incomingDataHandler);
  BLE.addService(tempService);
  BLE.advertise();
}

// Serve when / URL i requested
void index(Request &req, Response &res) {
  String temp = String(getTemperature()); 
  res.set("Content-Type", "text/html");
  res.println("<meta http-equiv='refresh' content='10'>");
  res.println("<!DOCTYPE html>");
  res.println("<html lang='en'>");
  res.println("<head>");
  res.println("  <meta charset='UTF-8' />");
  res.println("  <meta name='viewport' content='width=device-width, initial-scale=1.0'/>");
  res.println("  <title>Temperature Display</title>");
  res.println("  <style>");
  res.println("    body {");
  res.println("      display: flex;");
  res.println("      justify-content: center;");
  res.println("      align-items: center;");
  res.println("      height: 100vh;");
  res.println("      margin: 0;");
  res.println("      background: linear-gradient(135deg, #89f7fe 0%, #66a6ff 100%);");
  res.println("      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;");
  res.println("    }");
  res.println("    .temperature-card {");
  res.println("      background-color: white;");
  res.println("      padding: 2rem 3rem;");
  res.println("      border-radius: 20px;");
  res.println("      box-shadow: 0 8px 20px rgba(0, 0, 0, 0.15);");
  res.println("      text-align: center;");
  res.println("    }");
  res.println("    .temperature-value {");
  res.println("      font-size: 4rem;");
  res.println("      font-weight: bold;");
  res.println("      color: #333;");
  res.println("    }");
  res.println("    .temperature-unit {");
  res.println("      font-size: 2rem;");
  res.println("      color: #555;");
  res.println("      vertical-align: super;");
  res.println("    }");
  res.println("    .location {");
  res.println("      font-size: 1.2rem;");
  res.println("      color: #666;");
  res.println("      margin-top: 0.5rem;");
  res.println("    }");
  res.println("  </style>");
  res.println("</head>");
  res.println("  <script>");
  res.println("    window.addEventListener('load', () => {");
  res.println("      const now = new Date();");
  res.println("      const time = now.toLocaleTimeString();");
  res.println("      document.getElementById('datetime').textContent = `Time: ${time}`;");
  res.println("    });");
  res.println("  </script>");
  res.println("<body>");
  res.println("  <div class='temperature-card'>");
  res.print("    <div class='temperature-value'>");
  res.print(temp);
  res.println("<span class='temperature-unit'>&deg;C</span></div>");
  res.println("    <div class=\"date-time\" id=\"datetime\"></div>");
  res.println("</body>");
  res.println("</html>");
}

// Serve when /ws URL is requested
void ws(Request &req, Response &res) {
  String temp = String(getTemperature()); 
  res.print(temp);
  res.end();
}

// Requested web page not found
void notFound(Request &req, Response &res) {
  res.set("Content-Type", "application/json");
  res.print("{\"error\":\"This is not the page you are looking for.\"}");
}

void loop() {

  // Check for Bluetooth connections
  BLE.poll();
 
  // Show temperature on Arduino R4 WIFI LED
  getLEDTemperature();

  // Connect og reconnect if new credentials received via BLE
  if(reconnect) {
    reconnect = false;
    connectWifi();  
  }

  // Serve web if connected to WIFI
  if(status = WL_CONNECTED) {
    WiFiClient client = server.available();
    if (client.connected()) {
      app.process(&client);
    }
    client.stop();
  }
}

// Called when Bluetooth connects
void connectHandler(BLEDevice central) {
  // central connected event handler
  Serial.print("Connected event, central: ");
  Serial.println(central.address());
}

// Called when Bluetooth disconnects
void disconnectHandler(BLEDevice central) {
  // central disconnected event handler
  Serial.print("Disconnected event, central: ");
  Serial.println(central.address());
}

void incomingDataHandler(BLEDevice central, BLECharacteristic characteristic) {
  int i;
  String data;
  String name;
  String value;

  // Data received via BLE
  data = tempCharacteristic.value();

  // sent as text string name:value
  // no spaces allowed
  // network and passwords are case sensitive make sure to transmit accordingly
  // ssid must be transmitted before pass

  // parse name value pair
  i = data.indexOf(':'); 
  name = data.substring(0, i);
  value = data.substring(i+1);
  
  // new ssid
  if(name.equalsIgnoreCase("ssid")) {
    ssid = value;
    Serial.println("New ssid received");
    Serial.println(ssid);

    // Store WIFI credentials
    storeWifiCredentials();
    reconnect = true;
  }

  // new password
  if(name.equalsIgnoreCase("pass")) {
    pass = value;

    Serial.println("New pass received");
    Serial.println(pass);

    // Store WIFI credentials
    storeWifiCredentials();
    reconnect = true;
  }
}

// Store wifi SSID and password to EEPROM
void storeWifiCredentials() {
  char charBuf[50];
  ssid.toCharArray(charBuf, 50);
  EEPROM.put(0, charBuf); 
  pass.toCharArray(charBuf, 50);
  EEPROM.put(100, charBuf); 
}

// Retrieve stored SSID and password from EEPROM
void getStoredWifiCredentials() {
  char eepromData[100]; 

  EEPROM.get(0, eepromData);
  if(eepromData[0]!= -1) {
    ssid = String(eepromData);
    EEPROM.get(100, eepromData);
    if(eepromData[0]!= -1) {
      pass = String(eepromData);
      reconnect = true;
    }
  }  else {
    Serial.println("No stored credentials");
  } 
}


// Retreive temperature in Celcius from sensor
float getTemperature() {
  sensors.requestTemperatures();   
  float tempCelsius = sensors.getTempCByIndex(0);
  return tempCelsius;
}

// Show IP address on Arduino R4 WIFI LED screen
void showIP(String text) {
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textScrollSpeed(50);
  matrix.textFont(Font_5x7);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.println(text);
  matrix.endText(SCROLL_LEFT);
  matrix.endDraw();
}

// Show temperature on Arduino R4 WIFI LED screen
void getLEDTemperature() {
  String tempC = String(getTemperature());
  matrix.beginDraw();
  matrix.textFont(Font_5x7);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.println(tempC+"°C");
  matrix.endText(SCROLL_LEFT);
  matrix.endDraw();
}

// Print IP and WIFI strength to serial
void printWifiStatus() {
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  showIP(WiFi.localIP().toString());

  // print the received signal strength:
  Serial.print("signal strength (RSSI):");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
}






