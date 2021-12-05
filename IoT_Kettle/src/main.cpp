#include <Arduino.h>
#include "Ticker.h"
#include <analogWrite.h>

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

//  Misc
void onStartTimer();
float getTemperaure();
void rgbHandle(byte red, byte green, byte blue);

//  used when I don't know what the function will be called
//void something();

enum KettleState  {
  IDLE,
  PRE_INIT,
  POST_INIT,
  HEATING,
  POST_HEAT,
  ERROR
};

void (*STATE_HANDLERS[])() = {
  &idleHandle,
  &preInitHandle,
  &postInitHandle,
  &heatingHandle,
  &postHeatingHandle,
  &errorHandle
};

KettleState state = IDLE;

Ticker startTimer(onStartTimer, 2000, 1);

unsigned long heatingTime;
unsigned long coolingTime;

String errorMessage = "";

float kettleTargetTemprature = 70.0;

void setup() {

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

//Something for to use when I have yet to make a function
//void something(){}