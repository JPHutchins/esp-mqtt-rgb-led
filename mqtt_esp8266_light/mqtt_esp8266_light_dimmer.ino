/*
 * ESP8266 MQTT Non-RGB Lights (LED Strip) for Home Assistant.
 * See https://github.com/corbanmailloux/esp-mqtt-rgb-led for more information.
 */

// https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.h>

#include <ESP8266WiFi.h>

// http://pubsubclient.knolleary.net/
#include <PubSubClient.h>

// Set configuration options for LED type, pins, WiFi, and MQTT:
// Pins
const int ledPin = 5; //NodeMCU D1

// WiFi
const char* ssid = "YOUR SSID";
const char* password = "YOUR PASS";

// MQTT
const char* mqtt_server = "YOUR MQTT SERVER";
const char* mqtt_username = "YOUR MQTT USERNAME";
const char* mqtt_password = "YOUR MQTT PASSWORD";
const char* client_id = "SET UNIQUE FOR EACH PROJECT";
const uint16_t mqtt_port = 1883; //1883 is default

// MQTT Topics
const char* state_topic = "home/led1"; //MQTT reported state
const char* set_topic = "home/led1/set"; //MQTT control

// Reverse the LED logic
// false: 0 (off) - 255 (bright)
// true: 255 (off) - 0 (bright)
const bool led_invert =  false;

// Enables Serial and print statements
const bool debug_mode = true; //set false after tested

const char* on_cmd = "ON";
const char* off_cmd = "OFF";

const int BUFFER_SIZE = JSON_OBJECT_SIZE(8);

uint16_t pwm = 1023;
uint16_t brightness = 1023;

uint16_t realPwm = 0;

bool stateOn = false;
float brightScale[101]; //make sure to set brightness_scale: 100 in configuration.yaml - the goal is consistency with voice assistants

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  if (debug_mode) {
    Serial.begin(115200);
  }
 
  //https://diarmuid.ie/blog/pwm-exponential-led-fading-on-arduino-or-other-platforms/
  //Generate a more natural scale for perceieved brightness.  Needs further testing and may vary by LED.
  //It is likely better to generate in linear chunks (separate forloops) to fine tune
  for (int i=1; i<100; i++) {
    brightScale[i] = pow(2, ((float)i/10));
  }
  brightScale[0] = 0;
  brightScale[100] = 1023;
  
  pinMode(ledPin, OUTPUT);
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH); // Turn off the on-board LED

  analogWriteRange(1023);  // ESP8266 10bit PWM
  
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.println(message);

  if (!processJson(message)) {
    return;
  }

  if (stateOn) {
    // Update lights
    realPwm = brightScale[brightness];
  }
  else {
    realPwm = 0;
  }

  setBrightness(realPwm);
  sendState();
}

bool processJson(char* message) {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(message);

  if (!root.success()) {
    Serial.println("parseObject() failed");
    return false;
  }

  if (root.containsKey("state")) {
    if (strcmp(root["state"], on_cmd) == 0) {
      stateOn = true;
    }
    else if (strcmp(root["state"], off_cmd) == 0) {
      stateOn = false;
    }
    
   if (root.containsKey("brightness")) {
      brightness = root["brightness"];
    }
  }

  return true;
}

void sendState() {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  root["state"] = (stateOn) ? on_cmd : off_cmd;

  root["brightness"] = brightness;

  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));

  client.publish(state_topic, buffer, true);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(client_id, mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(set_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setBrightness(int inBrightness) {
  if (led_invert) {
    inBrightness = (1023 - inBrightness);
  }
  
  analogWrite(ledPin, inBrightness);
  
  Serial.println("Setting LEDs:");
  Serial.println(inBrightness);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
