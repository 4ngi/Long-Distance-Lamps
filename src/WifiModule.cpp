#include "WifiModule.h"

#include <Arduino.h>
#include <WiFi.h>

namespace {
    struct WifiNetwork {
        const char* ssid;
        const char* password;
    };

    const WifiNetwork NETWORKS[] = {
        //{"Angelo wifi", "password"},
        //{"SKYWIFI_YICIH", "xFYIS25RBB7j"},
        {"Vodafone-C01260028", "phRkxPh6RFdep3Yd"}
    };

    bool connectionReported = false;
    constexpr size_t NETWORK_COUNT = sizeof(NETWORKS) / sizeof(NETWORKS[0]);
    constexpr uint32_t WIFI_RETRY_MS = 5000;

    size_t nextNetwork = 0;
    uint32_t lastWiFiAttempt = 0;

    void tryNextNetwork() {
        const WifiNetwork& network = NETWORKS[nextNetwork];
        nextNetwork = (nextNetwork + 1) % NETWORK_COUNT;

        Serial.print("Connessione WiFi a: ");
        Serial.println(network.ssid);

        lastWiFiAttempt = millis();
        WiFi.begin(network.ssid, network.password);
    }
}

void Wifi_Begin() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);

    nextNetwork = 0;
    tryNextNetwork();
}

bool Wifi_isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void Wifi_Update() {
    if (Wifi_isConnected()) {
        if (!connectionReported) {
            connectionReported = true;

            Serial.println("\n--- WiFi connesso ---");
            Serial.print("SSID: ");
            Serial.println(WiFi.SSID());

            Serial.print("ESP IP: ");
            Serial.println(WiFi.localIP());

            Serial.print("Subnet: ");
            Serial.println(WiFi.subnetMask());

            Serial.print("Gateway: ");
            Serial.println(WiFi.gatewayIP());

            Serial.println("--------------------");
        }
        return;
    }

    connectionReported = false;
    
    const uint32_t now = millis();

    if (now - lastWiFiAttempt >= WIFI_RETRY_MS) {
        WiFi.disconnect(false, false, 0);
        tryNextNetwork();
    }
}