/*
 * GreenHouse8 v1.9.0 - Motor Control Enhancement
 * Enhanced motor control with auto mode, timing, and safety features
 * ESP32-S3 based greenhouse automation system
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <Update.h>

// Version
const char* VERSION = "GreenHouse8 v1.9.0 - Motor Control Enhanced";

// WiFi credentials (to be configured)
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ThingsBoard MQTT settings
const char* TB_SERVER = "YOUR_THINGSBOARD_SERVER";
const int TB_PORT = 1883;
const char* TB_TOKEN = "YOUR_DEVICE_TOKEN";

// Pin definitions (example - adjust based on hardware)
#define MOTOR_1_FW_PIN 16
#define MOTOR_1_RE_PIN 17
#define MOTOR_2_FW_PIN 18
#define MOTOR_2_RE_PIN 19
#define MOTOR_3_FW_PIN 20
#define MOTOR_3_RE_PIN 21
#define MOTOR_4_FW_PIN 22
#define MOTOR_4_RE_PIN 23

#define FAN_1_PIN 25
#define FAN_2_PIN 26

// Global objects
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Preferences preferences;

// Fan control variables (existing functionality)
bool fan1Auto = false;
unsigned long fan1OnTime = 30000;  // milliseconds
unsigned long fan1OffTime = 30000;
unsigned long fan1LastAction = 0;
bool fan1State = false;

bool fan2Auto = false;
unsigned long fan2OnTime = 30000;
unsigned long fan2OffTime = 30000;
unsigned long fan2LastAction = 0;
bool fan2State = false;

// Motor control variables - Individual motors
bool motor1Auto = false;
bool motor2Auto = false;
bool motor3Auto = false;
bool motor4Auto = false;

// Motor pin arrays for easier iteration
const int motorFwPins[4] = {MOTOR_1_FW_PIN, MOTOR_2_FW_PIN, MOTOR_3_FW_PIN, MOTOR_4_FW_PIN};
const int motorRePins[4] = {MOTOR_1_RE_PIN, MOTOR_2_RE_PIN, MOTOR_3_RE_PIN, MOTOR_4_RE_PIN};

// Motor states (0 = off, 1 = forward, 2 = reverse)
int motorStatus[4] = {0, 0, 0, 0};  // Array for motors 1-4 (index 0-3)

// Global motor auto control variables (NEW)
bool globalMotorAuto = false;
unsigned long globalFwTime = 30000;  // milliseconds
unsigned long globalReTime = 30000;  // milliseconds
unsigned long lastGlobalAction = 0;
int lastGlobalMotorState = 0;  // 0 = off, 1 = forward, 2 = reverse

// MQTT topics
const char* RPC_REQUEST_TOPIC = "v1/devices/me/rpc/request/+";
const char* RPC_RESPONSE_TOPIC = "v1/devices/me/rpc/response/";
const char* TELEMETRY_TOPIC = "v1/devices/me/telemetry";
const char* ATTRIBUTES_TOPIC = "v1/devices/me/attributes";

// Function prototypes
void setupWiFi();
void setupMQTT();
void reconnectMQTT();
void callback(char* topic, byte* payload, unsigned int length);
void checkAutoTimers();
void controlMotor(int motorNum, int status);
void controlMotorStatus(int motorNum, int status);
void loadGlobalConfig();
void saveGlobalConfig();
void loadFanConfig();
void saveFanConfig();
void sendTelemetry();
void processRPCRequest(const char* method, JsonDocument& params, const char* requestId);
void checkOTAUpdate();

// Helper function to prevent short circuit with interlock
void setMotorPins(int fwPin, int rePin, int status) {
  // Interlock: Never activate both FW and RE at the same time
  // First turn off both pins, then activate the desired one
  digitalWrite(fwPin, LOW);
  digitalWrite(rePin, LOW);
  
  // Brief delay for relay settling (10ms is usually sufficient for most relays)
  // This is a blocking delay but necessary for hardware safety
  // Trade-off: 10ms blocking vs potential short circuit damage
  delay(10);
  
  if (status == 1) {
    // Forward
    digitalWrite(fwPin, HIGH);
  } else if (status == 2) {
    // Reverse
    digitalWrite(rePin, HIGH);
  }
  // status == 0 means both stay LOW (motor off)
}

void setup() {
  Serial.begin(115200);
  Serial.println(VERSION);
  
  // Initialize NVS
  preferences.begin("greenhouse", false);
  
  // Setup pins
  pinMode(MOTOR_1_FW_PIN, OUTPUT);
  pinMode(MOTOR_1_RE_PIN, OUTPUT);
  pinMode(MOTOR_2_FW_PIN, OUTPUT);
  pinMode(MOTOR_2_RE_PIN, OUTPUT);
  pinMode(MOTOR_3_FW_PIN, OUTPUT);
  pinMode(MOTOR_3_RE_PIN, OUTPUT);
  pinMode(MOTOR_4_FW_PIN, OUTPUT);
  pinMode(MOTOR_4_RE_PIN, OUTPUT);
  pinMode(FAN_1_PIN, OUTPUT);
  pinMode(FAN_2_PIN, OUTPUT);
  
  // Initialize all motors to OFF state using arrays
  for (int i = 0; i < 4; i++) {
    setMotorPins(motorFwPins[i], motorRePins[i], 0);
  }
  
  // Load configurations from NVS
  loadGlobalConfig();
  loadFanConfig();
  
  // Setup WiFi and MQTT
  setupWiFi();
  setupMQTT();
  
  Serial.println("Setup Complete!");
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();
  
  // Check auto timers for fans and motors
  checkAutoTimers();
  
  // Send telemetry periodically
  static unsigned long lastTelemetry = 0;
  if (millis() - lastTelemetry > 5000) {
    sendTelemetry();
    lastTelemetry = millis();
  }
  
  // Check for OTA updates periodically
  static unsigned long lastOTACheck = 0;
  if (millis() - lastOTACheck > 300000) {  // Every 5 minutes
    checkOTAUpdate();
    lastOTACheck = millis();
  }
}

void setupWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed!");
  }
}

void setupMQTT() {
  mqttClient.setServer(TB_SERVER, TB_PORT);
  mqttClient.setCallback(callback);
  mqttClient.setBufferSize(512);
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to ThingsBoard...");
    if (mqttClient.connect("GreenHouse8", TB_TOKEN, NULL)) {
      Serial.println("connected");
      mqttClient.subscribe(RPC_REQUEST_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.println(mqttClient.state());
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("]");
  
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  
  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Extract request ID from topic
  String topicStr = String(topic);
  int lastSlash = topicStr.lastIndexOf('/');
  String requestId = topicStr.substring(lastSlash + 1);
  
  const char* method = doc["method"];
  JsonVariant params = doc["params"];
  
  processRPCRequest(method, doc, requestId.c_str());
}

void processRPCRequest(const char* method, JsonDocument& doc, const char* requestId) {
  Serial.print("RPC Method: ");
  Serial.println(method);
  
  JsonVariant params = doc["params"];
  
  // Global motor auto control (NEW)
  if (strcmp(method, "set_global_motor_auto") == 0) {
    globalMotorAuto = params["enabled"].as<bool>();
    saveGlobalConfig();
    
    // Send response
    JsonDocument responseDoc;
    responseDoc["enabled"] = globalMotorAuto;
    char response[128];
    serializeJson(responseDoc, response);
    String responseTopic = String(RPC_RESPONSE_TOPIC) + requestId;
    mqttClient.publish(responseTopic.c_str(), response);
    
    Serial.print("Global motor auto set to: ");
    Serial.println(globalMotorAuto);
  }
  else if (strcmp(method, "set_global_fw_time") == 0) {
    globalFwTime = params["time"].as<unsigned long>() * 1000;  // Convert seconds to ms
    saveGlobalConfig();
    
    JsonDocument responseDoc;
    responseDoc["time"] = globalFwTime / 1000;
    char response[128];
    serializeJson(responseDoc, response);
    String responseTopic = String(RPC_RESPONSE_TOPIC) + requestId;
    mqttClient.publish(responseTopic.c_str(), response);
  }
  else if (strcmp(method, "set_global_re_time") == 0) {
    globalReTime = params["time"].as<unsigned long>() * 1000;  // Convert seconds to ms
    saveGlobalConfig();
    
    JsonDocument responseDoc;
    responseDoc["time"] = globalReTime / 1000;
    char response[128];
    serializeJson(responseDoc, response);
    String responseTopic = String(RPC_RESPONSE_TOPIC) + requestId;
    mqttClient.publish(responseTopic.c_str(), response);
  }
  // Individual motor control with safety lock (NEW)
  else if (strcmp(method, "set_motor_1_status") == 0) {
    int status = params["status"].as<int>();
    controlMotorStatus(1, status);
    
    JsonDocument responseDoc;
    responseDoc["status"] = motorStatus[0];
    char response[128];
    serializeJson(responseDoc, response);
    String responseTopic = String(RPC_RESPONSE_TOPIC) + requestId;
    mqttClient.publish(responseTopic.c_str(), response);
  }
  else if (strcmp(method, "set_motor_2_status") == 0) {
    int status = params["status"].as<int>();
    controlMotorStatus(2, status);
    
    JsonDocument responseDoc;
    responseDoc["status"] = motorStatus[1];
    char response[128];
    serializeJson(responseDoc, response);
    String responseTopic = String(RPC_RESPONSE_TOPIC) + requestId;
    mqttClient.publish(responseTopic.c_str(), response);
  }
  else if (strcmp(method, "set_motor_3_status") == 0) {
    int status = params["status"].as<int>();
    controlMotorStatus(3, status);
    
    JsonDocument responseDoc;
    responseDoc["status"] = motorStatus[2];
    char response[128];
    serializeJson(responseDoc, response);
    String responseTopic = String(RPC_RESPONSE_TOPIC) + requestId;
    mqttClient.publish(responseTopic.c_str(), response);
  }
  else if (strcmp(method, "set_motor_4_status") == 0) {
    int status = params["status"].as<int>();
    controlMotorStatus(4, status);
    
    JsonDocument responseDoc;
    responseDoc["status"] = motorStatus[3];
    char response[128];
    serializeJson(responseDoc, response);
    String responseTopic = String(RPC_RESPONSE_TOPIC) + requestId;
    mqttClient.publish(responseTopic.c_str(), response);
  }
  // Legacy motor control methods with safety lock (NEW)
  else if (strcmp(method, "set_motor_1_fw") == 0) {
    controlMotorStatus(1, 1);  // Status 1 = forward
  }
  else if (strcmp(method, "set_motor_1_re") == 0) {
    controlMotorStatus(1, 2);  // Status 2 = reverse
  }
  else if (strcmp(method, "set_motor_2_fw") == 0) {
    controlMotorStatus(2, 1);
  }
  else if (strcmp(method, "set_motor_2_re") == 0) {
    controlMotorStatus(2, 2);
  }
  else if (strcmp(method, "set_motor_3_fw") == 0) {
    controlMotorStatus(3, 1);
  }
  else if (strcmp(method, "set_motor_3_re") == 0) {
    controlMotorStatus(3, 2);
  }
  else if (strcmp(method, "set_motor_4_fw") == 0) {
    controlMotorStatus(4, 1);
  }
  else if (strcmp(method, "set_motor_4_re") == 0) {
    controlMotorStatus(4, 2);
  }
  // Group control - All motors forward
  else if (strcmp(method, "set_all_motors_fw") == 0) {
    if (!globalMotorAuto) {  // Safety lock check
      setAllMotorsStatus(1);
    } else {
      Serial.println("All motors control blocked - Global auto mode is active");
    }
  }
  // Group control - All motors reverse
  else if (strcmp(method, "set_all_motors_re") == 0) {
    if (!globalMotorAuto) {  // Safety lock check
      setAllMotorsStatus(2);
    } else {
      Serial.println("All motors control blocked - Global auto mode is active");
    }
  }
  // Group control - All motors off
  else if (strcmp(method, "set_all_motors_off") == 0) {
    if (!globalMotorAuto) {  // Safety lock check
      setAllMotorsStatus(0);
    } else {
      Serial.println("All motors control blocked - Global auto mode is active");
    }
  }
  // Group control - Motors 1-2 forward
  else if (strcmp(method, "set_motors_12_fw") == 0) {
    controlMotorStatus(1, 1);
    controlMotorStatus(2, 1);
  }
  // Group control - Motors 1-2 reverse
  else if (strcmp(method, "set_motors_12_re") == 0) {
    controlMotorStatus(1, 2);
    controlMotorStatus(2, 2);
  }
  // Group control - Motors 1-2 off
  else if (strcmp(method, "set_motors_12_off") == 0) {
    controlMotorStatus(1, 0);
    controlMotorStatus(2, 0);
  }
  // Group control - Motors 3-4 forward
  else if (strcmp(method, "set_motors_34_fw") == 0) {
    controlMotorStatus(3, 1);
    controlMotorStatus(4, 1);
  }
  // Group control - Motors 3-4 reverse
  else if (strcmp(method, "set_motors_34_re") == 0) {
    controlMotorStatus(3, 2);
    controlMotorStatus(4, 2);
  }
  // Group control - Motors 3-4 off
  else if (strcmp(method, "set_motors_34_off") == 0) {
    controlMotorStatus(3, 0);
    controlMotorStatus(4, 0);
  }
  // Fan control (existing functionality)
  else if (strcmp(method, "set_fan_1_auto") == 0) {
    fan1Auto = params["enabled"].as<bool>();
    saveFanConfig();
  }
  else if (strcmp(method, "set_fan_1_on_time") == 0) {
    fan1OnTime = params["time"].as<unsigned long>() * 1000;
    saveFanConfig();
  }
  else if (strcmp(method, "set_fan_1_off_time") == 0) {
    fan1OffTime = params["time"].as<unsigned long>() * 1000;
    saveFanConfig();
  }
  else if (strcmp(method, "set_fan_2_auto") == 0) {
    fan2Auto = params["enabled"].as<bool>();
    saveFanConfig();
  }
  else if (strcmp(method, "set_fan_2_on_time") == 0) {
    fan2OnTime = params["time"].as<unsigned long>() * 1000;
    saveFanConfig();
  }
  else if (strcmp(method, "set_fan_2_off_time") == 0) {
    fan2OffTime = params["time"].as<unsigned long>() * 1000;
    saveFanConfig();
  }
  else if (strcmp(method, "set_fan_1_cmd") == 0) {
    if (!fan1Auto) {  // Only allow manual control if auto is off
      bool state = params["state"].as<bool>();
      digitalWrite(FAN_1_PIN, state ? HIGH : LOW);
      fan1State = state;
    }
  }
  else if (strcmp(method, "set_fan_2_cmd") == 0) {
    if (!fan2Auto) {  // Only allow manual control if auto is off
      bool state = params["state"].as<bool>();
      digitalWrite(FAN_2_PIN, state ? HIGH : LOW);
      fan2State = state;
    }
  }
}

// Helper function to control motor status with safety lock (NEW)
void controlMotorStatus(int motorNum, int status) {
  // Safety lock: Prevent manual control when global auto mode is active
  if (globalMotorAuto) {
    Serial.print("Motor ");
    Serial.print(motorNum);
    Serial.println(" manual control blocked - Global auto mode is active");
    return;
  }
  
  // Validate motor number (1-4)
  if (motorNum < 1 || motorNum > 4) {
    Serial.print("Invalid motor number: ");
    Serial.println(motorNum);
    return;
  }
  
  // Update status and control motor (motorNum is 1-based, array is 0-based)
  int index = motorNum - 1;
  motorStatus[index] = status;
  setMotorPins(motorFwPins[index], motorRePins[index], status);
  
  Serial.print("Motor ");
  Serial.print(motorNum);
  Serial.print(" set to status: ");
  Serial.println(status);
}

void controlMotor(int motorNum, int status) {
  // Legacy function - now calls controlMotorStatus
  controlMotorStatus(motorNum, status);
}

// Helper function to set all motors to a specific status
void setAllMotorsStatus(int status) {
  for (int i = 0; i < 4; i++) {
    motorStatus[i] = status;
    setMotorPins(motorFwPins[i], motorRePins[i], status);
  }
}

void checkAutoTimers() {
  unsigned long currentTime = millis();
  
  // Fan 1 auto timer
  if (fan1Auto) {
    if (fan1State) {
      // Fan is on, check if it's time to turn off
      if (currentTime - fan1LastAction >= fan1OnTime) {
        digitalWrite(FAN_1_PIN, LOW);
        fan1State = false;
        fan1LastAction = currentTime;
        Serial.println("Fan 1 auto OFF");
      }
    } else {
      // Fan is off, check if it's time to turn on
      if (currentTime - fan1LastAction >= fan1OffTime) {
        digitalWrite(FAN_1_PIN, HIGH);
        fan1State = true;
        fan1LastAction = currentTime;
        Serial.println("Fan 1 auto ON");
      }
    }
  }
  
  // Fan 2 auto timer
  if (fan2Auto) {
    if (fan2State) {
      if (currentTime - fan2LastAction >= fan2OnTime) {
        digitalWrite(FAN_2_PIN, LOW);
        fan2State = false;
        fan2LastAction = currentTime;
        Serial.println("Fan 2 auto OFF");
      }
    } else {
      if (currentTime - fan2LastAction >= fan2OffTime) {
        digitalWrite(FAN_2_PIN, HIGH);
        fan2State = true;
        fan2LastAction = currentTime;
        Serial.println("Fan 2 auto ON");
      }
    }
  }
  
  // Global motor auto timer (NEW)
  if (globalMotorAuto) {
    if (lastGlobalMotorState == 1) {
      // Motors are in forward, check if it's time to switch to reverse
      if (currentTime - lastGlobalAction >= globalFwTime) {
        setAllMotorsStatus(2);  // Set all motors to reverse
        lastGlobalMotorState = 2;
        lastGlobalAction = currentTime;
        Serial.println("Global motor auto: All motors REVERSE");
      }
    } else if (lastGlobalMotorState == 2) {
      // Motors are in reverse, check if it's time to switch to forward
      if (currentTime - lastGlobalAction >= globalReTime) {
        setAllMotorsStatus(1);  // Set all motors to forward
        lastGlobalMotorState = 1;
        lastGlobalAction = currentTime;
        Serial.println("Global motor auto: All motors FORWARD");
      }
    } else {
      // Initial state - start with forward
      setAllMotorsStatus(1);
      lastGlobalMotorState = 1;
      lastGlobalAction = currentTime;
      Serial.println("Global motor auto: Initial state - All motors FORWARD");
    }
  }
}

// Load global motor configuration from NVS (NEW)
void loadGlobalConfig() {
  globalMotorAuto = preferences.getBool("gMotorAuto", false);
  globalFwTime = preferences.getULong("gFwTime", 30000);
  globalReTime = preferences.getULong("gReTime", 30000);
  lastGlobalAction = millis();
  lastGlobalMotorState = 0;
  
  Serial.println("Global motor config loaded from NVS");
  Serial.print("  Auto: ");
  Serial.println(globalMotorAuto);
  Serial.print("  FW Time: ");
  Serial.println(globalFwTime);
  Serial.print("  RE Time: ");
  Serial.println(globalReTime);
}

// Save global motor configuration to NVS (NEW)
void saveGlobalConfig() {
  preferences.putBool("gMotorAuto", globalMotorAuto);
  preferences.putULong("gFwTime", globalFwTime);
  preferences.putULong("gReTime", globalReTime);
  
  Serial.println("Global motor config saved to NVS");
}

void loadFanConfig() {
  fan1Auto = preferences.getBool("fan1Auto", false);
  fan1OnTime = preferences.getULong("fan1OnTime", 30000);
  fan1OffTime = preferences.getULong("fan1OffTime", 30000);
  
  fan2Auto = preferences.getBool("fan2Auto", false);
  fan2OnTime = preferences.getULong("fan2OnTime", 30000);
  fan2OffTime = preferences.getULong("fan2OffTime", 30000);
  
  Serial.println("Fan config loaded from NVS");
}

void saveFanConfig() {
  preferences.putBool("fan1Auto", fan1Auto);
  preferences.putULong("fan1OnTime", fan1OnTime);
  preferences.putULong("fan1OffTime", fan1OffTime);
  
  preferences.putBool("fan2Auto", fan2Auto);
  preferences.putULong("fan2OnTime", fan2OnTime);
  preferences.putULong("fan2OffTime", fan2OffTime);
  
  Serial.println("Fan config saved to NVS");
}

void sendTelemetry() {
  JsonDocument doc;
  
  // Fan telemetry
  doc["fan1_state"] = fan1State;
  doc["fan1_auto"] = fan1Auto;
  doc["fan2_state"] = fan2State;
  doc["fan2_auto"] = fan2Auto;
  
  // Motor telemetry
  doc["motor1_status"] = motorStatus[0];
  doc["motor2_status"] = motorStatus[1];
  doc["motor3_status"] = motorStatus[2];
  doc["motor4_status"] = motorStatus[3];
  
  // Global motor auto telemetry
  doc["global_motor_auto"] = globalMotorAuto;
  doc["global_fw_time"] = globalFwTime / 1000;
  doc["global_re_time"] = globalReTime / 1000;
  
  // System telemetry
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;
  
  char buffer[512];
  serializeJson(doc, buffer);
  
  mqttClient.publish(TELEMETRY_TOPIC, buffer);
}

void checkOTAUpdate() {
  HTTPClient http;
  http.begin("https://raw.githubusercontent.com/anusitsrb-glitch/GreenHouse-8/main/config.json");
  
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      const char* version = doc["version"];
      const char* firmwareUrl = doc["firmware_url"];
      
      Serial.print("Current version: 17, Available version: ");
      Serial.println(version);
      
      // Check if update is available (simple version comparison)
      if (String(version).toInt() > 17) {
        Serial.println("Update available, downloading...");
        performOTAUpdate(firmwareUrl);
      }
    }
  }
  http.end();
}

void performOTAUpdate(const char* url) {
  HTTPClient http;
  http.begin(url);
  
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    
    if (contentLength > 0) {
      bool canBegin = Update.begin(contentLength);
      
      if (canBegin) {
        Serial.println("Starting OTA update...");
        WiFiClient* client = http.getStreamPtr();
        size_t written = Update.writeStream(*client);
        
        if (written == contentLength) {
          Serial.println("Written successfully");
        }
        
        if (Update.end()) {
          if (Update.isFinished()) {
            Serial.println("Update complete, rebooting...");
            ESP.restart();
          }
        }
      }
    }
  }
  http.end();
}
