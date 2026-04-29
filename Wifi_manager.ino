#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <HTTPClient.h>

// =========================
// WIFI CONFIG
// =========================
const char* WIFI_SSID = "EMTGAMER";
const char* WIFI_PASS = "Emmett2005";

// =========================
// APPS SCRIPT URL
// =========================
const char* GOOGLE_SCRIPT_URL =
    "https://script.google.com/macros/s/AKfycbw5JbiSfzJhIHPn1I4d0cIXz8iF1lGFIk1b8cv40EsMU6rOE7BhfWH1n2jMN-kcEAr2/exec";

// =========================
// CONNECT WIFI
// =========================
void connectWiFi() {

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    Serial.print("Connecting WiFi");

    unsigned long startAttempt = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - startAttempt < 20000) {

        Serial.print(".");
        delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWiFi Failed!");
    }
}

// =========================
// SEND DATA TO APPS SCRIPT
// =========================
void sendToGoogleSheet(
    long distance,
    float rpm,
    int speedA,
    int speedB,
    bool blocked
) {

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Disconnected");
        return;
    }

    HTTPClient http;

    String url = String(GOOGLE_SCRIPT_URL);

    url += "?distance=" + String(distance);
    url += "&rpm=" + String(rpm, 1);
    url += "&speedA=" + String(speedA);
    url += "&speedB=" + String(speedB);
    url += "&blocked=" + String(blocked);

    http.begin(url);

    int httpCode = http.GET();

    if (httpCode > 0) {
        Serial.print("[HTTP] Code: ");
        Serial.println(httpCode);

        String payload = http.getString();
        Serial.println(payload);
    } else {
        Serial.print("[HTTP] Failed: ");
        Serial.println(http.errorToString(httpCode));
    }

    http.end();
}

#endif