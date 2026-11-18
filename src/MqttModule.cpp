#include "MqttModule.h"
#include "WifiModule.h"

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

namespace {
    const char* MQTT_HOST = "tf1f6239.ala.us-east-1.emqxsl.com";
    const int MQTT_PORT = 8883;
    const char* MQTT_USER = "4ngi";
    const char* MQTT_PASSWORD = "password";
    const char* MQTT_TOPIC = "longdistance/lamps/message";

    constexpr uint32_t MQTT_RETRY_MS = 5000;
    uint32_t lastMQTTRetry = 0;
    uint32_t mqttConnectedAt = 0;

    WiFiClientSecure secureClient;
    PubSubClient mqtt(secureClient);
    String deviceId;

    void (*remoteMessageHandler)(const char*) = nullptr;

    void onMqttMessage(char* /*topic*/, byte* payload,
                       unsigned int length) {
        String message;

        for (unsigned int i = 0; i < length; i++) {
            message += static_cast<char>(payload[i]);
        }

        // Ignora i nostri messaggi.
        const String prefix = deviceId + ":";
        if (message.startsWith(prefix)) return;

        const int separator = message.indexOf(':');
        if (separator <= 0) return;

        const String text = message.substring(separator + 1);
        if (text.length() == 0) return;

        Serial.print("Ricevuto: ");
        Serial.println(text);

        if (remoteMessageHandler != nullptr) {
            remoteMessageHandler(text.c_str());
        }
    }
}

bool Mqtt_isConnected() {
    return mqtt.connected();
}

void Mqtt_Begin(void (*messageHandler)(const char*)) {
    remoteMessageHandler = messageHandler;
    lastMQTTRetry = millis();

    deviceId = "lampada_" + String(esp_random(), HEX);

    Serial.print("Device ID: ");
    Serial.println(deviceId);

    // TLS senza verifica del certificato.
    secureClient.setInsecure();
    secureClient.setHandshakeTimeout(5);

    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCallback(onMqttMessage);
    mqtt.setSocketTimeout(5);
}

void Mqtt_Update() {
    if (!Wifi_isConnected()) return;

    if (!mqtt.connected()) {
        const uint32_t now = millis();

        if (now - lastMQTTRetry >= MQTT_RETRY_MS) {
            lastMQTTRetry = now;

            Serial.println("Connessione MQTT...");

            if (mqtt.connect(
                    deviceId.c_str(),
                    MQTT_USER,
                    MQTT_PASSWORD)) {
                mqttConnectedAt = millis();        
                Serial.println("MQTT connesso");

                if (!mqtt.subscribe(MQTT_TOPIC)) {
                    Serial.println("Errore sottoscrizione MQTT");
                }
            } else {
                Serial.print("MQTT errore: ");
                Serial.println(mqtt.state());
            }
        }

        return;
    }

    mqtt.loop();
}

bool Mqtt_SendMessage(const char* text) {
    if (!mqtt.connected()) {
        Serial.println("MQTT non connesso");
        return false;
    }

    const String message = deviceId + ":" + text;

    if (mqtt.publish(MQTT_TOPIC, message.c_str())) {
        Serial.print("Inviato: ");
        Serial.println(text);
        return true;
    } else {
        Serial.println("Errore invio MQTT");
        return false;
    }

    return false;
}

bool Mqtt_HasBeen(uint32_t time) {
    const uint32_t now = millis();
    return Wifi_isConnected() && Mqtt_isConnected()
        && now - mqttConnectedAt >= time;
}