//#include <driver/ledc.h>
#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>

#define LED_PIN             12
// use 5000 Hz as a LEDC base frequency
#define LEDC_BASE_FREQ      5000
#define LEDC_CHANNEL_0      0
#define LEDC_TIMER_12_BIT   12
#define LED_BUILTIN         2
#define BUTTON_INPUT        27
#define BUTTON_OUTPUT       14
// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
int jam_flag = 0;

// SSID & Password
const char* ssid = "433jammer";  // Enter your SSID here
const char* password = "433jammer";  //Enter your Password here

// turn on html page
const char * turn_on_html PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<title>  433Mhz Jam Control Panel </title>
<style>
.button {
 border: none;
 color: white;
 padding: 15px 32px;
 text-align: center;
 display: inline-block;
 font-size: 58px;
 margin: 4px 2px;
 border-radius: 15px;
 font-family: sans-serif;
 cursor: pointer;
}
.duty_cycle {
 border: none;
 color: black;
 text-align: left;
 font-size: 30px;
 font-family: sans-serif;
}
.center {
 position:absolute;
 top:50%;
 left:50%;
 width :250px;
 height:200px;
 margin-left:-140px;
 margin-top:-100px;
}

.button_green {background-color: green;}
</style>
</head>
<body>
<div class="center">
<form action="turn_off" method="get">
<div class="duty_cycle">
Frequency in Hz:
<select class="duty_cycle" name="dutycycle" required>
<option value="10000">10000</option>
<option value="11000">11000</option>
<option value="12000">12000</option>
<option value="13000">13000</option>
<option value="14000">14000</option>
<option value="15000">15000</option>
<option value="16000">16000</option>
<option value="17000">17000</option>
<option value="18000">18000</option>
<option value="19000">19000</option>
<option value="20000">20000</option>
</select>
<input class="button button_green" type="submit" value="Start jamming">
</div>
</form>
</body>
</html>)rawliteral";


// turn off html page
const char * turn_off_html PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<title>  433Mhz Jam Control Panel </title>
<style>
.button {
  border: none;
  color: white;
  padding: 15px 32px;
  text-align: center;
  display: inline-block;
  font-size: 58px;
  margin: 4px 2px;
  border-radius: 15px;
  font-family: sans-serif;
  cursor: pointer;
}

.center {
  position:absolute;
  top:50%;
  left:50%;
  width :200px;
  height:200px;
  margin-left:-140px;
  margin-top:-100px;
}

.button_red {background-color: red;}
</style>
</head>
<div class="center">
 <a href="turn_on">
    <button class="button button_red">
    Stop jamming
    </button>
  </a>
</div>
</body>
</html>)rawliteral";


int lastButtonState = LOW;  // the previous reading from the input pin
int ledState = HIGH;        // the current state of the output pin
int buttonState;            // the current reading from the input pin
int dutycycle = 10000;
const char* PARAM_INPUT_1 = "dutycycle";
// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers


// IP Address details
IPAddress local_ip(192, 168, 2, 1);
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);
// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

void toggle_jamming(int arg)
{
  if(arg == 1)
  {
      //ledcWrite(LEDC_CHANNEL_0, dutycycle);
      tone(LED_PIN, dutycycle); // send square wave on pin
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.print("[+] Started jamming with Frequency: ");
      Serial.print(dutycycle);
  }
  else
  {
     ledcWrite(LEDC_CHANNEL_0, 0);
      //tone(LED_PIN, 0);   // send nothing to pin
      digitalWrite(LED_BUILTIN, LOW);
      Serial.println("[+] Stopped jamming");
  }

}
void setup()
{ 
  Serial.begin(115200);
  delay(2000);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_OUTPUT, OUTPUT);
  pinMode(BUTTON_INPUT, INPUT_PULLUP);
  digitalWrite(BUTTON_OUTPUT, LOW);
    // Setup timer and attach timer to a led pin
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttachPin(LED_PIN, LEDC_CHANNEL_0);
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.println("Configuring access point...");

  if (!WiFi.softAP(ssid, password, 6, 1, 2, false)) {
    log_e("Soft AP creation failed.");
    while(1);
  }
  WiFi.softAPConfig(local_ip, gateway, subnet);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("[*] SSID: ");
  Serial.println(ssid);
  Serial.print("[*] password: ");
  Serial.println(password);

  Serial.println("Server started");
  // Routes for web pages
  // root "/"
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send_P(200, "text/html", turn_on_html);
  });

  // turn on page "/turn_on"
  server.on("/turn_on", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    jam_flag = 0;

    toggle_jamming(jam_flag);
    request->send_P(200, "text/html", turn_on_html);
  });

  // turn off page "/turn_off"
  server.on("/turn_off", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    jam_flag = 1;
    String inputMessage;
    if (request->hasParam(PARAM_INPUT_1, false))
    {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
     // dutycycle =(0.0002 / ((inputMessage.toInt() * 0.000001 ) * 4096));    // Calculate Duty Cycle from LEDC_BASE_FREQ and Timer Resolution
      dutycycle =(inputMessage.toInt());    // Calculate Duty Cycle from LEDC_BASE_FREQ and Timer Resolution
    }

    toggle_jamming(jam_flag);
    request->send_P(200, "text/html", turn_off_html);
   
  });

  // Start server
  server.begin();
  Serial.println("[+] Server started");

}


void loop() 
{
    int reading = digitalRead(BUTTON_INPUT);

    if (reading != lastButtonState) 
    {
        // reset the debouncing timer
        lastDebounceTime = millis();
    }
      if ((millis() - lastDebounceTime) > debounceDelay) 
        {
          if (reading != buttonState) 
            {
              buttonState = reading;
              if (buttonState == HIGH)
              {
                toggle_jamming(jam_flag);
                jam_flag = !jam_flag;
              }
            }
        }
  lastButtonState = reading;
}
