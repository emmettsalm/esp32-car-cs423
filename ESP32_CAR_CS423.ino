#include <Bluepad32.h>

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
// LEDC CHANNEL MAP
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
const int STOP_DIST_CM = 20;
const int WARN_DIST_CM = 30;
bool frontBlocked      = false;
bool beepState         = false;
unsigned long lastBeepToggleMs = 0;

// ──────────────────────────────────────────────
// STARTUP MELODY (non-blocking)
// ──────────────────────────────────────────────
const int startupNotes[]     = {784, 1047, 1319};
const int startupDurations[] = {80,  80,   120};
const int startupCount = 3;
int  startupIndex      = 0;
bool playingStartup    = true;
unsigned long startupNextMs = 0;

// ──────────────────────────────────────────────
// CONNECT SOUND (non-blocking)
// ──────────────────────────────────────────────
const int connectNotes[]     = {220, 0, 330, 0, 494, 659};
const int connectDurations[] = {120, 60, 100, 40,  90, 200};
const int connectCount = 6;
int  connectIndex      = -1;
unsigned long connectNextMs = 0;

// ──────────────────────────────────────────────
// LIGHTS
// ──────────────────────────────────────────────
int  headlightMode  = 0;
bool underglowState = false;

// ──────────────────────────────────────────────
// BUTTON STATE
// ──────────────────────────────────────────────
bool prevSquare   = false;
bool prevTriangle = false;
bool prevCircle   = false;

ControllerPtr myControllers[BP32_MAX_GAMEPADS];

// ══════════════════════════════════════════════
// BUZZER
// ══════════════════════════════════════════════
void buzzerTone(int freq) {
    if (freq <= 0) {
        ledcWrite(CH_BUZZER, 0);
        return;
    }
    ledcSetup(CH_BUZZER, freq, BUZZER_RES);
    ledcAttachPin(BUZZER_PIN, CH_BUZZER);
    ledcWrite(CH_BUZZER, 512);
}

void buzzerOff() {
    ledcWrite(CH_BUZZER, 0);
}

// ──────────────────────────────────────────────
// STARTUP MELODY
// ──────────────────────────────────────────────
void handleStartupMelody() {
    if (!playingStartup) return;
    if (millis() < startupNextMs) return;

    if (startupIndex < startupCount) {
        buzzerTone(startupNotes[startupIndex]);
        startupNextMs = millis() + startupDurations[startupIndex] + 20;
        startupIndex++;
    } else {
        buzzerOff();
        playingStartup = false;
    }
}

// ──────────────────────────────────────────────
// CONNECT SOUND
// ──────────────────────────────────────────────
void startConnectSound() {
    connectIndex  = 0;
    connectNextMs = 0;
}

void handleConnectSound() {
    if (connectIndex < 0) return;
    if (millis() < connectNextMs) return;

    if (connectIndex < connectCount) {
        int note = connectNotes[connectIndex];
        if (note == 0) buzzerOff();   // rest
        else buzzerTone(note);
        connectNextMs = millis() + connectDurations[connectIndex];
        connectIndex++;
    } else {
        buzzerOff();
        connectIndex = -1;
    }
}

// ══════════════════════════════════════════════
// MOTOR CONTROL
// ══════════════════════════════════════════════
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

// ══════════════════════════════════════════════
// ULTRASONIC
// ══════════════════════════════════════════════
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

// ══════════════════════════════════════════════
// BUZZER HANDLER (parking sensor style)
// ══════════════════════════════════════════════
void handleBuzzer(long dist) {
    if (playingStartup || connectIndex >= 0) return;

    frontBlocked = (dist > 0 && dist <= STOP_DIST_CM);

    if (dist == 999 || dist > WARN_DIST_CM) {
        buzzerOff();
        beepState = false;
        return;
    }

    if (dist <= STOP_DIST_CM) {
        // ต่อเนื่อง ไม่บี๊บ
        buzzerTone(1200);
        return;
    }

    int interval = map(dist, STOP_DIST_CM, WARN_DIST_CM, 300, 1500);
    interval = constrain(interval, 300, 1500);

    unsigned long now = millis();
    if (now - lastBeepToggleMs >= (unsigned long)interval) {
        lastBeepToggleMs = now;
        beepState = !beepState;
        if (beepState) buzzerTone(900);
        else buzzerOff();
    }
}

// ══════════════════════════════════════════════
// LIGHTS
// ══════════════════════════════════════════════
void applyHeadlight() {
    const int levels[] = {0, 80, 255};
    ledcWrite(CH_HEADLIGHT, levels[headlightMode]);
}

void applyUnderglow() {
    digitalWrite(UNDERGLOW_PIN, underglowState ? HIGH : LOW);
}

void updateRearLight() {
    bool reversing = (currentSpeedA < 0 || currentSpeedB < 0);
    digitalWrite(REAR_LIGHT_PIN, reversing ? HIGH : LOW);
}

// ══════════════════════════════════════════════
// GAMEPAD
// ══════════════════════════════════════════════
void processGamepad(ControllerPtr ctl) {
    bool isBraking = ctl->a();

    bool squareNow = ctl->x();
    if (squareNow) {
        if (!playingStartup && connectIndex < 0) {
            buzzerTone(550);
        }
    } else {
        if (!playingStartup && connectIndex < 0) {
            buzzerOff();
        }
    }
    prevSquare = squareNow;

    // △ ไฟหน้า
    bool triNow = ctl->y();
    if (triNow && !prevTriangle) {
        headlightMode = (headlightMode + 1) % 3;
        applyHeadlight();
    }
    prevTriangle = triNow;

    bool circleNow = ctl->b();
    if (circleNow && !prevCircle) {
        underglowState = !underglowState;
        applyUnderglow();
    }
    prevCircle = circleNow;

    int yAxis = ctl->axisY();
    int xAxis = ctl->axisX();

    if (isBraking) {
        currentSpeedA = currentSpeedB = 0;

    } else if (frontBlocked) {
        int spd  = map(yAxis, -511, 512, MAX_SPEED, -MAX_SPEED);
        int turn = map(xAxis, -511, 512, -MAX_SPEED, MAX_SPEED);

        if (yAxis < -DEADZONE) {
            currentSpeedA = currentSpeedB = spd;
        } else if (abs(xAxis) >= DEADZONE) {
            currentSpeedA = -turn;
            currentSpeedB =  turn;
        } else {
            currentSpeedA = currentSpeedB = 0;
        }

    } else {
        if (abs(yAxis) < DEADZONE && abs(xAxis) < DEADZONE) {
            currentSpeedA = currentSpeedB = 0;

        } else if (abs(yAxis) >= abs(xAxis)) {
            int spd = map(yAxis, -511, 512, MAX_SPEED, -MAX_SPEED);
            currentSpeedA = currentSpeedB = spd;

        } else {
            int turn = map(xAxis, -511, 512, -MAX_SPEED, MAX_SPEED);
            currentSpeedA = -turn;
            currentSpeedB =  turn;
        }
    }

    setMotorA(currentSpeedA);
    setMotorB(currentSpeedB);
    updateRearLight();
}

// ══════════════════════════════════════════════
// SETUP
// ══════════════════════════════════════════════
void setup() {
    Serial.begin(115200);

    ledcSetup(CH_AIN1, PWM_FREQ, PWM_RES);
    ledcSetup(CH_AIN2, PWM_FREQ, PWM_RES);
    ledcSetup(CH_BIN1, PWM_FREQ, PWM_RES);
    ledcSetup(CH_BIN2, PWM_FREQ, PWM_RES);
    ledcAttachPin(AIN1, CH_AIN1);
    ledcAttachPin(AIN2, CH_AIN2);
    ledcAttachPin(BIN1, CH_BIN1);
    ledcAttachPin(BIN2, CH_BIN2);

    ledcSetup(CH_HEADLIGHT, PWM_FREQ, PWM_RES);
    ledcAttachPin(HEADLIGHT_PIN, CH_HEADLIGHT);

    ledcSetup(CH_BUZZER, 1000, BUZZER_RES);
    ledcAttachPin(BUZZER_PIN, CH_BUZZER);
    ledcWrite(CH_BUZZER, 0);

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(UNDERGLOW_PIN, OUTPUT);
    pinMode(REAR_LIGHT_PIN, OUTPUT);

    applyHeadlight();
    applyUnderglow();
    updateRearLight();

    BP32.setup(&onConnectedController, &onDisconnectedController);
}

// ══════════════════════════════════════════════
// CALLBACK
// ══════════════════════════════════════════════
void onConnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (!myControllers[i]) {
            myControllers[i] = ctl;
            startConnectSound();
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

// ══════════════════════════════════════════════
// LOOP
// ══════════════════════════════════════════════
void loop() {
    handleStartupMelody();
    handleConnectSound();

    BP32.update();

    for (auto ctl : myControllers) {
        if (ctl && ctl->isConnected()) {
            processGamepad(ctl);
            break;
        }
    }

    long dist = measureDistanceCm();
    handleBuzzer(dist);

    delay(10);
}
