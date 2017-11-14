#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Devive identifiers
const char* deviceName = "motion_sensor_01";
const char* roomName = "living_room";
const char* deviceType = "motion_sensor";

// Network
const char* ssid = "Cloud";
const char* ssid_password = "";

// MQTT
const char* mqtt_server = "192.168.0.110";
const char* outTopic = "home/out";
const char* inTopic = "home/in";
const char* debugTopic = "home/debug";

const int STATE_OFF = 0;
const int STATE_OK = 1;
const int STATE_INIT = 2;
const int STATE_ERROR = 3;
const int STATE_ALARM = 4;

int state = STATE_INIT;
int lastReportedState = STATE_OFF;
boolean forceSendStateOnNextLoop = false;

const char* CMD_STATE = "state";
const char* CMD_ON = "on";
const char* CMD_OFF = "off";
const char* CMD_RESET = "reset";

const char* NAME_ALL = "all";

int greenLedPin = 14;
int redLedPin = 4;
int pirPin = 12; // Input for motion sensor
int pirValue; // Place to store read PIR Value

WiFiClient espClient;
PubSubClient client(espClient);

boolean wifi_connecting = false, wifi_connected = false, wifi_error = false;
boolean mqtt_connecting = false, mqtt_connected = false, mqtt_error = false;
long wifiStrength = 0;
long lastReportedWifiStrength = 0;

DynamicJsonBuffer jsonBuffer;
char jsonMessageBuffer[256];

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
  }

  if (state != lastReportedState || abs(lastReportedWifiStrength - wifiStrength) > 3 || forceSendStateOnNextLoop) {
    sendState();
    lastReportedState = state;
    lastReportedWifiStrength = wifiStrength;
    forceSendStateOnNextLoop = false;
  }
  
  client.loop();

  updateState();
  updateLed();

//  debugPrint();

  delay(250);
}

void sendState() {
  JsonObject& root = jsonBuffer.createObject();
  root["name"] = deviceName;
  root["room"] = roomName;
  root["type"] = deviceType;
  root["signal"] = wifiStrength;

  root["state"] = state;  
  
  root.printTo(jsonMessageBuffer, sizeof(jsonMessageBuffer));
  
//  Serial.print("--> ");
//  Serial.println(jsonMessageBuffer);
  client.publish(outTopic, jsonMessageBuffer);
}

void onNewMessage(char* topic, byte* payload, unsigned int length) {
  JsonObject& message = jsonBuffer.parseObject(payload);

  if (!message.success())
  {
    client.publish(debugTopic, deviceName);
    client.publish(debugTopic, "cannot parse message");
//    Serial.println("cannot parse message");
    return;
  }

//  message.printTo(jsonMessageBuffer, sizeof(jsonMessageBuffer));  
//  Serial.print("<-- ");
//  Serial.println(jsonMessageBuffer);

  const char* messageName = message["name"];
  if (!eq(messageName, deviceName) && !eq(messageName, NAME_ALL)) {
    return;
  }

  const char* messageCmd = message["cmd"];
  if (eq(messageCmd, CMD_STATE)) {
    forceSendStateOnNextLoop = true;
  } else if (eq(messageCmd, CMD_ON)) {
    state = STATE_OK;
    forceSendStateOnNextLoop = true;
  } else if (eq(messageCmd, CMD_OFF)) {
    state = STATE_OFF;
    forceSendStateOnNextLoop = true;
  } else if (eq(messageCmd, CMD_RESET)) {
    state = STATE_OK;
    forceSendStateOnNextLoop = true;
  }
}

void updateState() {
  if (wifi_connected) {
    wifiStrength = WiFi.RSSI();
  } else {
    wifiStrength = 0;
  }

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

    if (client.connect(deviceName)) {
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

boolean eq(const char* a1, const char* a2) {
  return strcmp(a1, a2) == 0;
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

