// ====================================================================
// Task 3: MQTT + JSON - Serial Command Gateway (Nano 33 IoT)
// ====================================================================
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include "SimpleJson.h"


const char* WIFI_SSID = "MagentaWLAN-S6KB";
const char* WIFI_PASS = "Zebrastrudel88250";


const char* MQTT_BROKER = "broker.hivemq.com";
const int   MQTT_PORT   = 1883;
const char* TOPIC_DATA  = "iem/task3/pico/data";
const char* TOPIC_CMD   = "iem/task3/pico/cmd";


const char* SHARED_TOKEN    = "iem2026";
const char* EXPECTED_SOURCE = "pico";
const unsigned long WATCHDOG_TIMEOUT = 10000;


int  remotePotValue = 0, remoteInterval = 0, remoteUptime = 0;
bool remoteBlink = false, remoteLedState = false, remoteSafeState = false;


unsigned long lastValidData = 0;
bool picoTimeout = false;
int  expectedSeqNr = -1;
unsigned long seqOutgoing = 0;
unsigned long msgAccepted = 0, msgRejectedJson = 0;
unsigned long msgRejectedAuth = 0, msgSeqGaps = 0;

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);
SimpleJson   jsonOut, jsonIn;

String inputBuffer = "";


void setupWiFi() {
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("ERROR: WiFi module not found!");
    while (true);
  }

  Serial.print("Connecting to WiFi");
  int attempts = 0;
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi connection FAILED!");
  }
}


bool validateMessage(const SimpleJson& msg) {
  if (strcmp(msg.getString("token"), SHARED_TOKEN) != 0) {
    Serial.println("REJECTED: Invalid token");
    msgRejectedAuth++;
    return false;
  }
  if (strcmp(msg.getString("source"), EXPECTED_SOURCE) != 0) {
    Serial.println("REJECTED: Invalid source");
    msgRejectedAuth++;
    return false;
  }
  return true;
}


bool checkSequence(const SimpleJson& msg) {
  int seq = msg.getInt("seq");

  if (seq == 0) {
    Serial.println("INFO: Sequence reset, resyncing.");
    expectedSeqNr = 1;
    return true;
  }
  if (expectedSeqNr == -1) {
    expectedSeqNr = seq + 1;
    return true;
  }
  if (seq < expectedSeqNr) {
    Serial.println("WARNING: Replay detected, discarding.");
    return false;
  }
  if (seq > expectedSeqNr) {
    msgSeqGaps++;
    Serial.print("WARNING: Sequence gap! Expected ");
    Serial.print(expectedSeqNr);
    Serial.print(", got ");
    Serial.println(seq);
  }
  expectedSeqNr = seq + 1;
  return true;
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char buf[256];
  if (length >= sizeof(buf)) length = sizeof(buf) - 1;
  memcpy(buf, payload, length);
  buf[length] = '\0';

  Serial.print("MQTT IN: ");
  Serial.println(buf);

  if (!jsonIn.parse(buf)) {
    Serial.println("REJECTED: JSON parse error");
    msgRejectedJson++;
    return;
  }
  if (!validateMessage(jsonIn)) return;
  if (!checkSequence(jsonIn)) return;


  remotePotValue  = jsonIn.getInt("potValue");
  remoteInterval  = jsonIn.getInt("interval");
  remoteUptime    = jsonIn.getInt("uptime");
  remoteBlink     = jsonIn.getBool("blinkEnabled");
  remoteLedState  = jsonIn.getBool("ledState");
  remoteSafeState = jsonIn.getBool("safeState");


  digitalWrite(LED_BUILTIN, remoteLedState ? HIGH : LOW);

  lastValidData = millis();
  picoTimeout   = false;
  msgAccepted++;
}


void sendCommand(SimpleJson& cmd) {
  cmd.setString("token",  SHARED_TOKEN);
  cmd.setString("source", "nano");
  cmd.setInt   ("seq",    (int)seqOutgoing++);

  char buf[256];
  cmd.toCharArray(buf, sizeof(buf));
  mqtt.publish(TOPIC_CMD, buf);

  Serial.print("MQTT OUT: ");
  Serial.println(buf);
}


void processSerialInput() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() == 0) return;

      inputBuffer.trim();
      String cmd = inputBuffer;
      inputBuffer = "";

      Serial.print("> ");
      Serial.println(cmd);

      jsonOut.clear();

      if (cmd == "ON") {
        jsonOut.setBool("blinkEnabled", false);
        jsonOut.setBool("ledOn", true);
        sendCommand(jsonOut);
        Serial.println("-> LED ON");

      } else if (cmd == "OFF") {
        jsonOut.setBool("blinkEnabled", false);
        jsonOut.setBool("ledOn", false);
        sendCommand(jsonOut);
        Serial.println("-> LED OFF");

      } else if (cmd == "BLINK") {
        jsonOut.setBool("blinkEnabled", true);
        sendCommand(jsonOut);
        Serial.println("-> Blink ON");

      } else if (cmd == "NOBLINK") {
        jsonOut.setBool("blinkEnabled", false);
        sendCommand(jsonOut);
        Serial.println("-> Blink OFF");

      } else if (cmd.startsWith("INTERVAL ")) {
        int ms = cmd.substring(9).toInt();
        if (ms >= 50 && ms <= 2000) {
          jsonOut.setInt("interval", ms);
          sendCommand(jsonOut);
          Serial.print("-> Interval set to ");
          Serial.print(ms);
          Serial.println(" ms");
        } else {
          Serial.println("ERROR: Interval must be 50-2000 ms");
        }

      } else if (cmd == "POT") {
        jsonOut.setBool("useLocalPot", true);
        sendCommand(jsonOut);
        Serial.println("-> Local pot control enabled");

      } else if (cmd == "STATUS") {
        Serial.println("-- Remote Pico Status --");
        Serial.print("LED: ");
        Serial.println(remoteLedState ? "ON" : "OFF");
        Serial.print("Blink: ");
        Serial.println(remoteBlink ? "ON" : "OFF");
        Serial.print("Interval: ");
        Serial.print(remoteInterval);
        Serial.println(" ms");
        Serial.print("Pot: ");
        Serial.println(remotePotValue);
        Serial.print("Uptime: ");
        Serial.print(remoteUptime);
        Serial.println(" s");
        Serial.print("Safe State: ");
        Serial.println(remoteSafeState ? "YES" : "NO");

      } else if (cmd == "STATS") {
        Serial.println("-- Statistics --");
        Serial.print("Accepted: ");
        Serial.println(msgAccepted);
        Serial.print("Rejected (auth): ");
        Serial.println(msgRejectedAuth);
        Serial.print("Rejected (json): ");
        Serial.println(msgRejectedJson);
        Serial.print("Seq gaps: ");
        Serial.println(msgSeqGaps);

      } else if (cmd == "HELP") {
        Serial.println("-- Commands --");
        Serial.println("ON         - LED on");
        Serial.println("OFF        - LED off");
        Serial.println("BLINK      - Blink on");
        Serial.println("NOBLINK    - Blink off");
        Serial.println("INTERVAL x - Set interval (50-2000ms)");
        Serial.println("POT        - Local pot control");
        Serial.println("STATUS     - Show remote state");
        Serial.println("STATS      - Show statistics");

      } else {
        Serial.println("Unknown command. Type HELP.");
      }

    } else {
      inputBuffer += c;
    }
  }
}


void mqttReconnect() {
  while (!mqtt.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "NanoIoT-" + String(random(0xffff), HEX);
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("connected!");
      mqtt.subscribe(TOPIC_DATA);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" retrying in 3s...");
      delay(3000);
    }
  }
}


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  while (!Serial) { delay(100); }

  Serial.println("Nano 33 IoT - Task 3 MQTT Gateway");
  setupWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
}


void loop() {
  if (!mqtt.connected()) mqttReconnect();
  mqtt.loop();

  if (lastValidData > 0 && (millis() - lastValidData > WATCHDOG_TIMEOUT)) {
    if (!picoTimeout) {
      picoTimeout = true;
      digitalWrite(LED_BUILTIN, LOW);
      Serial.println("WARNING: No data from Pico for 10s!");
    }
  }

  processSerialInput();
}