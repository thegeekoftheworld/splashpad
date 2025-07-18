#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

#define MQTT_HOST "mqtt.domain.tld"
#define MQTT_PORT 1883
#define MQTT_USER "mqtt_user"
#define MQTT_PASS "mqtt_pass"
#define CONTROLLER_NAME "SplashPad Main"
#define CONTROLLER_LOCATION "Park Center"

const int NUM_VALVES = 13;
const int valvePins[NUM_VALVES] = {22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34};
const int motionPin = 2;

byte mac[6];
char topicPrefix[40];
IPAddress dns(8, 8, 8, 8);
EthernetClient ethClient;
PubSubClient client(ethClient);

bool enabled = false;
bool maintenanceMode = false;
bool valveCycleActive = false;
bool valveState[NUM_VALVES] = { false };

float temperature = 0.0;
float onTempThreshold = 40.0;
float lightningDistance = 20.0;
int maxValvesAtOnce = 3;

unsigned long lastMotionMillis = 0;
unsigned long lastValveSwitch = 0;
unsigned long valveDuration = 0;
unsigned long lastTimerPublish = 0;
unsigned long lastConfigHeartbeat = 0;
unsigned long nextResetMillis = 0;

enum ControllerMode {
  MODE_AUTO,
  MODE_OFF,
  MODE_MAINTENANCE_ON,
  MODE_MAINTENANCE_OFF,
  MODE_MAINTENANCE_FORCED_ON
};

ControllerMode currentMode = MODE_AUTO;

void setupEthernet() {
  Ethernet.init(10);
  bool macSet = true;
  for (int i = 0; i < 6; i++) {
    mac[i] = EEPROM.read(i);
    if (mac[i] == 0xFF || mac[i] == 0x00) macSet = false;
  }
  if (!macSet) {
    mac[0] = 0x00; mac[1] = 0x10; mac[2] = 0xA4;
    for (int i = 3; i < 6; i++) {
      mac[i] = random(0, 255);
      EEPROM.write(i, mac[i]);
    }
  }
  snprintf(topicPrefix, sizeof(topicPrefix), "splashpad/%02X%02X%02X%02X%02X%02X/",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Ethernet.begin(mac);
  Ethernet.dnsServerIP() = dns;
  delay(1000);
}

void reconnect() {
  while (!client.connected()) {
    char lwtTopic[80];
    snprintf(lwtTopic, sizeof(lwtTopic), "%scontroller/status", topicPrefix);
    char identityTopic[80];
    snprintf(identityTopic, sizeof(identityTopic), "%scontroller/identity", topicPrefix);

    if (client.connect("valveController", MQTT_USER, MQTT_PASS, lwtTopic, 1, true, "{\"status\":\"OFFLINE\"}")) {
      publishStatus("ONLINE");
      StaticJsonDocument<128> idDoc;
      idDoc["name"] = CONTROLLER_NAME;
      idDoc["location"] = CONTROLLER_LOCATION;
      char identityJson[128];
      serializeJson(idDoc, identityJson);
      client.publish(identityTopic, identityJson, true);

      const char* subs[] = {"controller/enable", "controller/config", "controller/reset", "controller/maintenance", "controller/mode"};
      for (int i = 0; i < 5; i++) {
        char topic[80];
        snprintf(topic, sizeof(topic), "%s%s", topicPrefix, subs[i]);
        client.subscribe(topic);
      }

      client.subscribe("splashpad/weather");
    } else {
      delay(5000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String message = String((char*)payload);
  String t = String(topic);

  if (t.endsWith("controller/enable")) {
    enabled = (message == "ON");
  } else if (t.endsWith("controller/maintenance")) {
    maintenanceMode = (message == "ON");
  } else if (t.endsWith("controller/config")) {
    StaticJsonDocument<256> doc;
    if (!deserializeJson(doc, message)) {
      if (doc.containsKey("on_temp")) onTempThreshold = doc["on_temp"].as<float>();
      if (doc.containsKey("max_valves")) {
        int val = doc["max_valves"].as<int>();
        if (val > 0 && val <= NUM_VALVES) maxValvesAtOnce = val;
      }
      if (doc.containsKey("enable")) enabled = (doc["enable"].as<String>() == "ON");
      if (doc.containsKey("maintenance")) maintenanceMode = (doc["maintenance"].as<String>() == "ON");
      if (doc.containsKey("mode")) {
        String mode = doc["mode"].as<String>();
        if (mode == "AUTO") setControllerMode(MODE_AUTO);
        else if (mode == "OFF") setControllerMode(MODE_OFF);
        else if (mode == "MAINTENANCE_ON") setControllerMode(MODE_MAINTENANCE_ON);
        else if (mode == "MAINTENANCE_OFF") setControllerMode(MODE_MAINTENANCE_OFF);
        else if (mode == "MAINTENANCE_FORCED_ON") setControllerMode(MODE_MAINTENANCE_FORCED_ON);
      }
    }
  } else if (t.endsWith("controller/reset")) {
    turnOffAllValves();
    char fullTopic[80];
    snprintf(fullTopic, sizeof(fullTopic), "%scontroller/status", topicPrefix);
    client.publish(fullTopic, "{\"status\":\"MANUAL_RESET\"}", true);
    delay(1000);
    client.publish(fullTopic, "{\"status\":\"RESTARTING\"}", true);
    delay(1000);
    asm volatile ("jmp 0");
  } else if (t == "splashpad/weather") {
    StaticJsonDocument<256> doc;
    if (!deserializeJson(doc, message)) {
      if (doc.containsKey("temp")) {
        temperature = doc["temp"];
        publishStatus(enabled ? (valveCycleActive ? "ON" : "IDLE") : "OFF");
      }
      if (doc.containsKey("lightning")) {
        lightningDistance = doc["lightning"];
        if (lightningDistance <= 20.0) {
          enabled = false;
          publishStatus("LIGHTNING_SHUTOFF");
        }
      }
    }
  } else if (t.endsWith("controller/mode")) {
    if (message == "AUTO") setControllerMode(MODE_AUTO);
    else if (message == "OFF") setControllerMode(MODE_OFF);
    else if (message == "MAINTENANCE_ON") setControllerMode(MODE_MAINTENANCE_ON);
    else if (message == "MAINTENANCE_OFF") setControllerMode(MODE_MAINTENANCE_OFF);
    else if (message == "MAINTENANCE_FORCED_ON") setControllerMode(MODE_MAINTENANCE_FORCED_ON);
  }
}

void setupMQTT() {
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(mqttCallback);
}

void publishStatus(const char* status) {
  char valveList[100] = "";
  bool first = true;
  for (int i = 0; i < NUM_VALVES; i++) {
    if (valveState[i]) {
      if (!first) strcat(valveList, ",");
      char buf[8];
      itoa(i, buf, 10);
      strcat(valveList, buf);
      first = false;
    }
  }

  unsigned long remaining = 0;
  if (valveCycleActive) {
    unsigned long elapsed = (millis() - lastMotionMillis) / 1000;
    remaining = (elapsed < 900) ? 900 - elapsed : 0;
  }
  const char* motion = (digitalRead(motionPin) == HIGH) ? "MOTION_DETECTED" : "IDLE";

  char buffer[512];
  snprintf(buffer, sizeof(buffer),
    "{\"status\":\"%s\",\"name\":\"%s\",\"enabled\":%s,\"temperature\":%.1f,\"on_temp\":%.1f,\"valve_active\":%s,\"valves\":[%s],\"max_valves\":%d,\"maintenance\":%s,\"motion\":\"%s\",\"timer_remaining\":%lu,\"lightning_distance\":%.1f,\"location\":\"%s\"}",
    status,
    CONTROLLER_NAME,
    enabled ? "true" : "false",
    temperature,
    onTempThreshold,
    valveCycleActive ? "true" : "false",
    valveList,
    maxValvesAtOnce,
    maintenanceMode ? "true" : "false",
    motion,
    remaining,
    lightningDistance,
    CONTROLLER_LOCATION);

  char fullTopic[80];
  snprintf(fullTopic, sizeof(fullTopic), "%scontroller/status", topicPrefix);
  client.publish(fullTopic, buffer, true);
}

void turnOffAllValves() {
  for (int i = 0; i < NUM_VALVES; i++) {
    digitalWrite(valvePins[i], LOW);
    valveState[i] = false;
  }
}

void switchRandomValves() {
  turnOffAllValves();
  int numToActivate = random(1, maxValvesAtOnce + 1);
  for (int i = 0; i < numToActivate; i++) {
    int valve = random(NUM_VALVES);
    digitalWrite(valvePins[valve], HIGH);
    valveState[valve] = true;
  }
  valveDuration = random(10, 31) * 1000UL;
  lastValveSwitch = millis();
}

void scheduleNextReset() {
  unsigned long base = 24UL * 60 * 60 * 1000;
  unsigned long extra = (1 + random(6)) * 60UL * 60 * 1000;
  nextResetMillis = millis() + base + extra;
}

void setControllerMode(ControllerMode mode) {
  currentMode = mode;
  switch (mode) {
    case MODE_AUTO:
      enabled = true;
      maintenanceMode = false;
      break;
    case MODE_OFF:
      enabled = false;
      maintenanceMode = false;
      turnOffAllValves();
      break;
    case MODE_MAINTENANCE_ON:
      enabled = true;
      maintenanceMode = true;
      break;
    case MODE_MAINTENANCE_OFF:
      enabled = false;
      maintenanceMode = false;
      turnOffAllValves();
      break;
    case MODE_MAINTENANCE_FORCED_ON:
      enabled = true;
      maintenanceMode = true;
      valveCycleActive = true;
      publishStatus("ON");
      switchRandomValves();
      break;
  }
  publishStatus("MODE_CHANGED");
}

void setup() {
  randomSeed(analogRead(0));
  scheduleNextReset();

  for (int i = 0; i < NUM_VALVES; i++) {
    pinMode(valvePins[i], OUTPUT);
    digitalWrite(valvePins[i], LOW);
  }
  pinMode(motionPin, INPUT);

  setupEthernet();
  setupMQTT();
  publishStatus("ONLINE");
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  if (millis() - lastConfigHeartbeat >= 60000) {
    lastConfigHeartbeat = millis();
    publishStatus(enabled ? (valveCycleActive ? "ON" : "IDLE") : "OFF");
  }

  if (millis() >= nextResetMillis) {
    turnOffAllValves();
    publishStatus("RESTARTING");
    delay(1000);
    asm volatile ("jmp 0");
  }

  if (digitalRead(motionPin) == HIGH) {
    lastMotionMillis = millis();
    if (!valveCycleActive && enabled && !maintenanceMode && temperature >= onTempThreshold) {
      valveCycleActive = true;
      publishStatus("ON");
      switchRandomValves();
    }
  }

  if (valveCycleActive) {
    unsigned long elapsed = millis() - lastMotionMillis;
    if (elapsed > 900000 || !enabled || temperature < onTempThreshold) {
      valveCycleActive = false;
      turnOffAllValves();
      publishStatus(temperature < onTempThreshold ? "TEMP_LOW" : "IDLE");
    } else {
      if (millis() - lastValveSwitch >= valveDuration) switchRandomValves();
      if (millis() - lastTimerPublish >= 5000) {
        lastTimerPublish = millis();
        publishStatus("ON");
      }
    }
  } else {
    if (!enabled) publishStatus("OFF");
  }
}
