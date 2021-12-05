#include <Arduino.h>

#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <analogWrite.h>
#include <Ticker.h>

#include <iostream>
#include <cstring>
#include <string>

#include "index.h"

//  Buttons
#define kettleSwitch 23
#define mugSensor 2

//  Thermistor
#define knownResistor 220
#define thermistor 34

//  Thermistor unsure values
#define THERMISTORNOMINAL 10000
#define BCOEFFICIENT 3950

//  RGB LED
#define red 25
#define green 0
#define blue 14

//  Relay
#define relay 17

//  Forward Declare Functions
String wifiScan();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void handleRoot();
void handleRGB(byte r, byte g, byte b);
void sampleCollector();
void therminstorCalculator();
void kettleRange();
String splitChar();
void doTheThing(char* property, uint8_t num);
void doTheThingWith(char* property, char* param, uint8_t num);
void payloadConvert(char* payload, uint8_t num);

//  Declare WebServers
WebServer server(80);
WebSocketsServer webSocket(81);

//  Global Variables
String networksDetected;
const char* softAPName = "Kettle";

boolean mugDetect = false;
boolean kettleState = false;

//  Counter for Thermistor calculation
byte counter = 0;
const byte maxCounter = 4;
unsigned int samples[maxCounter];

float thermistorAverage;
float temprature;

unsigned int kettleRangeArray[60];
byte countKettle = 0;
const byte kettleRangeMax = 60;

//  Tickers
Ticker thermistorSampleCollectorTicker;
Ticker thermistorConverterTicker;


void setup() 
{
  //  Serial for Debuging
  Serial.begin(115200);

  //  Pin Setup
  pinMode(kettleSwitch, PULLDOWN);
  pinMode(mugSensor, PULLDOWN);

  pinMode(thermistor, INPUT);

  pinMode(relay, OUTPUT);

  pinMode(red, OUTPUT);
  pinMode(green, OUTPUT);
  pinMode(blue, OUTPUT);

  //  Scan for avaiable WiFi Networks
  networksDetected = wifiScan();

  //  Start of the Soft Access Point
  Serial.println(WiFi.softAP(softAPName) ? "Ready" : "Failed!");

  //  mDNS Setup "https://Kettle.local/"
  if (!MDNS.begin("kettle")) Serial.println("Error setting up MDNS responder!");
  else Serial.println("kettle.local");

  //  Add service to MDNS-SD
  MDNS.addService("https", "tcp", 80);

  server.on("/", handleRoot);

  //  Starts WebServer and WebSocket
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() 
{
  //  Handles Websocket
  webSocket.loop();

  // Handles WebServer
  server.handleClient();

  //  IF kettleButton HIGH -> kettleState = true;
  //  IF mugButton HIGH -> mugSensor = true;

  if(digitalRead(kettleSwitch))
  {
    Serial.print("Kettle Button True");
  }



  //  Serial.println(digitalRead(kettleSwitch));
  //  Serial.println(digitalRead(mugSensor)); 

}

/**
 * @brief 
 * 
 * Scans for WiFi Networks
 * 
 * @return String 
 */

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

void handleRoot()
{
  String s = MAIN_page;
  server.send(200, "text/html", s);
}

void handleRGB(byte r, byte g, byte b)
{
  analogWrite(red, r);
  analogWrite(green, g);
  analogWrite(blue, b);
}

void kettleRange()
{
  thermistorSampleCollectorTicker.attach_ms(10, sampleCollector);
}

void sampleCollector()
{
  if(counter < maxCounter)
  {
    samples[counter] = analogRead(thermistor);
    counter++;
  }
  else
  {
    therminstorCalculator();
    memset(samples, 0, sizeof(samples));
    counter = 0;
  }
}

void therminstorCalculator()
{
  thermistorAverage = 0;
  temprature = 0;

  for(int i = 0; i < maxCounter; i++)
  {
    thermistorAverage += samples[i];
  } 

  thermistorAverage /= maxCounter;

  thermistorAverage = 1023 / thermistorAverage - 1;
  thermistorAverage = knownResistor / thermistorAverage;

  temprature = log(thermistorAverage / THERMISTORNOMINAL);  //ln(R/Ro)
  temprature /= BCOEFFICIENT;
  temprature += 1.0 / (THERMISTORNOMINAL + 273.15);
  temprature = 1.0 / temprature;
  temprature -= 273.15;

  webSocket.broadcastTXT("TEMPRATURE," + String(temprature));

  //  Store it somewhere
  if(countKettle >= kettleRangeMax) 
  {
    thermistorSampleCollectorTicker.detach();
    countKettle = 0;
  }
  else
  {
    kettleRangeArray[countKettle] = temprature;
    countKettle++;
  }
}

void payloadConvert(char* payload, uint8_t num){
  char* p = strchr(payload, ' ');
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
  if(strcmp(property, "MUG") == 0){
    webSocket.sendTXT(num, "MUG," + String(mugDetect));
  }
  else if(strcmp(property, "WIFI") == 0){
    webSocket.sendTXT(num, "NETWORKS," + networksDetected);
  }
  else if(strcmp(property, "KETTLE") == 0){
    kettleState = !kettleState;
    webSocket.sendTXT(num, "KETTLE," + String(kettleState));
  }
  else if(strcmp(property, "THERMISTOR") == 0){
    kettleRange();
  }
  else{
    Serial.printf("%s \n", property); 
  }
}

void doTheThingWith(char* property, char* param, uint8_t num)
{
  if(strcmp(property, "LED") == 0){
    if(strcmp(param, "RANGE") == 0){
      handleRGB(0,0,0);
      webSocket.sendTXT(num, "LED,RANGE");
    }
    else if(strcmp(param, "RED") == 0){
      handleRGB(255,0,0);
      webSocket.sendTXT(num, "LED,BLUE");
    }
    else if(strcmp(param, "GREEN") == 0){
      handleRGB(0,255,0);
      webSocket.sendTXT(num, "LED,VIOLET");
    }
    else if(strcmp(param, "BLUE") == 0){
      handleRGB(0,0,255);
      webSocket.sendTXT(num, "LED,ORANGE");
    }
    else if(strcmp(param, "OFF") == 0){
      handleRGB(0,0,0);
      webSocket.sendTXT(num, "LED,OFF");
    }
    else{
      Serial.println(param);
    }
  }
  else{
    Serial.printf("%s | %s \n", property, param); 
  }
}