//USE THE 2.0.17 ESP32 by Espressif Ststems Board Library

// Define debugging and configuration settings
#define DEBUG_ETHERNET_WEBSERVER_PORT Serial
#define _ETHERNET_WEBSERVER_LOGLEVEL_ 3
#define UNIQUE_SUBARTNET

// Define configuration constants
#define NB_CHANNEL_PER_LED 3  // Number of channels per LED (3 for RGB, 4 for RGBW)
#define COLOR_GRB // Define color order (Green-Red-Blue)

// Include necessary libraries
#include "Arduino.h"
#include <Preferences.h>
#include <WebServer_WT32_ETH01.h>
//#include <WebServer_ESP32_W5500.h> 
#include <WebServer.h>
#include "I2SClocklessLedDriver.h" // Library to drive LEDs via I2S
// https://github.com/hpwit/I2SClocklessLedDriver
#include "artnetESP32V2.h" // Artnet library for ESP32
// https://github.com/hpwit/artnetesp32v2
#include <Adafruit_NeoPixel.h>

//Timeout settings
unsigned long startAttemptTime = millis();
const unsigned long connectionTimeout = 30000; // 30 seconds timeout


//Server definitions
WebServer server(80);
Preferences preferences;


//Configuration par défault
int numledsoutput = 100;
int numoutput = 4;=
int startuniverse = 0;
int suffix_ipv4 = 100;
String nodename = "KSS led strip";

int pins[4] = {15,12,14,4};
bool ledCycleEnabled = false;



// Instance dut client Artnet
artnetESP32V2 artnet = artnetESP32V2();

// Instance du driver I2S 
I2SClocklessLedDriver driver;

// envoyer les données artnet aux rubans leds 
void displayfunction(void *param) {
    subArtnet *subartnet = (subArtnet *)param;
    driver.showPixels(NO_WAIT, subartnet->data);
}


// Page HTML Interface WEB
void handleRoot() {
  String content = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";  
  content += "<link rel=\"icon\" href=\"data:,\">"; // Empty favicon
  content += "<style>";
  content += "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f4f4f4; color: #333; text-align: center; }";
  content += "h1 { font-size: 24px; margin-bottom: 20px; }";
  content += "form { background-color: #fff; padding: 20px; padding-right: 40px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1); max-width: 400px; margin: auto; }";
  content += "input[type='text'] { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ccc; border-radius: 4px; font-size: 16px; }";
  content += "input[type='submit'] { background-color: #4CAF50; color: white; border: none; padding: 12px; font-size: 16px; cursor: pointer; border-radius: 4px; width: 100%; }";
  content += "input[type='submit']:hover { background-color: #45a049; }";
  content += ".warning { margin-top: 20px; color: #d9534f; font-size: 14px; }"; // Warning text style
  content += "</style></head>";
  content += "<body><h1>" + nodename + " Configuration</h1>";
  content += "<form method='POST' action='/config'>";
  content += "<label for='numledsoutput'>Nombre de leds par bandeaux (Max. 680):</label>";
  content += "<input type='text' id='numledsoutput' name='numledsoutput' value='" + String(numledsoutput) + "'><br>";
  content += "<label for='numoutput'>Nombre de bandeaux (Max. 4):</label>";
  content += "<input type='text' id='numoutput' name='numoutput' value='" + String(numoutput) + "'><br>";
  content += "<label for='startuniverse'>Start universe:</label>";
  content += "<input type='text' id='startuniverse' name='startuniverse' value='" + String(startuniverse) + "'><br>";
  content += "<label for='nodename'>Artnet Node name:</label>";
  content += "<input type='text' id='nodename' name='nodename' value='" + nodename + "'><br>";
  content += "<label for='suffix_ipv4'>suffix ipv4 :</label>";
  content += "192.168.1.<input type='text' id='suffix_ipv4' name='suffix_ipv4' value='" + String(suffix_ipv4) + "'><br>";
  content += "<label for='ledCycleEnabled'>Enable RGB Test Cycle:</label>";
  content += "<input type='checkbox' id='ledCycleEnabled' name='ledCycleEnabled' " + String(ledCycleEnabled ? "checked" : "") + "><br>";
  content += "<input type='submit' value='Save'>";
  content += "</form>";
  content += "</body></html>";
  
  server.send(200, "text/html", content); // Send HTML response
}

// Fonction appelé par le serveur WEB losqu'une nouvelle conbfiguration est entrée par l'utilisateur
void handleConfig() {

  if (server.method() == HTTP_POST) {

    // récuperer les arguments envoyé dans la requette
      if (server.hasArg("numledsoutput")) numledsoutput = server.arg("numledsoutput").toInt();
      if (server.hasArg("numoutput")) numoutput = server.arg("numoutput").toInt();
      if (server.hasArg("startuniverse")) startuniverse = server.arg("startuniverse").toInt();
      if (server.hasArg("nodename")) nodename = server.arg("nodename");
      if (server.hasArg("suffix_ipv4")) suffix_ipv4 = server.arg("suffix_ipv4").toInt();
      ledCycleEnabled = server.hasArg("ledCycleEnabled"); // Process new checkbox value

      // Stocker les paramètres en ROM
      preferences.begin("esp32config", false);
        preferences.putInt("numledsoutput", numledsoutput);
        preferences.putInt("numoutput", numoutput);
        preferences.putInt("startuniverse", startuniverse);
        if(suffix_ipv4 <256 && suffix_ipv4 >= 0)
          preferences.putInt("suffix_ipv4", suffix_ipv4);
        preferences.putString("nodename", nodename);
      preferences.end();

      server.sendHeader("Location", "/", true);
      server.send(302, "text/plain", "");

      // Reset l'ESP pour appliquer les paramètres
      if (!ledCycleEnabled){
        ESP.restart();
      }
  }
}




void setupEthernet() {
  Serial.println("Initializing Ethernet...");
  WT32_ETH01_onEvent(); // 
  ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER);

  
  // Set your Static IP address
  IPAddress local_IP(192, 168, 1, suffix_ipv4);
  // Set your Gateway IP address
  IPAddress gateway(192, 168, 1, 1);

  IPAddress subnet(255, 255, 255, 0);
  IPAddress primaryDNS(8, 8, 8, 8);   //optional
  IPAddress secondaryDNS(8, 8, 4, 4); //optional

  ETH.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);

  while (millis() - startAttemptTime < connectionTimeout) {
      int remainingTime = (connectionTimeout - (millis() - startAttemptTime)) / 1000;

      // Check if we have a valid IP
      if (ETH.localIP() != INADDR_NONE) {
          Serial.println("Ethernet connected successfully.");
          Serial.print("IP Address: ");
          Serial.println(ETH.localIP());

          // Set up web server routes
          server.on("/", HTTP_GET, handleRoot);
          server.on("/config", HTTP_POST, handleConfig);
          server.begin();
          Serial.println("HTTP server started (Ethernet).");

          return;
      }

      Serial.print("Waiting for Ethernet connection... ");
      Serial.print(remainingTime);
      Serial.println("s remaining");

      delay(1000);
  }
}





void setup() {
    Serial.begin(115200);

    Serial.print("Booting KSS strip led\n");

    // Récupérer les paramètres strockés en ROM
    preferences.begin("esp32config", true);
    numledsoutput = preferences.getInt("numledsoutput", numledsoutput);
    numoutput = preferences.getInt("numoutput", numoutput);
    startuniverse = preferences.getInt("startuniverse", startuniverse);
    nodename = preferences.getString("nodename", nodename);
    suffix_ipv4 = preferences.getInt("suffix_ipv4", suffix_ipv4);
    preferences.end();

    Serial.print(numoutput);
    Serial.print(" strips de ");
    Serial.print(numledsoutput);
    Serial.println(" leds");
    Serial.print(numledsoutput * numoutput * NB_CHANNEL_PER_LED / (numledsoutput * NB_CHANNEL_PER_LED));
    Serial.println(" univers");
    

    // Démarre la connection ethernet et le serveur web
    setupEthernet();

    // Initialiser le driver I2S
    driver.initled(NULL, pins, numoutput, numledsoutput);
    driver.setBrightness(100);

    // Démarrer le serveur Artnet
    artnet.addSubArtnet(startuniverse, numledsoutput * numoutput * NB_CHANNEL_PER_LED, numledsoutput * NB_CHANNEL_PER_LED, &displayfunction);
    artnet.setNodeName(nodename);


    IPAddress activeIP = ETH.localIP();
    if (artnet.listen(activeIP, 6454)) {
        Serial.print("Artnet Listening on IP: ");
        Serial.println(activeIP);
    }
  
}

void loop() {
  server.handleClient();// fonction a executer régulièrement pour gérer le serveur

  // Run the LED cycle if it's enabled
  if (ledCycleEnabled) {
      cycleLEDs(); // Keep cycling the LEDs if enabled
  }
}



// fonction pour tester chaque couleur de chaque leds
void cycleLEDs() {

  // initialisation des instances nécéssaires pour le test des leds
  static uint8_t currentColor = 0;  // 0 = Red, 1 = Green, 2 = Blue ...
  static unsigned long lastUpdate = 0;
  static const unsigned long cycleDelay = 1000;  // Change color every 1 second
  static Adafruit_NeoPixel* strips[4];

  static bool first = true;
  if(first)
  {
    first = false;
    for (int i = 0; i < numoutput; i++) {
      strips[i] = new Adafruit_NeoPixel(numledsoutput, pins[i], NEO_GRB + NEO_KHZ800);
    }
  }

  if (millis() - lastUpdate >= cycleDelay) {
      lastUpdate = millis();

      // Determine color based on cycle
      uint32_t color;
      switch (currentColor) {
          case 0: color = strips[0]->Color(255, 0, 0); break;  // Red
          case 1: color = strips[0]->Color(0, 255, 0); break;  // Green
          case 2: color = strips[0]->Color(0, 0, 255); break;  // Blue
          case 3: color = strips[0]->Color(0, 255, 255); break;  // cyan
          case 4: color = strips[0]->Color(255, 0, 255); break;  // magenta
          case 5: color = strips[0]->Color(255, 255, 0); break;  // yellow
          case 6: color = strips[0]->Color(255, 255, 255); break;  // wiht
      }

      // Apply color to all strips
      for (int i = 0; i < numoutput; i++) {
          for (int j = 0; j < numledsoutput; j++) {
              strips[i]->setPixelColor(j, color);
          }
          strips[i]->show();
      }

      // Move to the next color in cycle
      currentColor = (currentColor + 1) % 7;
  }
}
