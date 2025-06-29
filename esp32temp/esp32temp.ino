
/***************************

This ESP32 temperature probe reports temperature via WiFi
For the probe to operate it needs to connect to local WIFI
Therefore it establishes an accesspoint (SSID) called "TEMP"
This accesspoint can be connected to without password

To program the probe to connect to local WiFI, find the IP of the gateway for TEMP
and send use the following URL

http://<ip_of_gateway

You will be met by a page that will scan for access points. You can then select the
appropriate accesspoint. On the next page you will be asked for the password.
After clicking connect, the ESP32 will store the credentials and reboot.

You can check if it establishes a connection by checking http://<ip_of_gateway/
If 0.0.0.0 is return it has not connected, otherwise you have your IP

You can view readings as follows
http://<your_ip/  for formatted output
http://<your_ip>/ws for raw output



***************************/


// Import required libraries
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>


// For storing credentials
Preferences preferences;

// Buzzer digital pin
#define BUZZER_PIN 18

// Data wire is connected to GPIO 4
#define ONE_WIRE_BUS 23

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
String temperatureC = "";

// Keep empty unless you want to hard code credentials.
String ssid = "";
String password = "";

// Open access point to get credentials
const char* open_ssid     = "TEMP";
const char* open_password = "";
// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Function for reading termperature
String readDSTemperatureC() {
  sensors.requestTemperatures(); 
  float tempC = sensors.getTempCByIndex(0);

  if(tempC == -127.00) {
    Serial.println("Failed to read from DS18B20 sensor");
    return "--";
  } 
  return String(tempC);
}

// Pretty formatting for web page showing temperature

const char index_html[] PROGMEM = R"rawliteral(
<meta http-equiv='refresh' content='10'>
<meta http-equiv="Cache-Control" content="no-cache, no-store, must-revalidate">
<!DOCTYPE html>
<html lang='en'>
<head>
  <meta charset='UTF-8' />
  <meta name='viewport' content='width=device-width, initial-scale=1.0'/>
  <title>Temperature Display</title>
  <style>
    body {
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
      margin: 0;
      background: linear-gradient(135deg, #89f7fe 0%, #66a6ff 100%);
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    }
    .temperature-card {
      background-color: white;
      padding: 2rem 3rem;
      border-radius: 20px;
      box-shadow: 0 8px 20px rgba(0, 0, 0, 0.15);
      text-align: center;
    }
    .temperature-value {
      font-size: 4rem;
      font-weight: bold;
      color: #333;
    }
    .temperature-unit {
      font-size: 2rem;
      color: #555;
      vertical-align: super;
    }
    .location {
      font-size: 1.2rem;
      color: #666;
      margin-top: 0.5rem;
    }
  </style>
  <script>
    window.addEventListener('load', () => {
      function updateTime() {
        const now = new Date();
        const time = now.toLocaleTimeString();
        document.getElementById('datetime').textContent = `Time: ${time}`;
      }
      updateTime();
      setInterval(updateTime, 1000);
    });
  </script>
</head>
<body>
  <div class='temperature-card'>
    <div class='temperature-value'>
      %TEMPERATUREC%<span class='temperature-unit'>&deg;C</span>
    </div>
    <div class="date-time" id="datetime"></div>
  </div>
</body>
</html>
)rawliteral";

// Replaces placeholder with DS18B20 values
String processor(const String& var){
  if(var == "TEMPERATUREC"){
    return temperatureC;
  }
  return String();
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
    storeWiFi(ssid, password);
  
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

// To connect to wifi if credentials are loaded from EEPROM
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
void connectWiFi() {
  // Get stored in EEPROM
  preferences.begin("credentials", false);
  ssid = preferences.getString("ssid", ""); 
  password = preferences.getString("password", "");
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
    playBuzzer();
    storeAP(false);
  } else {  
      Serial.print("Connect failed with code: ");
      Serial.println(WiFi.status());
      storeAP(true);
  }
}

// Store WiFi credentials in EEPROM
void storeWiFi(String s, String p) {
  preferences.begin("credentials", false);
  preferences.putString("ssid", s); 
  preferences.putString("password", p);
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

void setup(){
  // Serial port for debugging purposes
  Serial.begin(9600);
  delay(100);
  Serial.println("");
  Serial.println("Startup");

  // Start up the DS18B20 library
  sensors.begin();

  // Mode is both access point and client
  if(checkAP()) {
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();
  delay(100);

  // Access point
  Serial.println("\n[*] Creating AP");
  WiFi.softAP(open_ssid, open_password);
  Serial.print("[+] AP Created with IP Gateway ");
  Serial.println(WiFi.softAPIP());
  Serial.println();
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
  }

  // Client
  connectWiFi();
  // Give some time to connect
  delay(500);
  // Get the sensor going
  temperatureC = readDSTemperatureC();

  // Routing of web pages. ON_AP_FILTER is for access point
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (ON_AP_FILTER(request)) {   
        request->send(200, "text/html", "<p>ESP32 IP Address: " + 
          WiFi.localIP().toString() + "</p>" + 
          scanAvailableNetworks());
    } else
    if (ON_STA_FILTER(request)) {
      // Probe needs to "warm up" with two reads
      temperatureC = readDSTemperatureC();
      delay(1);
      temperatureC = readDSTemperatureC();
      request->send_P(200, "text/html", index_html, processor);
      return;
    } 
  });

  server.on("/ws", HTTP_GET, [](AsyncWebServerRequest *request){
    // Probe needs to "warm up" with two reads
    temperatureC = readDSTemperatureC();
    delay(1);
    temperatureC = readDSTemperatureC();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", temperatureC.c_str());
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
    request->send(response);
  });

  server.on("/pick", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (ON_AP_FILTER(request)) {  
      handlePickRequest(request);
    }
  });

  server.on("/connect", HTTP_GET, [](AsyncWebServerRequest *request) {
     if (ON_AP_FILTER(request)) {  
      handleConnectRequest(request);
    }
  });

  // Start server
  server.begin();
}

// To play sound when WiFi is connected
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

// Read the temperature every 5 seconds
// This value is returned by the web pages
void loop(){
}

