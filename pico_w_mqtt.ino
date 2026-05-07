// ====================================================================
// Task 3: MQTT + JSON (Pico W, Wokwi)
// ====================================================================
#include <WiFi.h>
#include <PubSubClient.h>
#include "SimpleJson.h"

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

const char* MQTT_BROKER = "broker.hivemq.com";
const int   MQTT_PORT   = 1883;
const char* TOPIC_DATA  = "iem/task3/pico/data";
const char* TOPIC_CMD   = "iem/task3/pico/cmd";


const char* SHARED_TOKEN    = "iem2026";
const char* EXPECTED_SOURCE = "nano";
const unsigned long WATCHDOG_TIMEOUT = 10000;
const int INTERVAL_MIN = 50;
const int INTERVAL_MAX = 2000;


const int LED_PIN    = 28;
const int BUTTON_PIN = 2;
const int POT_PIN    = 26;


bool blinkEnabled     = true;
bool ledState         = false;
int  overrideInterval = -1;
unsigned long lastValidCmd = 0;
bool inSafeState      = false;
int  expectedSeqNr    = -1;
unsigned long seqOutgoing = 0;


unsigned long lastBlinkTime = 0;
unsigned long lastPublish   = 0;


bool lastButtonState   = HIGH;
unsigned long lastDebounce = 0;


unsigned long msgAccepted = 0, msgRejectedJson = 0;
unsigned long msgRejectedAuth = 0, msgSeqGaps = 0;

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);
SimpleJson   jsonOut, jsonIn;


void setupWiFi() {
  Serial1.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial1.print(".");
  }
  Serial1.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
}


void enterSafeState() {
  if (!inSafeState) {
    inSafeState = true;
    overrideInterval = -1;
    Serial1.println("WARNING: Watchdog timeout! Entering safe state.");
  }
}

void leaveSafeState() {
  if (inSafeState) {
    inSafeState = false;
    Serial1.println("INFO: Valid command received. Leaving safe state.");
  }
}


bool validateMessage(const SimpleJson& msg) {
  if (strcmp(msg.getString("token"), SHARED_TOKEN) != 0) {
    Serial1.println("REJECTED: Invalid token");
    msgRejectedAuth++;
    return false;
  }
  if (strcmp(msg.getString("source"), EXPECTED_SOURCE) != 0) {
    Serial1.println("REJECTED: Invalid source");
    msgRejectedAuth++;
    return false;
  }
  return true;
}


bool checkSequence(const SimpleJson& msg) {
  int seq = msg.getInt("seq");

  if (seq == 0) {
    Serial1.println("INFO: Sequence reset, resyncing.");
    expectedSeqNr = 1;
    return true;
  }
  if (expectedSeqNr == -1) {
    expectedSeqNr = seq + 1;
    return true;
  }
  if (seq < expectedSeqNr) {
    Serial1.println("WARNING: Replay detected, discarding.");
    return false;
  }
  if (seq > expectedSeqNr) {
    msgSeqGaps++;
    Serial1.print("WARNING: Sequence gap! Expected ");
    Serial1.print(expectedSeqNr);
    Serial1.print(", got ");
    Serial1.println(seq);
  }
  expectedSeqNr = seq + 1;
  return true;
}


void processCommand(const SimpleJson& cmd) {
  if (cmd.hasKey("blinkEnabled")) {
    blinkEnabled = cmd.getBool("blinkEnabled");
    Serial1.print("CMD: blinkEnabled = ");
    Serial1.println(blinkEnabled);
  }
  if (cmd.hasKey("ledOn")) {
    bool on = cmd.getBool("ledOn");
    ledState = on;
    digitalWrite(LED_PIN, on ? HIGH : LOW);
    blinkEnabled = false;
    Serial1.print("CMD: ledOn = ");
    Serial1.println(on);
  }
  if (cmd.hasKey("interval")) {
    int iv = cmd.getInt("interval");
    if (iv >= INTERVAL_MIN && iv <= INTERVAL_MAX) {
      overrideInterval = iv;
      Serial1.print("CMD: interval = ");
      Serial1.println(iv);
    } else {
      Serial1.print("REJECTED: interval out of range: ");
      Serial1.println(iv);
    }
  }
  if (cmd.hasKey("useLocalPot")) {
    if (cmd.getBool("useLocalPot")) {
      overrideInterval = -1;
      Serial1.println("CMD: useLocalPot -> back to poti control");
    }
  }
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char buf[256];
  if (length >= sizeof(buf)) length = sizeof(buf) - 1;
  memcpy(buf, payload, length);
  buf[length] = '\0';

  Serial1.print("MQTT IN: ");
  Serial1.println(buf);

  if (!jsonIn.parse(buf)) {
    Serial1.println("REJECTED: JSON parse error");
    msgRejectedJson++;
    return;
  }
  if (!validateMessage(jsonIn)) return;
  if (!checkSequence(jsonIn)) return;

  lastValidCmd = millis();
  leaveSafeState();
  msgAccepted++;

  processCommand(jsonIn);
}


void mqttReconnect() {
  while (!mqtt.connected()) {
    Serial1.print("Connecting to MQTT...");
    String clientId = "PicoW-" + String(random(0xffff), HEX);
    if (mqtt.connect(clientId.c_str())) {
      Serial1.println("connected!");
      mqtt.subscribe(TOPIC_CMD);
    } else {
      Serial1.print("failed, rc=");
      Serial1.print(mqtt.state());
      Serial1.println(" retrying in 3s...");
      delay(3000);
    }
  }
}


void publishSensorData(int potValue, unsigned long blinkInterval) {
  jsonOut.clear();
  jsonOut.setString("token",        SHARED_TOKEN);
  jsonOut.setString("source",       "pico");
  jsonOut.setInt   ("seq",          (int)seqOutgoing++);
  jsonOut.setInt   ("potValue",     potValue);
  jsonOut.setBool  ("blinkEnabled", blinkEnabled);
  jsonOut.setInt   ("interval",     (int)blinkInterval);
  jsonOut.setBool  ("ledState",     ledState);
  jsonOut.setBool  ("safeState",    inSafeState);
  jsonOut.setInt   ("uptime",       (int)(millis() / 1000));

  char buf[256];
  jsonOut.toCharArray(buf, sizeof(buf));
  mqtt.publish(TOPIC_DATA, buf);

  Serial1.print("MQTT OUT: ");
  Serial1.println(buf);
}


void setup() {
  pinMode(LED_PIN,    OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial1.begin(115200);
  delay(1000);

  setupWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
}


void loop() {
  if (!mqtt.connected()) mqttReconnect();
  mqtt.loop();

  // Watchdog check
  if (lastValidCmd > 0 && (millis() - lastValidCmd > WATCHDOG_TIMEOUT)) {
    enterSafeState();
  }

  // Button debounce
  bool buttonState = digitalRead(BUTTON_PIN);
  if (lastButtonState == HIGH && buttonState == LOW) {
    if (millis() - lastDebounce > 50) {
      blinkEnabled = !blinkEnabled;
      Serial1.print("Button: blinkEnabled = ");
      Serial1.println(blinkEnabled);
      lastDebounce = millis();
    }
  }
  lastButtonState = buttonState;

  // Read poti + determine interval
  int potValue = analogRead(POT_PIN);
  unsigned long blinkInterval;
  if (overrideInterval != -1) {
    blinkInterval = overrideInterval;
  } else {
    blinkInterval = map(potValue, 0, 4095, INTERVAL_MIN, INTERVAL_MAX);
  }

  // Blink logic
  if (blinkEnabled) {
    if (millis() - lastBlinkTime >= blinkInterval) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
      lastBlinkTime = millis();
    }
  }

  // Publish every 500ms
  if (millis() - lastPublish >= 500) {
    publishSensorData(potValue, blinkInterval);
    lastPublish = millis();
  }
}
