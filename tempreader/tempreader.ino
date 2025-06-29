#include <Wire.h>               // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h" 
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

// For storing credentials
Preferences preferences;

// Keep empty unless you want to hard code credentials.
String server = "";
String ssid = "";
String password = "";

// Open access point to get credentials
const char* open_ssid     = "READER";
const char* open_password = "";
// Create AsyncWebServer object on port 80
AsyncWebServer webServer(80);

SSD1306Wire display(0x3c, SDA, SCL);  


void setup() {
  Serial.begin(9600);
  delay(5000);
  Serial.println("");
  Serial.println("Startup");

  display.init();
  display.setContrast(255);
  display.setFont(ArialMT_Plain_24);

  // Mode is both access point and client
  if(checkAP()) {
    Serial.println("Turning on AP");
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect();
    delay(100);

    // Access point
    Serial.println("\n[*] Creating AP");
    WiFi.softAP(open_ssid, open_password);
    Serial.print("[+] AP Created with IP Gateway ");
    Serial.println(WiFi.softAPIP());
    Serial.println();
    // Start server
    Serial.println("Server begin");
    webServer.begin();
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
  }

  // Client
  connectWiFi();
  // Give some time to connect
  delay(500);

   // Routing of web pages. ON_AP_FILTER is for access point
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (ON_AP_FILTER(request)) {   
        request->send(200, "text/html", "<p>ESP32 IP Address: " + 
          WiFi.localIP().toString() + "</p>" + 
          scanAvailableNetworks());
    }  
  });

  webServer.on("/pick", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (ON_AP_FILTER(request)) {  
      handlePickRequest(request);
    }
  });

  webServer.on("/connect", HTTP_GET, [](AsyncWebServerRequest *request) {
     if (ON_AP_FILTER(request)) {  
      handleConnectRequest(request);
    }
  });


}

void loop() {
  display.clear();

  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  String data = fetchDataFromServer();
  display.drawString(90, 20, data+"°");  // x=128, height 64
  // write the buffer to the display
  display.display();

  delay(10000);
}

String fetchDataFromServer() {
  WiFiClient client;
  HTTPClient http;
  char buf[10];

  http.begin(client, server);
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    if (httpResponseCode == 200) {
      String payload = http.getString();
      float tempValue = round(payload.toFloat());
      sprintf(buf, "%.0f", tempValue);
      payload = String(buf);
      http.end();
      return payload; 
    } else {
      http.end();
      return "E: " + String(httpResponseCode);
    }
  } else {
    http.end();
    return "E: " + String(httpResponseCode);
  }
  // Disconnect
  http.end();
  WiFi.disconnect();
  return "Error";
}

void connectWiFi() {
  // Get stored in EEPROM
  preferences.begin("credentials", false);
  ssid = preferences.getString("ssid", ""); 
  password = preferences.getString("password", "");
  server = preferences.getString("server", server);
  preferences.end();

  // No data do not connect
  if (ssid == "" || password == ""){
    return;
  }

  Serial.println("Connecting to Wifi");
  Serial.println(ssid);
  Serial.println(password);
    // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  int count = 0;
  while (WiFi.status() != WL_CONNECTED && count++ < 20) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println(WiFi.localIP());
    storeAP(false);
  } else {  
      Serial.print("Connect failed with code: ");
      Serial.println(WiFi.status());
      storeAP(true);
  }
}

// Store WiFi credentials in EEPROM
void storeWiFi(String s, String p, String v) {
  preferences.begin("credentials", false);
  preferences.putString("ssid", s); 
  preferences.putString("password", p);
  preferences.putString("server", v);
  Serial.println("Network Credentials Saved using Preferences");
  preferences.end();
}

// Store if AccessPoint enabled in EEPROM
void storeAP(bool enable) {
  preferences.begin("active", false);
  preferences.putBool("ap", enable);
  Serial.println("Access point enabled Saved using Preferences");
  preferences.end();
}

bool checkAP() {
  bool enable = true;
  preferences.begin("active", false);
  enable = preferences.getBool("ap", true); 
  preferences.end();
  return enable;
}

// Helper function to URL encode parameters passed in http requests
String urlencode(String str) {
    String encodedString = "";
    for (unsigned int i = 0; i < str.length(); i++) {
        if (isalnum(str.charAt(i)) || (str.charAt(i) == '-') || (str.charAt(i) == '_') || (str.charAt(i) == '.') || (str.charAt(i) == '~')) {
            encodedString += str.charAt(i);
        } else if (str.charAt(i) == ' ') {
            encodedString += '+';
        } else {
            encodedString += '%';
            char hex[3];
            sprintf(hex, "%02X", (int)str.charAt(i));
            encodedString += hex;
        }
    }
    return encodedString;
}

// Scan for available networks
String scanAvailableNetworks() {
  WiFi.disconnect(true);
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.print("Found ");
  Serial.print(n);
  Serial.println(" networks.");

  String htmlList = "";
  if (n == 0) {
    htmlList = "<p style='font-size: 1.5em;'>No Wi-Fi networks found.</p>";
  } else {
    htmlList += "<ol>";
    for (int i = 0; i < n; ++i) {
      String currentSSID = WiFi.SSID(i);
      htmlList += "<li style='font-size: 1.5em;'><a href='/pick?ssid=";
      htmlList += urlencode(currentSSID); // Encode SSID for URL safety
      htmlList += "'>";
      htmlList += currentSSID.c_str();
      htmlList += "</a></li>";
    }
    htmlList += "</ol>";
  }
  return htmlList;
}

// If an SSID is picked from the list of SSIDs ask for password
void handlePickRequest(AsyncWebServerRequest *request) {
  if (request->hasParam("ssid")) {
    String pickedSSID = request->getParam("ssid")->value();
    Serial.print("User picked SSID: ");
    Serial.println(pickedSSID);
    String htmlResponse = "<!DOCTYPE html><html><head><title>Connect to Wi-Fi</title></head><body>";
    htmlResponse += "<h1 style='font-size: 2em;'>Connect to:</h1>";
    htmlResponse += "<p style='font-size: 1.5em;'>" + pickedSSID + "</p>";
    htmlResponse += "<form action='/connect' method='get'>";
    htmlResponse += "<label for='password'>Password:</label>";
    htmlResponse += "<input type='password' id='password' name='password'><br><br>";
    htmlResponse += "<label for='server'>Server:</label>";
    htmlResponse += "<input type='text' id='server' name='server' value='http://192.168.0.17/ws'><br><br>";
    htmlResponse += "<input type='hidden' name='ssid' value='" + pickedSSID + "'>";
    htmlResponse += "<input type='submit' value='Connect'>";
    htmlResponse += "</form>";
    htmlResponse += "<p><a href='/'>Back</a></p>";
    htmlResponse += "</body></html>";
    request->send(200, "text/html", htmlResponse);
  } else {
    request->send(400, "text/plain", "Error: SSID parameter missing");
  }
}

// With SSID and password store credentials and go for boot
void handleConnectRequest(AsyncWebServerRequest *request) {
  if (request->hasParam("ssid") && request->hasParam("password")) {
    ssid = request->getParam("ssid")->value();
    password = request->getParam("password")->value();
    server = request->getParam("server")->value();
    storeWiFi(ssid, password, server);
  
    String htmlResponse = "<!DOCTYPE html><html><head><title>Connecting...</title></head><body>";
    htmlResponse += "<h1 style='font-size: 2em;'>Attempting to connect to:</h1>";
    htmlResponse += "<p style='font-size: 1.5em;'>SSID: " + ssid + "</p>";
    htmlResponse += "<p style='font-size: 1.5em;'>Password: " + password + "</p>";
    htmlResponse += "<p>Connecting... (check serial monitor for status)</p>";
    htmlResponse += "<p><a href='/'>Home</a></p>";
    htmlResponse += "</body></html>";
    request->send(200, "text/html", htmlResponse);
    delay(1000);
    ESP.restart();
  } else {
    request->send(400, "text/plain", "Error: SSID or Password parameter missing");
  }
}
