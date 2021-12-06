#include <Arduino.h>
#include <Ticker.h>
#include <analogWrite.h>

#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Ticker.h>
#include <Preferences.h>

#include <iostream>
#include <cstring>
#include <string>


#include "index.h"

#define KETTLESWITCH 1
#define MUGSWITCH 2
#define WATERSWITCH 3

#define REDPIN 4
#define BLUEPIN 5
#define GREENPIN 6

#define relay 7

//  Tempreature Readings
#define THERMISITORPIN            8
#define SERIEREISITOR         10000
#define THERMISTORNOMINAL      1100      
#define BCOEFFICIENT           3950
#define TEMPERATURENOMINAL       25   
#define MAX_VALUE              4096 //ESP32
#define r                         0.05f

//  Error Handling
#define MAXHEATINGTIME         5000.0f

//  Kettle cooldown
#define COOLDOWNTIME          100000.0f

/**
 * Forward Decleartion
 */

void onStartPressISR();

//  State Handling
void idleHandle();
void heatingHandle();
void preInitHandle();
void postInitHandle();
void postHeatingHandle();

//  Error Handling
void errorHandle();
void errorState(String errorMessage);
void errorMug();
void errorWater();
void errorHeating();

//  Misc
void onStartTimer();
float getTemperaure();
void rgbHandle(byte red, byte green, byte blue);

//  WiFi Functions
void startHandle();
void setupHandle();
void connectionHandle();
void connectedHandle();
void wifiErrorHandle();

//  Server Handling
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);

//  Server Sending
void payloadHandling(char* property, char* param, uint8_t num);

//  Helper Functionality
String wifiScan();
void payloadConvert(char* payload, uint8_t num);

//  Webpage Handling
void wifiSetupPage();
void mainPage();
void debugPage();

//  Webserver Declaration
WebServer server(80);
WebSocketsServer webSocket(81);

//  Global Variables
String networksDetected;
const char* softAPName = "Kettle";

Preferences flashStorage;

//  used when I don't know what the function will be called
//  void something();

enum KettleState  {
  IDLE,
  PRE_INIT,
  POST_INIT,
  HEATING,
  POST_HEAT,
  ERROR,
};

enum KettleWiFiState  {
  STARTUP,
  SETUP,
  CONNECTION_TEST,
  CONNECTED,
  WIFIERROR
};

void (*STATE_HANDLERS[])() = {
  &idleHandle,
  &preInitHandle,
  &postInitHandle,
  &heatingHandle,
  &postHeatingHandle,
  &errorHandle,
};

void (*WIFI_STATE_HANDLERS[])() = {
  &startHandle,
  &setupHandle,
  &connectionHandle,
  &connectedHandle,
  &wifiErrorHandle
};

KettleState state = IDLE;
KettleWiFiState wifiState = STARTUP;

Ticker startTimer(onStartTimer, 2000, 1);

unsigned long heatingTime;
unsigned long coolingTime;

String errorMessage = "";

float kettleTargetTemprature = 70.0;

void setup() {

  Serial.begin(115200);

  pinMode(KETTLESWITCH, INPUT_PULLUP);  //  Kettle on/off Button
  pinMode(MUGSWITCH, INPUT_PULLUP);     //  Mug Switch on/off Button
  pinMode(WATERSWITCH, INPUT_PULLUP);   //  Water on/off

  //  RGB LED
  pinMode(REDPIN, OUTPUT);
  pinMode(GREENPIN, OUTPUT);
  pinMode(BLUEPIN, OUTPUT);

  //  Relay Switch
  pinMode(relay, OUTPUT);

  attachInterrupt(KETTLESWITCH, onStartPressISR, RISING);

}

void loop() {
  STATE_HANDLERS[state]();
  WIFI_STATE_HANDLERS[wifiState]();

  //  Handles Websocket
  webSocket.loop();

  // Handles WebServer
  server.handleClient();

}

void idleHandle(){
  startTimer.update();
  rgbHandle(0,255,255);
}

void preInitHandle(){
  startTimer.start();
  state = IDLE;
}

void onStartPressISR(){
  state = PRE_INIT;
}

void onStartTimer(){
  state = POST_INIT;
}

void postInitHandle(){
   if(!digitalRead(MUGSWITCH)){
    errorState("No Mug Present");
    return;
  }

  if(!digitalRead(WATERSWITCH)){
    errorState("No water in the kettle");
    return;
  }

  state = HEATING;
  rgbHandle(0,255,0);
  attachInterrupt(MUGSWITCH, errorMug, FALLING);
  attachInterrupt(WATERSWITCH, errorWater, FALLING);
  heatingTime = millis();
  digitalWrite(relay, HIGH);
}

void heatingHandle(){
  if(getTemperaure() >= kettleTargetTemprature){
    state = POST_HEAT;
    digitalWrite(relay, LOW);       //  Turn off the kettle
    detachInterrupt(KETTLESWITCH);  //  Turn off the switch controling the switch
    rgbHandle(255,128,0);
    coolingTime = millis();
    return;
  }
  else if(millis() - heatingTime > MAXHEATINGTIME){
    state = ERROR;
    errorMessage = "Heating too long somethings wrong!";
    return;
  }
  else{
    state = HEATING;
    digitalWrite(relay, HIGH);
    return;
  }
}

void postHeatingHandle(){
  if(millis() - coolingTime > COOLDOWNTIME){
    state = IDLE;
    attachInterrupt(KETTLESWITCH, onStartPressISR, RISING);
  }
}

void errorHandle(){
  Serial.println(errorMessage);
  digitalWrite(relay, LOW);
  rgbHandle(255,0,0);
  state = IDLE;
}

void errorMug(){
  state = ERROR;
  errorMessage = "Mug Moved!";
}

void errorWater(){
  state = ERROR;
  errorMessage =  "No water in system!";
}

float getTemperaure() {
  int rawReading = analogRead(THERMISITORPIN);
  static float smoothedReading = rawReading;
  smoothedReading = rawReading*r + smoothedReading*(1.0f-r);
  float partial = log(SERIEREISITOR / (((MAX_VALUE / smoothedReading)-1) * THERMISTORNOMINAL));
  return 1.0 / (partial / BCOEFFICIENT + 1.0 / (TEMPERATURENOMINAL + 273.15)) - 273.15;
}

/**
 * @brief Handles the RGB light in the top of the kettle
 * analogWrite is 0 to 255
 * @param redValue 
 * @param greenValue 
 * @param blueValue 
 */
void rgbHandle(byte redValue, byte greenValue, byte blueValue){
  analogWrite(REDPIN, redValue, redValue);
  analogWrite(GREENPIN, greenValue, greenValue);
  analogWrite(BLUEPIN, blueValue, blueValue);
}

void startHandle()
{
  flashStorage.begin("credentials", false);
  String ssid = flashStorage.getString("SSID", ""); 
  flashStorage.end();

  if(ssid == "") wifiState = SETUP;
  else wifiState = CONNECTION_TEST;

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

void setupHandle()
{
  //  Scan for avaiable WiFi Networks
  networksDetected = wifiScan();

  if((WiFi.softAP(softAPName))) server.on("/", wifiSetupPage);
  else wifiState = WIFIERROR;
}

void connectionHandle(){
  flashStorage.begin("credentials", false);
  if(WiFi.begin((flashStorage.getString("SSID", "")).c_str(), (flashStorage.getString("PASSWORD", "")).c_str())) wifiState = CONNECTED;
  else wifiState = WIFIERROR;
  flashStorage.end();
}

void connectedHandle(){
  server.on("/", mainPage);
}

void wifiErrorHandle(){
  if((WiFi.softAP(softAPName))) server.on("/", debugPage);
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
    payloadHandling(payload, p, num);
  }
  else
  {
    payloadHandling(payload, NULL, num);
  }
}

void payloadHandling(char* property, char* param, uint8_t num){
  if(strcmp(property, "WIFI") == 0){}
  else if(strcmp(property, "RESET") == 0){}
  else if(strcmp(property, "AccessPointName") == 0){}
  else if(strcmp(property, "AccessPointPassword") == 0){}
  else{}
}

void wifiCredentials(char* property, char* param)
{
  flashStorage.begin("credentials", false);
  flashStorage.putString(property, param);
  flashStorage.end();
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

void wifiSetupPage()
{
  server.send(200, "text/html", WIFI_SETUP);
}

void mainPage()
{
  server.send(200, "text/html", MAIN);
}

void debugPage()
{
  server.send(200, "text/html", DEBUG);
}

//Something for to use when I have yet to make a function
//void something(){}