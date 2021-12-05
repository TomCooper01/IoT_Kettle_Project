#include <Arduino.h>

const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en" dir="ltr">

<head>
    <meta charset="utf-8">
    <title>IOT Kettle - Demo</title>

    <script type="text/javascript">

        var connection = new WebSocket('ws://' + location.hostname + ':81/', ['arduino']);
        
        connection.onopen = function () {
            connection.send('Connect ' + new Date());
        };
        connection.onerror = function (error) {
            console.log('WebSocket Error ', error);
        };
        connection.onmessage = function (e) {
            doTheWebsiteThing(e.data);
        };
        connection.onclose = function () {
            console.log('WebSocket connection closed');
        };

        function onOffSwitch()
        {
            connection.send("KETTLE");
        }

        function ledTest(input)
        {
            connection.send("LED " + input);
        }

        function thermistorTest()
        {
            connection.send("THERMISTOR");
        }

        function avaliableWiFiNetworks()
        {
            connection.send("WIFI");
        }

        function mugTest()
        {
            connection.send("MUG")
        }

        function doTheWebsiteThing(e)
        {
            const messageSplit = e.split(",");
            switch(messageSplit[0])
            {
                case "MUG":
                    document.getElementById("MugTestOutput").innerHTML = messageSplit[1];
                    console.log(messageSplit[1]);
                break;
                case "NETWORKS":
                    document.getElementById("WiFiNetworkOutput").innerHTML = messageSplit[1];

                    var wifiSelect = document.createElement("SELECT");
                    wifiSelect.id = "WiFiSelect";

                    for(var i = 1; i < messageSplit.length; i++)
                    {
                        var option = document.createElement("option");
                        option.value = messageSplit[i];
                        if(i == 1) option.selected = "selected";
                        option.innerHTML = messageSplit[i];
                        wifiSelect.appendChild(option);
                    }

                    document.getElementById("WiFiNetwork").appendChild(wifiSelect);

                    var passwordFeild = document.createElement("input");
                    passwordFeild.type = "password";
                    passwordFeild.id = "WiFiPassword";
                    document.getElementById("WiFiNetwork").appendChild(passwordFeild);
                    

                    var btn = document.createElement("BUTTON");
                    btn.innerHTML = "Connect"
                    btn.onclick = function(){
                        connection.send("WiFiNetwork," + document.getElementById("WiFiSelect").value);
                        connection.send("WiFiPassword" + document.getElementById("WiFiPassword").value);
                    }
                    document.getElementById("WiFiNetwork").appendChild(btn);

                    console.log(messageSplit[1]);
                break;
                case "KETTLE":
                    document.getElementById("typeOfButtonPressed").innerHTML = messageSplit[1];
                    console.log(messageSplit[1]);
                break;
                case "LED":
                    document.getElementById("LEDState").innerHTML = messageSplit[1];
                    console.log(messageSplit[1]);
                break;
                case "THERMISTOR": 
                    document.getElementById("thermistorOutput").innerHTML = messageSplit[1];
                    console.log(messageSplit[1]);
                break;
                case "Connected": 
                    console.log("Connected to WebSocket!");
                break;
                default:
                    console.log(messageSplit);
                break;
            }
                
        }
        

    </script>

</head>

<body>
    <center>
        <h1>IoT Kettle Demo</h1>
        <br>
        <div id="kettleSwitch">
            <p>Kettle Control Switch</p>
            <button id = "buttonSwitch" onclick="onOffSwitch()">kettleSwitch</button>
            <p id="typeOfButtonPressed"></p>
        </div>
        <div id = "RGB">
            <p>RGB Test Panel</p>
            <button id="Range" onclick="ledTest('RANGE')">Test Range</button>
            <button id="Red" onclick="ledTest('RED')">BLUE Button</button>
            <button id="Green" onclick="ledTest('GREEN')">Violet Button</button>
            <button id="Blue" onclick="ledTest('BLUE')">Orange Button</button>
            <button id="OFF" onclick="ledTest('OFF')">OFF Button</button>
            <p id = "LEDState"></p>
            <br>
        </div>
        <div id="thermistor_test_panel">
            <p>Thermistor Test Panel</p>
            <button id="thermistorButton" onclick="thermistorTest()">Thermistor Test</button>
            <br>
            <p id="thermistorOutput"></p>
            <br>
        </div>
        <div id="mugOutput">
            <p>Mug Detect Test</p>
            <p id="MugTestOutput"></p>
            <button id="mugButton" onclick="mugTest()">Mug Test</button>
            <br>
        </div>
        <div id="WiFiNetwork">
            <p>Avaliable WiFi Networks</p>
            <button id="mugButton" onclick=avaliableWiFiNetworks()>WIFI Test</button>
            <div id="WiFiNetworkOutput"></div>
            <br>
        </div>
        <br>
    </center>
</body>

</html>
)=====";

const char Setup_Page[] PROGMEM = R"=====(

)=====";