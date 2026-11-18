#include <EspWizLight.h>


#include <Arduino.h>
#include <string.h>

#include "src/TouchModule.h"
#include "src/WifiModule.h"
#include "src/MqttModule.h"
#include "src/LampModule.h"

// Uscite digitali dei tre moduli touch (non elettrodi touchRead).
constexpr uint8_t TOUCH_POWER_PIN = 32;
constexpr uint8_t TOUCH_BRIGHTNESS_PIN = 27;
constexpr uint8_t TOUCH_TEMPERATURE_PIN = 18;

TouchModule touchPower;
TouchModule touchBrightness;
TouchModule touchTemperature;
constexpr uint8_t REMOTE_LED_PIN = 19;

constexpr uint32_t HEARTBEAT_MS = 10000;
constexpr uint32_t REMOTE_TIMEOUT_MS = 30000;
constexpr uint32_t CLAIM_INTERVAL_MS = 5000;
constexpr uint32_t DISCOVERY_WAIT_MS = 6000;

bool prevState = false;
bool currState = false;
bool hasChanged = false;
bool remoteOn = false;

uint32_t lastHeartbeatMs = 0;
uint32_t lastRemoteOnMs = 0;
uint32_t lastClaimMs = 0;
String lastClaimMac = "";

void blinkLed();

bool sendMessage(const char* message) {
    if (!Wifi_isConnected() || !Mqtt_SendMessage(message)) {
        return false;
    }
    return true;
}

void blinkLed() {
    bool previous = digitalRead(REMOTE_LED_PIN);

    for (int i = 0; i < 2; i++) {
        digitalWrite(REMOTE_LED_PIN, !previous);
        delay(150);
        digitalWrite(REMOTE_LED_PIN, previous);
        delay(150);
    }
}

void onRemoteMessage(const char* text) {
  if (text == nullptr) return;

  if (strncmp(text, "CLAIM:", 6) == 0) {
    Lamp_AddClaim(String(text + 6), millis());
    return;
  }

  if (strcmp(text, "ON") == 0) {
    digitalWrite(REMOTE_LED_PIN, HIGH);
    if (!remoteOn) Lamp_ShowChangeOfState();
    remoteOn = true;
    lastRemoteOnMs = millis();
  }
  else if (strcmp(text, "OFF") == 0) {
    digitalWrite(REMOTE_LED_PIN, LOW);
    if (remoteOn) Lamp_ShowChangeOfState();
    remoteOn = false;
  }
  else if (strcmp(text, "PULSE") == 0) {
    if (Lamp_Pulse() != 0)
      Serial.println("Impulso remoto non eseguito");
  }
}

void updateClaim(uint32_t now, bool mqttConnected) {
  if (!mqttConnected) {
    lastClaimMac = "";
    return;
  }

  const String mac = Lamp_GetAssignedMac();
  if (mac.isEmpty()) {
    lastClaimMac = "";
    return;
  }

  if (mac != lastClaimMac || now - lastClaimMs >= CLAIM_INTERVAL_MS) {
    const String message = String("CLAIM:") + mac;
    Mqtt_SendMessage(message.c_str());
    lastClaimMac = mac;
    lastClaimMs = now;
  }
}

//invia heartbeat ogni 10s se è ON, altrimenti non invia niente
void updateHeartbeat(uint32_t now) {
  if (Lamp_isOn() && now - lastHeartbeatMs >= HEARTBEAT_MS) {
    lastHeartbeatMs = now;
    sendMessage("ON");
  }
}

//mette remote a off se è passato troppo tempo
void updateRemoteTimeout(uint32_t now) {
  if (remoteOn && now - lastRemoteOnMs >= REMOTE_TIMEOUT_MS) {
    remoteOn = false;
    digitalWrite(REMOTE_LED_PIN, LOW);
  }
}

//Invia il cambio di stato
void updateLocalState() {
  currState = Lamp_isOn();
  hasChanged = (currState != prevState);
  prevState = currState;

  if (hasChanged){
    lastHeartbeatMs = millis();
    sendMessage(currState ? "ON" : "OFF");
  }
}

void setup() {
  pinMode(REMOTE_LED_PIN, OUTPUT);
  digitalWrite(REMOTE_LED_PIN, LOW);

  Serial.begin(115200);

  touchPower.Begin(TOUCH_POWER_PIN, GradientType::NONE);
  touchBrightness.Begin(TOUCH_BRIGHTNESS_PIN, GradientType::BRIGHTNESS);
  touchTemperature.Begin(TOUCH_TEMPERATURE_PIN, GradientType::TEMPERATURE);
  Serial.println("Touch pronto");

  Wifi_Begin();
  Mqtt_Begin(onRemoteMessage);
  Lamp_Begin();
}

void loop() {

  // 1. Aggiorna le connessioni
  Wifi_Update();
  Mqtt_Update();
  Lamp_Update(Mqtt_HasBeen(DISCOVERY_WAIT_MS));
  updateClaim(millis(), Mqtt_isConnected());

  // 2. Gestisci il touch
  const uint32_t now = millis();
  const bool lampOn = Lamp_isOn();
  // Leggi tutti gli ingressi prima degli effetti/rete, che possono bloccare.
  const TouchEvent powerEvent = touchPower.Update(now, lampOn);
  const TouchEvent brightnessEvent = touchBrightness.Update(now, lampOn);
  const TouchEvent temperatureEvent = touchTemperature.Update(
    now,
    lampOn && !Lamp_IsDimToWarm()
  );

  switch (powerEvent) {

    case TouchEvent::SINGLE_TAP:
      Serial.println("TOCCO SINGOLO – Power");
      if (Lamp_Toggle() != 0){
        Serial.println("Cambio di stato della lampadina fallito");
        blinkLed();
      }
      break;

    case TouchEvent::DOUBLE_TAP:
      Serial.println("DOPPIO TOCCO");
      if (Lamp_isOn() && sendMessage("PULSE")) {
        if (Lamp_Pulse() != 0){
          Serial.println("Impulso locale fallito");
          blinkLed();
        }
      }
      break;

    default:
      break; 
  }

  switch (brightnessEvent) {

    case TouchEvent::SINGLE_TAP:
      Serial.println("TOCCO SINGOLO – Brightness");
      break;
    
    case TouchEvent::DOUBLE_TAP:
      Serial.println("TOCCO DOPPIO – Brightness");
      if(Lamp_ToggleDimToWarm() != 0){
        Serial.println("Cambio Dim-to-Warm fallito");
      } else{
        Serial.printf(
          "Dim-to-Warm: %s\n",
          Lamp_IsDimToWarm() ? "ON" : "OFF"
        );
      }
      break;

    case TouchEvent::STEP_UP:
    case TouchEvent::STEP_DOWN:
      Serial.println("TOCCO PROLUNGATO – Brightness");
      if (Lamp_AdjustBrightness(brightnessEvent == TouchEvent::STEP_UP ? 1 : -1) != 0)
        Serial.println("Regolazione della luminosita fallita");
      break;    

    default:
      break;  // Una pressione lunga su on/off non cambia la lampada.
  }

  switch (temperatureEvent) {

    case TouchEvent::SINGLE_TAP:
      Serial.println("TOCCO SINGOLO – Temperature");
      break;

    case TouchEvent::STEP_UP:
    case TouchEvent::STEP_DOWN:
      Serial.println("TOCCO PROLUNGATO – Temperature");
      if (!Lamp_IsDimToWarm()) {
        if (Lamp_AdjustTemperature(temperatureEvent == TouchEvent::STEP_UP ? 1 : -1) != 0){
          Serial.println("Regolazione della temperatura fallita");
          blinkLed();
        }
      }
      break;
      
    default:
      break;  // Una pressione lunga su on/off non cambia la lampada.
  }
  // 3. Comunica lo stato aggiornato
  updateLocalState();
  updateHeartbeat(millis());
  updateRemoteTimeout(millis());

  delay(1);
}
