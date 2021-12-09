#include <Arduino.h>

const char WIFISETUP[] PROGMEM = R"=====(
<!DOCTYPE html><html lang="en" dir="ltr"><head><meta charset="utf-8"><title>IOT Kettle - WiFi Setup</title><script type="text/javascript">var connection=new WebSocket('ws://' + location.hostname + ':81/', ['arduino']);connection.onopen=function(){connection.send('Connect ' + new Date());connection.send("WIFI");};connection.onerror=function(error){console.log('WebSocket Error ', error);};connection.onmessage=function(e){formSetup(e.data);};connection.onclose=function(){console.log('WebSocket connection closed');};function formSetup(e){const messageSplit=e.split(",");if (messageSplit[0]=="NETWORKS"){document.getElementById("WiFiNetwork").innerHTML="";var wifiSelect=document.createElement("SELECT");wifiSelect.id="WiFiSelect";for (var i=1; i < messageSplit.length; i++){var option=document.createElement("option");option.value=messageSplit[i];if (i==1) option.selected="selected";option.innerHTML=messageSplit[i];wifiSelect.appendChild(option);}document.getElementById("WiFiNetwork").appendChild(wifiSelect);var break0=document.createElement("BR");document.getElementById("WiFiNetwork").appendChild(break0);var passwordFeild=document.createElement("input");passwordFeild.type="password";passwordFeild.id="WiFiPassword";document.getElementById("WiFiNetwork").appendChild(passwordFeild);var break1=document.createElement("BR");document.getElementById("WiFiNetwork").appendChild(break1);var btn=document.createElement("BUTTON");btn.innerHTML="Connect";btn.onclick=function(){connection.send("AccessPointName," + document.getElementById("WiFiSelect").value);connection.send("AccessPointPassword," + document.getElementById("WiFiPassword").value);};document.getElementById("WiFiNetwork").appendChild(btn); var brn1=document.createElement("BUTTON");brn1.innerHTML="REFRESH";brn1.onclick=function(){connection.send("RESET");};document.getElementById("WiFiNetwork").appendChild(brn1);}else if (messageSplit[0]=="Connected"){console.log("Connection Established");}else{console.log(messageSplit);}}</script></head><body><center><div id="WiFiNetwork"></div></center></body></html>
)=====";

const char MAIN[] PROGMEM = R"=====(
<!DOCTYPE html><html lang="en" dir="ltr"><head> <meta charset="utf-8"> <title>IOT Kettle - Home</title> <script type="text/javascript">var connection=new WebSocket('ws://' + location.hostname + ':81/', ['arduino']); connection.onopen=function (){connection.send('Connect ' + new Date());}; connection.onerror=function (error){console.log('WebSocket Error ', error);}; connection.onmessage=function (e){e.data;}; connection.onclose=function (){console.log('WebSocket connection closed');}; function switch(){connection.send("SWITCH");}</script></head><body> <p>I am on WIFI</p><br><button onclick="switch()">Switch</button> <p id="KettleOutput"></p></body></html>
)=====";

const char DEBUG[] PROGMEM = R"=====(

)=====";