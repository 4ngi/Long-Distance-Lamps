#include "LampModule.h"
#include "WifiModule.h"
#include <EspWizLight.h>

namespace {

    constexpr uint32_t SEARCH_INTERVAL_MS = 5000;
    constexpr uint32_t CHECK_INTERVAL_MS = 60000;
    uint32_t lastSearch = 0;
    uint32_t lastCheck = 0;

    constexpr uint32_t CLAIM_TIMEOUT_MS = 20000;
    
    constexpr uint32_t PULSE_STEP_MS = 20;
    constexpr uint32_t PULSE_HOLD_MS = 250;
    constexpr int PULSE_MIN = 10;
    constexpr int PULSE_MAX = 80;

    constexpr int MAX_ERRORS = 10;
    constexpr int MAX_LAMPS = 10;
    bool connected = false;
    int consecutiveErrors = 0;

    constexpr WizScene DIM_TO_WARM = static_cast<WizScene>(40);
    constexpr int DEFAULT_MANUAL_TEMPERATURE = 2600;
    WizLight lampada;
    int manualTemperature = DEFAULT_MANUAL_TEMPERATURE;

    struct Claim {
        String mac;
        uint32_t lastSeen;
    };
    Claim claims[MAX_LAMPS];
    int claimCount = 0;




    /*****************PRIVATE FUNCTIONS************************/

    //getters
    bool isConnected() {
        return Wifi_isConnected() && connected;
    }

    //claims
    bool isTaken(const String& mac) {
        for (int i = 0; i < claimCount; i++) {
            if (claims[i].mac == mac)
                return true;
        }
        return false;
    }

    void removeOldClaims(uint32_t now) {
        for (int i = 0; i < claimCount;) {
            if (now - claims[i].lastSeen >= CLAIM_TIMEOUT_MS) {
                claims[i] = claims[--claimCount];
            } else {
                i++;
            }
        }
    }


    void disconnect() {
        connected = false;
        consecutiveErrors = 0;
        lastSearch = millis() - SEARCH_INTERVAL_MS;

        Serial.println("Lampadina offline");
    }

    int sendConfig() {
        if (!Wifi_isConnected() || !connected)
            return -1;

        return lampada.pushConfig() == SUCCESS ? 0 : -1;
    }

    bool rampTo(int target) {
        int current = max(PULSE_MIN, lampada.getDimming());

        while (current != target) {
            if (current < target)
                current = min(current + 5, target);
            else
                current = max(current - 5, target);

            lampada.setDimming(current);

            if (sendConfig() == -1)
                return false;

            delay(PULSE_STEP_MS);
        }

        return true;
    }

    void showConnection() {
        const LightConfig previous = lampada.getConfig();

        lampada.setDimming(PULSE_MIN);
        lampada.setState(true);
        if (!rampTo(PULSE_MAX))
            Serial.println("Effetto di connessione: aumento luminosita fallito");

        delay(PULSE_HOLD_MS);

        if (!rampTo(PULSE_MIN))
            Serial.println("Effetto di connessione: diminuzione luminosita fallita");

        lampada.setConfig(previous);
        if (sendConfig() != 0)
            Serial.println("Ripristino dopo effetto di connessione fallito");
    }

    void checkAssignedLamp(uint32_t now) {
        if (now - lastCheck < CHECK_INTERVAL_MS)
            return;

        lastCheck = now;

        if (lampada.pullConfig() == SUCCESS) {
            consecutiveErrors = 0;
            return;
        }

        consecutiveErrors++;
        Serial.println("Controllo della lampadina assegnata fallito");

        if (consecutiveErrors >= MAX_ERRORS)
            disconnect();
    }

    void discoverFreeLamp(uint32_t now) {
        if (now - lastSearch < SEARCH_INTERVAL_MS)
            return;

        lastSearch = now;
        Serial.println("Cerco lampadine libere via broadcast...");

        WizLight trovate[MAX_LAMPS];
        const int count = WizLight::discoverLights(trovate, MAX_LAMPS);

        for (int i = 0; i < count; i++) {
            const String mac = trovate[i].getMac();

            if (mac.isEmpty())
                continue;

            if (isTaken(mac)) {
                Serial.printf(
                    "Ignoro lampadina occupata: %s\n",
                    mac.c_str()
                );
                continue;
            }

            if (trovate[i].pullConfig() != SUCCESS) {
                Serial.println("Lettura configurazione della lampadina trovata fallita");
                continue;
            }

            lampada = trovate[i];
            const int kelvin = lampada.getTemperature();
            manualTemperature = kelvin > 0
                ? constrain(kelvin, 2700, 6500)
                : DEFAULT_MANUAL_TEMPERATURE;

            connected = true;
            consecutiveErrors = 0;
            lastCheck = millis();

            Serial.print("Lampadina assegnata: ");
            Serial.println(lampada.getIP());

            showConnection();
            return;
        }
    }
}

void Lamp_Begin() {
    manualTemperature = DEFAULT_MANUAL_TEMPERATURE;

    connected = false;

    claimCount = 0;
    consecutiveErrors = 0;

    lastSearch = millis() - SEARCH_INTERVAL_MS;
    lastCheck = millis();
}

void Lamp_Update(bool canDiscover) {
    if (!Wifi_isConnected()) {
        connected = false;
        consecutiveErrors = 0;
        lastSearch = millis() - SEARCH_INTERVAL_MS;
        return;
    }

    const uint32_t now = millis();
    removeOldClaims(now);

    if (connected) {
        checkAssignedLamp(now);
        return;
    }

    if (canDiscover)
        discoverFreeLamp(now);
}

bool Lamp_isOn() {
    return isConnected() && lampada.getState();
}

bool Lamp_IsDimToWarm() {
    const LightConfig config = lampada.getConfig();

    return isConnected()
        && config.mode == SCENE_MODE
        && config.scene == DIM_TO_WARM;
}

int Lamp_Toggle() {
    if (!isConnected())
        return -1;

    lampada.setState(!lampada.getState());
    return sendConfig();
}

int Lamp_ToggleDimToWarm() {
    if (!isConnected())
        return -1;

    // Legge anche eventuali cambiamenti fatti dall'app.
    if (lampada.pullConfig() != SUCCESS || !lampada.getState())
        return -1;

    int restoreTemperature = manualTemperature;

    if (Lamp_IsDimToWarm()) {
        // Disattiva realmente la scena ripristinando il bianco manuale.
        lampada.setTemperature(restoreTemperature);
    } else {
        const int kelvin = lampada.getTemperature();

        if (kelvin > 0)
            restoreTemperature = constrain(kelvin, 2700, 6500);

        lampada.setScene(DIM_TO_WARM);
    }

    manualTemperature = restoreTemperature;
    return sendConfig();
}

int Lamp_AdjustTemperature(int direction) {
    if (!Lamp_isOn() || Lamp_IsDimToWarm())
        return -1;

    const int current = lampada.getTemperature();
    const int base = current > 0 ? current : manualTemperature;
    const int target = constrain(base + direction * 38, 2700, 6500);

    if (target == current)
        return 0;

    lampada.setTemperature(target);
    manualTemperature = target;
    return sendConfig();
}

int Lamp_AdjustBrightness(int direction) {
    if (!Lamp_isOn())
        return -1;

    const int current = lampada.getDimming();
    const int target = constrain(current + direction, 10, 75);

    if (target == current)
        return 0;

    lampada.setDimming(target);
    return sendConfig();
}

int Lamp_Pulse() {

    if (!Lamp_isOn())
        return -1;

    bool wasOn = lampada.getState();
    int original = max(PULSE_MIN, lampada.getDimming());

    lampada.setState(true);
    if (sendConfig() == -1) return -1;

    if (original < 40) {
        if (!rampTo(PULSE_MAX)) return -1;
        delay(PULSE_HOLD_MS);

        if (!rampTo(PULSE_MIN)) return -1;
        if (!rampTo(PULSE_MAX)) return -1;
        delay(PULSE_HOLD_MS);

    } else {
        if (!rampTo(PULSE_MIN)) return -1;
        if (!rampTo(PULSE_MAX)) return -1;
        delay(PULSE_HOLD_MS);

        if (!rampTo(PULSE_MIN)) return -1;
        if (!rampTo(PULSE_MAX)) return -1;
        delay(PULSE_HOLD_MS);
    }

    if (!rampTo(original)) return -1;

    lampada.setState(wasOn);
    return sendConfig();
}

void Lamp_ShowChangeOfState() {
    if (!Lamp_isOn())
        return;

    const int original = max(PULSE_MIN, lampada.getDimming());
    const bool startBright = original < 40;

    if (!rampTo(startBright ? PULSE_MAX : PULSE_MIN))
        Serial.println("Effetto cambio stato: prima rampa fallita");

    delay(PULSE_HOLD_MS);

    if (!rampTo(startBright ? PULSE_MIN : PULSE_MAX))
        Serial.println("Effetto cambio stato: seconda rampa fallita");

    if (!rampTo(original))
        Serial.println("Effetto cambio stato: ripristino luminosita fallito");
}

void Lamp_AddClaim(const String& mac, uint32_t now) {
    if (mac.isEmpty()) return;

    for (int i = 0; i < claimCount; i++) {
        if (claims[i].mac == mac) {
            claims[i].lastSeen = now;
            return;
        }
    }

    if (claimCount < MAX_LAMPS) {
        claims[claimCount++] = {mac, now};
    }
}

String Lamp_GetAssignedMac() {
    return isConnected() ? lampada.getMac() : String();
}

bool Lamp_isConnected() {
    return isConnected();
}

