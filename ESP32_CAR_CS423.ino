#include <Bluepad32.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ──────────────────────────────────────────────
// WIFI CONFIG
// ──────────────────────────────────────────────
const char* ssid = "EMTGAMER";
const char* password = "Emmett2005";
String scriptURL = "https://script.google.com/macros/s/AKfycbwz5BA7ygQentplMFGlO5JUhpHoFr_tyNH_0CKBdeSScVcfwGQTp3chHEidTOgjJB3y/exec";

// ──────────────────────────────────────────────
// PIN MAP
// ──────────────────────────────────────────────
const int AIN1 = 22;
const int AIN2 = 23;
const int BIN1 = 32;
const int BIN2 = 33;

const int TRIG_PIN       = 12;
const int ECHO_PIN       = 13;
const int BUZZER_PIN     = 16;
const int HEADLIGHT_PIN  = 4;
const int UNDERGLOW_PIN  = 2;
const int REAR_LIGHT_PIN = 15;

// ──────────────────────────────────────────────
// LEDC
// ──────────────────────────────────────────────
#define CH_AIN1      0
#define CH_AIN2      1
#define CH_BIN1      2
#define CH_BIN2      3
#define CH_HEADLIGHT 4
#define CH_BUZZER    5

#define PWM_FREQ     20000
#define PWM_RES      8
#define BUZZER_RES   10

// ──────────────────────────────────────────────
// MOTOR
// ──────────────────────────────────────────────
const int MAX_SPEED = 200;
const int DEADZONE  = 80;
int currentSpeedA   = 0;
int currentSpeedB   = 0;

// ──────────────────────────────────────────────
// ULTRASONIC
// ──────────────────────────────────────────────
long measureDistanceCm() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long dur = pulseIn(ECHO_PIN, HIGH, 20000);
    if (dur == 0) return 999;
    return dur / 58;
}

// ──────────────────────────────────────────────
// WIFI
// ──────────────────────────────────────────────
void connectWiFi() {
    WiFi.begin(ssid, password);
    Serial.print("Connecting WiFi");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\n✅ WiFi Connected");
    Serial.println(WiFi.localIP());
}

void sendDistance(long distance) {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    String url = scriptURL + "?distance=" + String(distance);

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
        Serial.println("📡 Sent");
    } else {
        Serial.println("❌ Send fail");
    }

    http.end();
}

// ──────────────────────────────────────────────
// MOTOR CONTROL
// ──────────────────────────────────────────────
void setMotorA(int speed) {
    speed = constrain(speed, -255, 255);
    if (speed > 0) {
        ledcWrite(CH_AIN1, speed);
        ledcWrite(CH_AIN2, 0);
    } else if (speed < 0) {
        ledcWrite(CH_AIN1, 0);
        ledcWrite(CH_AIN2, -speed);
    } else {
        ledcWrite(CH_AIN1, 0);
        ledcWrite(CH_AIN2, 0);
    }
}

void setMotorB(int speed) {
    speed = constrain(speed, -255, 255);
    speed = -speed;

    if (speed > 0) {
        ledcWrite(CH_BIN1, speed);
        ledcWrite(CH_BIN2, 0);
    } else if (speed < 0) {
        ledcWrite(CH_BIN1, 0);
        ledcWrite(CH_BIN2, -speed);
    } else {
        ledcWrite(CH_BIN1, 0);
        ledcWrite(CH_BIN2, 0);
    }
}

// ──────────────────────────────────────────────
// GAMEPAD
// ──────────────────────────────────────────────
ControllerPtr myControllers[BP32_MAX_GAMEPADS];

void processGamepad(ControllerPtr ctl) {
    int yAxis = ctl->axisY();
    int xAxis = ctl->axisX();

    if (abs(yAxis) < DEADZONE && abs(xAxis) < DEADZONE) {
        currentSpeedA = currentSpeedB = 0;

    } else if (abs(yAxis) >= abs(xAxis)) {
        int spd = map(yAxis, -511, 512, MAX_SPEED, -MAX_SPEED);
        currentSpeedA = currentSpeedB = spd;

    } else {
        int turn = map(xAxis, -511, 512, -MAX_SPEED, MAX_SPEED);
        currentSpeedA = -turn;
        currentSpeedB = turn;
    }

    setMotorA(currentSpeedA);
    setMotorB(currentSpeedB);

    // ไฟถอย
    bool reversing = (currentSpeedA < 0 || currentSpeedB < 0);
    digitalWrite(REAR_LIGHT_PIN, reversing ? HIGH : LOW);
}

// ──────────────────────────────────────────────
// CALLBACK
// ──────────────────────────────────────────────
void onConnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (!myControllers[i]) {
            myControllers[i] = ctl;
            break;
        }
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == ctl) {
            myControllers[i] = nullptr;
        }
    }
}

// ──────────────────────────────────────────────
// SETUP
// ──────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    connectWiFi();

    ledcSetup(CH_AIN1, PWM_FREQ, PWM_RES);
    ledcSetup(CH_AIN2, PWM_FREQ, PWM_RES);
    ledcSetup(CH_BIN1, PWM_FREQ, PWM_RES);
    ledcSetup(CH_BIN2, PWM_FREQ, PWM_RES);

    ledcAttachPin(AIN1, CH_AIN1);
    ledcAttachPin(AIN2, CH_AIN2);
    ledcAttachPin(BIN1, CH_BIN1);
    ledcAttachPin(BIN2, CH_BIN2);

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(REAR_LIGHT_PIN, OUTPUT);

    BP32.setup(&onConnectedController, &onDisconnectedController);
}

// ──────────────────────────────────────────────
// LOOP
// ──────────────────────────────────────────────
void loop() {
    BP32.update();

    for (auto ctl : myControllers) {
        if (ctl && ctl->isConnected()) {
            processGamepad(ctl);
            break;
        }
    }

    // ───── ส่งเฉพาะตอนถอย ─────
    static unsigned long lastSend = 0;

    if (millis() - lastSend > 1500) {
        lastSend = millis();

        bool reversing = (currentSpeedA < 0 || currentSpeedB < 0);

        if (reversing) {
            long dist = measureDistanceCm();

            Serial.print("🔙 Distance: ");
            Serial.println(dist);

            sendDistance(dist);
        }
    }

    // ───── WiFi Check ─────
    static unsigned long wifiCheck = 0;
    if (millis() - wifiCheck > 5000) {
        wifiCheck = millis();
        Serial.println(WiFi.status() == WL_CONNECTED ? "📶 WiFi OK" : "❌ WiFi Lost");
    }

    // ───── Bluetooth Check ─────
    static unsigned long btCheck = 0;
    if (millis() - btCheck > 3000) {
        btCheck = millis();

        bool connected = false;
        for (auto ctl : myControllers) {
            if (ctl && ctl->isConnected()) connected = true;
        }

        Serial.println(connected ? "🎮 BT OK" : "❌ BT Lost");
    }

    delay(10);
}
