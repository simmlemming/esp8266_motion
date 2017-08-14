#include <ESP8266WiFi.h>
#include <PubSubClient.h>

int greenLedPin = 14;
int redLedPin = 4;
int pirPin = 12; // Input for motion sensor

int pirValue; // Place to store read PIR Value

const char* ssid = "Cloud_2";
const char* ssid_password = "";

const char* mqtt_server = "192.168.0.110";
const char* clientID = "motion01";
const char* outTopic = "motion01";
const char* inTopic = "motion01_cmd";

WiFiClient espClient;
PubSubClient client(espClient);

boolean wifi_connecting = false, wifi_connected = false, wifi_error = false;
boolean mqtt_connecting = false, mqtt_connected = false, mqtt_error = false;

const int STATE_OFF = 0;
const int STATE_OK = 1;
const int STATE_INIT = 2;
const int STATE_ERROR = 3;
const int STATE_ALARM = 4;

int state = STATE_INIT;

void setup() {
  delay(100);
  Serial.begin(115200);
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(onNewMessage);

  pinMode(greenLedPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(pirPin, INPUT);

 ledOff();
}

void loop() {  
  setup_wifi();

  if (wifi_connected) {
    setup_mqtt();
  }

  if (state != STATE_OFF) {
    pirValue = digitalRead(pirPin);
    Serial.println(pirValue);
  }
  
  client.loop();

  updateState();
  updateLed();

//  debugPrint();

  delay(500);
}

void onNewMessage(char* topic, byte* payload, unsigned int length) {
  // Conver the incoming byte array to a string
  payload[length] = '\0'; // Null terminator used to terminate the char array
  String message = (char*)payload;

  Serial.print("Message arrived on topic: [");
  Serial.print(topic);
  Serial.print("], ");
  Serial.println(message);

  if (message == "reset") {
    state = STATE_OK;
  } else if (message == "off") {
    state = STATE_OFF;
  } else if (message == "on") {
    state = STATE_OK;
  }
}

void updateState() {
  if (state == STATE_OFF) {
    return;
  }
  
  if (wifi_error || mqtt_error) {
    state = STATE_ERROR;
    return;
  }

  if (wifi_connecting || mqtt_connecting) {
    state = STATE_INIT;
    return;
  }

  if (state == STATE_ALARM || pirValue) {
    state = STATE_ALARM;
    return;
  }

  state = STATE_OK;
}

void setup_mqtt() {
  mqtt_connected = client.connected();

  if (mqtt_connected) {
    return;
  }

  if (!mqtt_connecting) {
    mqtt_connecting = true;

    if (client.connect(clientID)) {
      mqtt_error = false;
      mqtt_connecting = false;
      mqtt_connected = true;
      client.subscribe(inTopic);
    } else {
      mqtt_error = true;
      mqtt_connecting = false;
      mqtt_connected = false;
    }
  }
}

void setup_wifi() {
  wifi_connected = WiFi.status() == WL_CONNECTED;
  wifi_error = WiFi.status() == WL_CONNECT_FAILED;
  if (wifi_connected || wifi_error) {
    wifi_connecting = false;
  }
  
  if (wifi_connected) {
    return;
  }

  if (!wifi_connecting) {
    wifi_connecting = true;  
    WiFi.begin(ssid, ssid_password);    
  }
}

void ledRed() {
  digitalWrite(redLedPin, 1);
  digitalWrite(greenLedPin, 0);
}

void ledGreen() {
  digitalWrite(redLedPin, 0);
  digitalWrite(greenLedPin, 1);
}

void ledOff() {
  digitalWrite(redLedPin, 0);
  digitalWrite(greenLedPin, 0);
}

void updateLed() {
  if (state == STATE_ALARM) {
    ledRed();
    return;
  }

  if (state == STATE_INIT) {
    ledGreen();
    delay(250);
    ledOff();
    return;
  }

  if (state == STATE_ERROR) {
    ledRed();
    delay(250);
    ledOff();
    return;
  }

  if (state == STATE_OFF) {
    ledOff();
    return;
  }
  
  ledGreen();
}

void debugPrint() {
  Serial.println(" ");

  Serial.print("state = ");
  Serial.println(state);

  Serial.print("wifi connecting = ");
  Serial.println(wifi_connecting);

  Serial.print("wifi connected = ");
  Serial.println(wifi_connected);
  
  Serial.print("wifi error = ");
  Serial.println(wifi_error);

  Serial.print("mqtt connecting = ");
  Serial.println(mqtt_connecting);

  Serial.print("mqtt connected = ");
  Serial.println(mqtt_connected);
  
  Serial.print("mqtt error = ");
  Serial.println(mqtt_error);
}

