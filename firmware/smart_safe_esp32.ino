#include <WiFi.h>
#include <Wire.h>
#include <Keypad.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <PubSubClient.h>
#include <math.h>

const char *WIFI_SSID = "Wokwi-GUEST";
const char *WIFI_PASSWORD = "";
const char *MQTT_HOST = "broker.emqx.io";
const int MQTT_PORT = 1883;
const char *DATA_TOPIC = "smartsafe/groupX/data";
const char *COMMAND_TOPIC = "smartsafe/groupX/cmd";

const int SERVO_PIN = 18;
const int BUZZER_PIN = 4;
const int RED_LED_PIN = 5;
const int GREEN_LED_PIN = 19;
const float VIBRATION_THRESHOLD = 1.5f;
const unsigned long STATUS_INTERVAL_MS = 2000;

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 25, 33, 32};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

Adafruit_MPU6050 mpu;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
Servo lockServo;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

const char *const DEFAULT_PIN = "1234";
String enteredPin;
String lastEvent = "Brankas siap";
int pinErrors = 0;
bool isUnlocked = false;
bool alarmOn = false;
bool vibrationDetected = false;
int vibrationCounter = 0;          // ← NEW: debounce getaran
unsigned long lastStatusAt = 0;
unsigned long lastLedToggleAt = 0;
bool redLedState = false;

// ---------- Display ----------
void updateDisplay(const String &message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("SMART SAFE");
  display.println("----------------");
  display.println(message);
  display.print("PIN: ");
  display.print(enteredPin);
  display.display();
}

// ---------- MQTT Publish ----------
void publishStatus() {
  if (!mqttClient.connected()) return;

  String payload = "{\"lock\":\"";
  payload += isUnlocked ? "UNLOCKED" : "LOCKED";
  payload += "\",\"vibration\":\"";
  payload += vibrationDetected ? "DETECTED" : "NORMAL";
  payload += "\",\"alarm\":\"";
  payload += alarmOn ? "ON" : "OFF";
  payload += "\",\"pin_errors\":";
  payload += String(pinErrors);
  payload += ",\"last_event\":\"";
  payload += lastEvent;
  payload += "\"}";
  mqttClient.publish(DATA_TOPIC, payload.c_str());
  Serial.print("MQTT publish: ");
  Serial.println(payload);
}

// ---------- Safe Actions ----------
void unlockSafe(const String &event) {
  isUnlocked = true;
  lockServo.write(90);
  digitalWrite(GREEN_LED_PIN, HIGH);
  lastEvent = event;
  updateDisplay("Brankas terbuka");
  publishStatus();
}

void lockSafe() {
  isUnlocked = false;
  lockServo.write(0);
  digitalWrite(GREEN_LED_PIN, LOW);
  lastEvent = "Brankas dikunci";
  updateDisplay(lastEvent);
  publishStatus();
}

void resetAlarm() {
  alarmOn = false;
  pinErrors = 0;
  vibrationCounter = 0;
  vibrationDetected = false;
  noTone(BUZZER_PIN);
  digitalWrite(RED_LED_PIN, LOW);
  redLedState = false;
  lastEvent = "Alarm direset";
  updateDisplay(lastEvent);
  publishStatus();
}

// ---------- PIN Processing ----------
void processPin() {
  if (enteredPin == DEFAULT_PIN) {
    pinErrors = 0;
    alarmOn = false;                     // ← NEW: PIN benar mematikan alarm
    noTone(BUZZER_PIN);
    digitalWrite(RED_LED_PIN, LOW);
    unlockSafe("PIN Benar");
    Serial.println("PIN BENAR - Brankas terbuka");
  } else {
    pinErrors++;
    lastEvent = "PIN Salah";
    updateDisplay("PIN salah");
    Serial.print("PIN SALAH. Total errors: ");
    Serial.println(pinErrors);

    if (pinErrors >= 3) {
      alarmOn = true;
      lastEvent = "Alarm: PIN salah 3 kali";
      tone(BUZZER_PIN, 2000);            // ← NEW: bunyi instan!
      publishStatus();
      Serial.println("ALARM AKTIF! Buzzer ON");
    }
  }
  enteredPin = "";
}

// ---------- Keypad ----------
void handleKeypad() {
  char key = keypad.getKey();
  if (!key) return;

  Serial.print("Key pressed: ");
  Serial.println(key);

  if (key == '#') {
    processPin();
  } else if (key == '*') {
    enteredPin = "";
    updateDisplay("Masukkan PIN");
  } else if (key >= '0' && key <= '9' && enteredPin.length() < 8) {
    enteredPin += key;
    updateDisplay("Masukkan PIN");
  }
}

// ---------- Vibration (dengan debounce) ----------
void checkVibration() {
  sensors_event_t acceleration, gyro, temperature;
  if (!mpu.getEvent(&acceleration, &gyro, &temperature)) return;

  const float magnitude = sqrt(
    acceleration.acceleration.x * acceleration.acceleration.x +
    acceleration.acceleration.y * acceleration.acceleration.y +
    acceleration.acceleration.z * acceleration.acceleration.z
  );
  const bool detected = fabs(magnitude - 9.81f) > VIBRATION_THRESHOLD;

  // Debounce: butuh 2x berturut-turut baru dianggap getaran nyata
  if (detected) {
    vibrationCounter++;
    if (vibrationCounter >= 2 && !vibrationDetected) {
      vibrationDetected = true;
      if (!alarmOn) {                    // ← NEW: jangan override alarm PIN
        alarmOn = true;
        lastEvent = "Getaran terdeteksi";
        tone(BUZZER_PIN, 2000);          // ← NEW: bunyi instan
        publishStatus();
        Serial.println("GETARAN TERDETEKSI! Alarm ON");
      }
    }
  } else {
    vibrationCounter = 0;
    vibrationDetected = false;
  }
}

// ---------- MQTT Callback ----------
void mqttCallback(char *topic, byte *payload, unsigned int length) {
  if (String(topic) != COMMAND_TOPIC) return;

  String command;
  for (unsigned int i = 0; i < length; i++) command += (char)payload[i];
  command.trim();

  Serial.print("MQTT command received: ");
  Serial.println(command);

  if (!command.startsWith("{") || !command.endsWith("}")) return;

  const int actionKey = command.indexOf("\"action\"");
  const int colon = actionKey < 0 ? -1 : command.indexOf(':', actionKey + 8);
  if (colon < 0) return;

  int valueStart = colon + 1;
  while (valueStart < command.length() &&
         (command[valueStart] == ' ' || command[valueStart] == '\t' ||
          command[valueStart] == '\r' || command[valueStart] == '\n')) {
    valueStart++;
  }
  if (valueStart >= command.length() || command[valueStart] != '"') return;

  const int valueEnd = command.indexOf('"', valueStart + 1);
  if (valueEnd < 0) return;
  const String action = command.substring(valueStart + 1, valueEnd);

  if (action == "UNLOCK") {
    unlockSafe("Dibuka dari dashboard");
  } else if (action == "LOCK") {
    lockSafe();
  } else if (action == "RESET_ALARM") {
    resetAlarm();
  }
}

// ---------- Connection ----------
void connectWiFi() {
  Serial.printf("Connecting to WiFi %s...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(250);
  Serial.print("WiFi connected; IP: ");
  Serial.println(WiFi.localIP());
}

void connectMqtt() {
  while (!mqttClient.connected()) {
    String clientId = "smart-safe-";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("Connected to MQTT broker");
      if (!mqttClient.subscribe(COMMAND_TOPIC)) {
        Serial.printf("MQTT subscribe failed: %s\n", COMMAND_TOPIC);
      } else {
        Serial.printf("Subscribed to %s\n", COMMAND_TOPIC);
      }
      publishStatus();
    } else {
      Serial.printf("MQTT failed, state=%d; retry\n", mqttClient.state());
      delay(2000);
    }
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);

  lockServo.attach(SERVO_PIN);
  lockServo.write(0);

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
  } else {
    updateDisplay("Menginisialisasi...");
  }
  if (!mpu.begin()) {
    Serial.println("MPU6050 init failed");
  } else {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  connectWiFi();
  connectMqtt();
  lastStatusAt = millis();
  updateDisplay("Masukkan PIN");
}

// ---------- Loop ----------
void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqttClient.connected()) connectMqtt();
  mqttClient.loop();

  handleKeypad();
  checkVibration();

  if (alarmOn) {
    tone(BUZZER_PIN, 2000);
    if (millis() - lastLedToggleAt >= 250) {
      lastLedToggleAt = millis();
      redLedState = !redLedState;
      digitalWrite(RED_LED_PIN, redLedState ? HIGH : LOW);
    }
  } else {
    noTone(BUZZER_PIN);
    digitalWrite(RED_LED_PIN, LOW);
  }

  if (millis() - lastStatusAt >= STATUS_INTERVAL_MS) {
    lastStatusAt = millis();
    publishStatus();
  }
}