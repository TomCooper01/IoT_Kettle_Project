#include <Arduino.h>

#include <Ticker.h>
#include <analogWrite.h>

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

#include <Ticker.h>
#include <analogWrite.h>

#define KETTLESWITCH 0
#define MUGSWITCH 16
#define WATERSWITCH 3

#define REDPIN 13
#define BLUEPIN 15
#define GREENPIN 14

#define relay 12

//  Tempreature Readings
#define THERMISITORPIN           13
#define SERIEREISITOR         10000
#define THERMISTORNOMINAL      1100      
#define BCOEFFICIENT           3950
#define TEMPERATURENOMINAL       25   
#define MAX_VALUE              4096 //ESP32
#define r                         0.05f

//  Error Handling
#define MAXHEATINGTIME        100000.0f

//  Kettle cooldown
#define COOLDOWNTIME          100000.0f

float kettleTargetTemprature = 40.0;

// Demo define will allow for Serial 
#define DEBUG

/**
 * Forward Decleartion
 */

//  On button press change state
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

//  WiFi Handle
void WiFiSetupHandle();
void WiFiCredentialCheck();
void WiFiErrorHandle();

//  Website Handle
void setupPage();
void homePage();
void debugPage();

//  WiFi Misc
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void wifiCredentials(const char* property, const char* param);
String wifiScan();

//  Websocket Event and Server handle
void payloadConvert(char* payload, uint8_t num);
void doTheThing(char* property, uint8_t num);
void doTheThingWith(char* property, char* param, uint8_t num);

//  Misc
void onStartTimer();
float getTemperaure();
void rgbHandle(byte red, byte green, byte blue);

enum KettleState  {
  IDLE,
  PRE_INIT,
  POST_INIT,
  HEATING,
  POST_HEAT,
  ERROR
};

/*
  TODO: Locking of the kettle and ownership of the control
*/

void (*STATE_HANDLERS[])() = {
  &idleHandle,
  &preInitHandle,
  &postInitHandle,
  &heatingHandle,
  &postHeatingHandle,
  &errorHandle
};

KettleState state = IDLE;

#ifdef DEBUG
  KettleState previousState = state;
#endif

Ticker startTimer;

unsigned long heatingTime;
unsigned long coolingTime;

//  Declare WebServers
WebServer server(80);
WebSocketsServer webSocket(81);

//  Global Variables
String networksDetected;
const char* softAPName = "Kettle";

Preferences flashStorage;

String WiFiErrorMessage = "";

String errorMessage = "";

void setup() {
  #ifdef DEBUG
  Serial.begin(115200);
  #endif

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

  flashStorage.begin("credentials", false);
  String SSID = flashStorage.getString("SSID", ""); 
  flashStorage.end();   

  if(SSID == "") WiFiSetupHandle();
  else WiFiCredentialCheck();

  #ifdef DEBUG
    server.on("/debug", debugPage);
  #endif

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
  STATE_HANDLERS[state]();

  //  Handles Websocket
  webSocket.loop();

  // Handles WebServer
  server.handleClient();

  // Handles Errors
  if(!(WiFiErrorMessage == "")) WiFiErrorHandle();
  
  #ifdef DEBUG  
    if(state != previousState)  {
      previousState = state;
      webSocket.broadcastTXT("DEBUG,STATE," + String(state));
    }
  #endif

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

  if(!(WiFi.begin(SSID.c_str(), PASSWORD.c_str())))
  {
    WiFiErrorMessage = "Credentials not found!";
    WiFiSetupHandle();
  }
  else{
    server.on("/", homePage);
  }
}

void WiFiErrorHandle()
{
  Serial.println(WiFiErrorMessage);
  WiFiErrorMessage = "";
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

void wifiCredentials(const char* property, const char* param)
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
  server.send(200, "text/html", DEBUGPAGE);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED: {
        #ifdef DEBUG
        Serial.printf("[%u] Disconnected!\n", num);
        #endif
        break;
      }
    case WStype_CONNECTED: {
        IPAddress ip = webSocket.remoteIP(num);
        #ifdef DEBUG
          Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        #endif

        // Send message to client
        webSocket.sendTXT(num, "Connected");

        #ifdef DEBUG
          webSocket.sendTXT(num, "DEBUG,STATE," + String(state));
        #endif

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
  else if(strcmp(property, "SWITCH") == 0){
    state = PRE_INIT;
    webSocket.broadcastTXT("STATE CHANGED");
  }
  else if(strcmp(property, "RESET") == 0){
    flashStorage.clear();
    wifiCredentials("SSID", "");
    wifiCredentials("PASSWORD", "");
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

void idleHandle(){
  rgbHandle(0,255,255);
}

void preInitHandle(){
  startTimer.attach_ms(2000, onStartTimer);
  state = IDLE;
}

void onStartPressISR(){
  state = PRE_INIT;
}

void onStartTimer(){
  startTimer.detach();
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
    #ifdef DEBUG
      float tempTemprature = getTemperaure();
      Serial.println(tempTemprature);
      webSocket.broadcastTXT("SENSORS,THERMISTOR,"  + String(tempTemprature));
    #endif
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
  rgbHandle(255,0,10);
  errorMessage = "Mug Moved!";
}

void errorWater(){
  state = ERROR;
  rgbHandle(255,10,0);
  errorMessage =  "No water in system!";
}

void errorState(String errorMessage){
    #ifdef DEBUG
    Serial.println(errorMessage);
    #endif

    webSocket.broadcastTXT("ERROR " + errorMessage);
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
  analogWrite(REDPIN, redValue);
  analogWrite(GREENPIN, greenValue);
  analogWrite(BLUEPIN, blueValue);
}

//Something for to use when I have yet to make a function
//void something(){}
