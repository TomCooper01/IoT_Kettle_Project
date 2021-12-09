#include <Arduino.h>

#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <analogWrite.h>
#include <Ticker.h>
#include <Preferences.h>

#include <iostream>
#include <cstring>
#include <string>

#include "index.h"

//  Declare WebServers
WebServer server(80);
WebSocketsServer webSocket(81);

//  Global Variables
String networksDetected;
const char* softAPName = "Kettle";

Preferences flashStorage;

String WiFiErrorMessage = "";

void WiFiBoot();
void WiFiSetupHandle();
void WiFiCredentialCheck();
void WiFiConnectedHandle();
void WiFiErrorHandle();
void WiFiHold();

void setupPage();
void homePage();
void debugPage();

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void wifiCredentials(char* property, char* param);
String wifiScan();

void payloadConvert(char* payload, uint8_t num);
void doTheThing(char* property, uint8_t num);
void doTheThingWith(char* property, char* param, uint8_t num);

enum WiFiSate{
  IDLE,
  SETUP,
  CONNECTING,
  CONNECTED,
  WiFiERROR,
  HOLD
};

void (*WIFI_STATE_HANDLERS[])() = {
  &WiFiBoot,
  &WiFiSetupHandle,
  &WiFiCredentialCheck,
  &WiFiConnectedHandle,
  &WiFiErrorHandle,
  &WiFiHold
};

WiFiSate KettleWiFi = IDLE;

void setup() {

  Serial.begin(115200);

  //  mDNS Setup "https://Kettle.local/"
  if (!MDNS.begin("kettle")) Serial.println("Error setting up MDNS responder!");
  else Serial.println("kettle.local");

  //  Add service to MDNS-SD
  MDNS.addService("https", "tcp", 80);

  //  Starts WebServer and WebSocket
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  //  Handles WiFi State
  WIFI_STATE_HANDLERS[KettleWiFi]();
  
  //  Handles Websocket
  webSocket.loop();

  // Handles WebServer
  server.handleClient();
}

void WiFiBoot() {

  Serial.print("3");
  flashStorage.begin("credentials", false);

  if(flashStorage.getString("SSID", "") == "") KettleWiFi = SETUP;
  else  KettleWiFi = CONNECTING;

  Serial.println(KettleWiFi);

  flashStorage.end();
  Serial.print("4");
}

void WiFiSetupHandle()
{
  //  Scan for avaiable WiFi Networks
  networksDetected = wifiScan();

  //  Start of the Soft Access Point
  Serial.println(WiFi.softAP(softAPName) ? "Ready" : "Failed!");

  //  WiFi Setup Page
  server.on("/", setupPage);
}

void WiFiCredentialCheck()
{
  flashStorage.begin("credentials", false);
  String SSID = flashStorage.getString("SSID", ""); 
  String PASSWORD = flashStorage.getString("PASSWORD", "");
  flashStorage.end();

  WiFi.mode(WIFI_STA);

  if(WiFi.begin(SSID.c_str(), PASSWORD.c_str())) KettleWiFi = CONNECTED;
  else
  {
    WiFiErrorMessage = "Credentials not found!";
    KettleWiFi = WiFiERROR;
  }
}

void WiFiConnectedHandle()
{
  server.on("/", homePage);
  KettleWiFi = HOLD;
}

void WiFiErrorHandle()
{
  Serial.println(WiFiErrorMessage);
  WiFiErrorMessage = "";
  KettleWiFi = SETUP;
}

String wifiScan() {

  WiFi.mode(WIFI_STA);

  int wifiSize = WiFi.scanNetworks();

  WiFi.disconnect();

  String WiFiNetworks = "";

  if (wifiSize == 0) return strdup("");

  for (int i = 0; i < wifiSize; i++) {
    if(i == wifiSize-1) WiFiNetworks += WiFi.SSID(i);
    else WiFiNetworks += WiFi.SSID(i) + ",";
  }

  return WiFiNetworks;
}

void WiFiHold()
{
  // Does nothing
}

void wifiCredentials(char* property, char* param)
{
  flashStorage.begin("credentials", false);
  flashStorage.putString(property, param);
  flashStorage.end();
}

void setupPage()
{
  server.send(200, "text/html", WIFISETUP);
}

void homePage()
{
  server.send(200, "text/html", MAIN);
}

void debugPage()
{
  server.send(200, "text/html", DEBUG);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED: {
        Serial.printf("[%u] Disconnected!\n", num);
        break;
      }
    case WStype_CONNECTED: {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

        // Send message to client
        webSocket.sendTXT(num, "Connected");

        break;
      }
    case WStype_TEXT: {
        //  Convert payload to Char array
        payloadConvert((char*)payload, num);
        break;
      }
    case WStype_ERROR:
    case WStype_BIN:
    case WStype_PING:
    case WStype_PONG:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
    break;
  }
}

void payloadConvert(char* payload, uint8_t num){
  char* p = strchr(payload, ',');
  if(p) {
    *p='\0';
    p++;
    doTheThingWith(payload, p, num);
  }
  else
  {
    doTheThing(payload, num);
  }
}

void doTheThing(char* property, uint8_t num)
{
  if(strcmp(property, "WIFI") == 0){
    webSocket.sendTXT(num, "NETWORKS," + networksDetected);
  }
  else if(strcmp(property, "RESET") == 0){
    flashStorage.clear();
    wifiCredentials("SSID", NULL);
    wifiCredentials("PASSWORD", NULL);
    ESP.restart();
  }
}

void doTheThingWith(char* property, char* param, uint8_t num)
{
  if(strcmp(property, "AccessPointName") == 0){
    wifiCredentials("SSID", param);
    webSocket.sendTXT(num, "Access Point Name Saved");
  }
  else if(strcmp(property, "AccessPointPassword") == 0){
    wifiCredentials("PASSWORD", param);
    webSocket.sendTXT(num, "Access Point Password Saved");
    ESP.restart();
  }
  else{
    Serial.printf("Property: %s | Param: %s \n", property, param);
  }
}
