#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <DHT.h>
#include <ArduinoJson.h>

/*
Simple webserver with websocket to constantly refresh the webpage.
The webpage diaplays the temperature and humidity reading, and there
are 2 buttons to either turn on or off the LED
JSON is also used to handle the data sent to the webpage
*/

// Replace with your network credentials
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"

// Pins for the DHT sensor and LED
#define DHTPIN 32
#define led_pin 27

int interval = 1000;  // Interval to send updates
unsigned long previousMillies = 0;

// HTML and JavaScript code for the webpage. HTML and JavaScript code is copied from dht.html and pasted here in an R"()" string
// Code could be improved by reading the file from SPIFFS
String webpage = R"(
<!DOCTYPE html>
<html lang='en'>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>DHT Sensor Data</title>
</head>
<body>
    <h1>Humidity and Temperature data</h1>
    <p>Here is the Humidity data: <span id='humidity'>-</span>%</p>
    <p>Here is the Temperature data: <span id='temperature'>-</span>°C</p>
    <button type='button' id='LED_ON'>Turn LED on</button>
    <button type='button' id='LED_OFF'>Turn LED off</button>

    <script>
        // Create a new WebSocket for real time data and changes
        var Socket;

        // Add event listeners to the buttons, and call the functions to send the on/off signal to the ESP32
        document.getElementById('LED_ON').addEventListener('click', send_signal_on);
        document.getElementById('LED_OFF').addEventListener('click', send_signal_off);

        // Function to send the on/off signal to the ESP32
        function send_signal_on() {
            Socket.send('LED_ON');
            console.log('LED ON');
        }
        function send_signal_off() {
            Socket.send('LED_OFF');
            console.log('LED OFF');
        }

        // Function to initialize the WebSocket
        function init() {
            Socket = new WebSocket('ws://' + window.location.hostname + ':81/');    // Connect to the WebSocket server
            
            Socket.onopen = function() {
                console.log('WebSocket Connected!');
            };

            Socket.onerror = function(error) {
                console.log('WebSocket Error:', error);
            };
            
            Socket.onmessage = function(event) {
                console.log('Received Data:', event.data);
                processCommand(event);
            };
        }
        // Function to process the command received from the WebSocket (in this case, changing the inner html of the span elements to the temp and humidity data received)
        function processCommand(event) {
            // Parse the JSON object from the event and update the values on the HTML page
            var jsonobj = JSON.parse(event.data);
            document.getElementById('humidity').innerHTML = jsonobj.humidity;
            document.getElementById('temperature').innerHTML = jsonobj.temperature;
            console.log(jsonobj.humidity);    // Log the data to the console for debugging
            console.log(jsonobj.temperature);
        }   
        // Call the init function when the page is loaded to start websocket
        window.onload = function(event) {
            init();
        }
    </script>
</body>
</html>

)";

// Initialize the DHT sensor
DHT dht(DHTPIN, DHT11);

// Initialize the WebServer and WebSocket
WebServer server(80);  
WebSocketsServer webSocket = WebSocketsServer(81);  

// Initialize the JSON documents
JsonDocument doc_tx;    // JSON for transmitting data
JsonDocument doc_rx;    // JSON for receiving data

void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length) {
    // num is client number, type is type of event (connect, disconnect, text message), payload is data received (array of unsigned int), length is number of bytes

    Serial.println("WebSocket Event Triggered");

    if (type == WStype_CONNECTED) {
        Serial.println("Client connected");
    }
    else if (type == WStype_DISCONNECTED) {
        Serial.println("Client disconnected");
    }
    else if (type == WStype_TEXT) {
        String command = String((char *)payload);   //Cast payload (8 bit unsigned int) to char pointer, then to String
        Serial.print("Received Command: ");
        Serial.println(command);

        if (command == "LED_ON") {
            digitalWrite(led_pin, HIGH);    // Turn on the LED
            Serial.println("LED ON");
        }
        else if (command == "LED_OFF") {
            digitalWrite(led_pin, LOW);    // Turn off the LED
            Serial.println("LED OFF"); 
        }
    }
}

void setup() {
    // put your setup code here, to run once:

    Serial.begin(115200);
    dht.begin();
    pinMode(led_pin, OUTPUT);

    // Connect to Wi-Fi network with SSID and password
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // Wait for connection
    while(WiFi.status() != WL_CONNECTED){
        delay(1000);
        Serial.println("Connecting to WiFi with SSID: " + String(WIFI_SSID)+ " and Password: " + String(WIFI_PASSWORD));
    }

    // Print local IP address
    Serial.println("Connected to the WiFi network at IP address: ");
    Serial.println(WiFi.localIP());

    // Set up the HTML webpage
    server.on("/", []() { server.send(200, "text/html", webpage); });

    // Start the server and websocket
    server.begin();
    Serial.println("HTTP Server started!");
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.println("WebSocket Server started on port 81!");

}

void loop(){
    // put your main code here, to run repeatedly:

    server.handleClient();
    webSocket.loop();

    // Send data every 'interval' milliseconds
    unsigned long now = millis();
    if (now - previousMillies > interval) {
        String jsonString = "";
        JsonObject object = doc_tx.to<JsonObject>();
        
        // Get temperature and humidity
        float temp = dht.readTemperature();
        float hum = dht.readHumidity();
        Serial.println("Sending Temperature: " + String(temp));
        Serial.println("Sending Humidity: " + String(hum));

        // Add data to the object in JSON format
        object["temperature"] = temp;
        object["humidity"] = hum;

        serializeJson(doc_tx, jsonString);  // Convert JSON to string
        webSocket.broadcastTXT(jsonString); // Send data to clients

        previousMillies = now;
    }
}

