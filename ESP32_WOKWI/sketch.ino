#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

// =============================
// CONFIG WIFI & MQTT
// =============================
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "test.mosquitto.org";

WiFiClient espClient;
PubSubClient client(espClient);

// =============================
// CONFIG GENERAL
// =============================
#define NUM_TERRENOS 3

struct Terreno {
    // Pines
    int dhtPin;
    int soilPin;
    int relayPin;
    int ledPin;
    int buttonPin;

    // Sensores
    DHT* dht;

    // Estado
    bool controlManual;
    bool bombaEncendida;
    float soilHumidity;
    float temp;
    float humAmb;
    long tiempoInicioRiego;

    // Variables de volumen
    float volumenObjetivo_L;
    float caudalTotal_Lps;
    float volumenAcumulado;
    unsigned long tiempoAnteriorVol;

    // Botón
    unsigned long ultimoTiempoBoton;
    int ultimoEstadoBoton;
    // Flags de eventos
    bool alertaTemp;
    bool accionManual;
    bool accionAutoEncendido;
    bool accionAutoApagado;
    bool seguridadTiempo;
    bool seguridadVolumen;
};

// =============================
// CONFIG DE LOS TERRENOS
// =============================
Terreno terrenos[NUM_TERRENOS] = {
    {25, 32, 0, 33, 5,  nullptr, false, false, 70.0, 18.0, 50.0, 0, 2667.0, 20.0/60.0, 0.0, 0, 0, HIGH},
    {26, 34, 4, 14, 18, nullptr, false, false, 70.0, 18.0, 50.0, 0, 2667.0, 20.0/60.0, 0.0, 0, 0, HIGH},
    {27, 35, 16, 13, 19, nullptr, false, false, 70.0, 18.0, 50.0, 0, 2667.0, 20.0/60.0, 0.0, 0, 0, HIGH}
};

// =============================
// UMBRALES Y SEGURIDAD
// =============================
const float HUM_ON = 60.0;
const float HUM_OFF = 80.0;
const unsigned long MAX_RIEGO_MS = 1800000; 
const unsigned long REBOTE = 300;
const unsigned long INTERVALO_VOL = 1000;

// =============================
// FUNCIONES DE WIFI
// =============================
void setupWifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi conectado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// =============================
// FUNCIONES DE MQTT
// =============================

void callback(char* topic, byte* message, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)message[i];
  }

  Serial.print("[MQTT] Mensaje en ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(msg);

  for (int i = 0; i < NUM_TERRENOS; i++) {
    String controlTopic = "riego/control/" + String(i+1);
    if (String(topic) == controlTopic) {
      if (msg == "ON") {
        terrenos[i].controlManual = true;
        terrenos[i].bombaEncendida = true;
        terrenos[i].tiempoInicioRiego = millis();
        Serial.printf("[MQTT] Terreno %d - Bomba encendida remotamente\n", i+1);
      } else if (msg == "OFF") {
        terrenos[i].controlManual = false;
        terrenos[i].bombaEncendida = false;
        Serial.printf("[MQTT] Terreno %d - Bomba apagada remotamente\n", i+1);
      }
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("[MQTT] Intentando conexión...");
    if (client.connect("ESP32RiegoClient")) {
      Serial.println("Conectado ✅");

      // Suscripción para cada terreno
      for (int i = 0; i < NUM_TERRENOS; i++) {
        String topic = "riego/control/" + String(i+1);
        client.subscribe(topic.c_str());
        Serial.print("[MQTT] Suscrito a ");
        Serial.println(topic);
      }

    } else {
      Serial.print("Error, rc=");
      Serial.print(client.state());
      Serial.println(" Reintentando en 5s...");
      delay(5000);
    }
  }
}

// =============================
// FUNCIONES DE ESTADO
// =============================

void publishEstado() {
  for (int i = 0; i < NUM_TERRENOS; i++) {
    String payload = "{";
    payload += "\"temp\":" + String(terrenos[i].temp) + ",";
    payload += "\"humAmb\":" + String(terrenos[i].humAmb) + ",";
    payload += "\"humSuelo\":" + String(terrenos[i].soilHumidity) + ",";
    payload += "\"volumen\":" + String(terrenos[i].volumenAcumulado) + ",";
    payload += "\"bomba\":\"" + String(terrenos[i].bombaEncendida ? "ON" : "OFF") + "\"";
    payload += "}";

    String topic = "riego/estado/" + String(i+1);
    client.publish(topic.c_str(), payload.c_str(), true);

    Serial.printf("[MQTT] Estado publicado (%s): %s\n", topic.c_str(), payload.c_str());
  }
}

void logEstado() {
  unsigned long timestamp = millis() / 1000;
  for (int i = 0; i < NUM_TERRENOS; i++) {
    Terreno &t = terrenos[i];
    Serial.printf("[LOG][%lus] Terreno %d | Temp=%.1f°C | HumAmb=%.1f%% | HumSuelo=%.1f%% | Volumen=%.2f L | Bomba=%s\n",
                  timestamp, i+1,
                  t.temp,
                  t.humAmb,
                  t.soilHumidity,
                  t.volumenAcumulado,
                  t.bombaEncendida ? "ON" : "OFF");

    // Mostrar alertas/acciones específicas
    if (t.alertaTemp) Serial.printf("   ⚠️ Terreno %d: Temp fuera de rango (%.1f°C)\n", i+1, t.temp);
    if (t.accionManual) Serial.printf("   🎛️ Terreno %d: Cambio manual (estado=%s)\n", i+1, t.bombaEncendida ? "ON" : "OFF");
    if (t.accionAutoEncendido) Serial.printf("   🤖 Terreno %d: Bomba encendida por humedad baja (%.1f%%)\n", i+1, t.soilHumidity);
    if (t.accionAutoApagado) Serial.printf("   🤖 Terreno %d: Bomba apagada por humedad alta (%.1f%%)\n", i+1, t.soilHumidity);
    if (t.seguridadTiempo) Serial.printf("   🛑 Terreno %d: Riego detenido por tiempo máximo\n", i+1);
    if (t.seguridadVolumen) Serial.printf("   🛑 Terreno %d: Riego detenido por volumen alcanzado\n", i+1);
  }
}

// =============================
// FUNCIONES DE TERRENOS
// =============================

void inicializarTerreno(Terreno &t) {
    pinMode(t.relayPin, OUTPUT);
    pinMode(t.ledPin, OUTPUT);
    pinMode(t.buttonPin, INPUT_PULLUP);
    t.dht = new DHT(t.dhtPin, DHT22);
    t.dht->begin();
}

bool leerBoton(Terreno &t) {
    int estado = digitalRead(t.buttonPin);
    unsigned long ahora = millis();
    if (estado == LOW && t.ultimoEstadoBoton == HIGH && (ahora - t.ultimoTiempoBoton) > REBOTE) {
        t.ultimoTiempoBoton = ahora;
        t.ultimoEstadoBoton = estado;
        return true;
    }
    t.ultimoEstadoBoton = estado;
    return false;
}

void actualizarTerreno(Terreno &t) {
    // Resetear flags en cada ciclo
    t.alertaTemp = false;
    t.accionManual = false;
    t.accionAutoEncendido = false;
    t.accionAutoApagado = false;
    t.seguridadTiempo = false;
    t.seguridadVolumen = false;

    // Lectura sensores
    t.temp = t.dht->readTemperature();
    t.humAmb = t.dht->readHumidity();
    int rawValue = analogRead(t.soilPin);
    t.soilHumidity = map(rawValue, 0, 4095, 100, 0);

    // Alerta temperatura
    if (t.temp < 10.0 || t.temp > 30.0) {
        t.alertaTemp = true;
    }

    // Botón manual
    if (leerBoton(t)) {
        t.controlManual = !t.controlManual;
        t.accionManual = true;
    }

    // Control manual
    if (t.controlManual) {
        if (!t.bombaEncendida) {
            t.bombaEncendida = true;
            t.tiempoInicioRiego = millis();
            t.accionManual = true;
        }
    } else {
        // Automático
        if (t.soilHumidity < HUM_ON && !t.bombaEncendida) {
            t.bombaEncendida = true;
            t.tiempoInicioRiego = millis();
            t.accionAutoEncendido = true;
        } else if (t.soilHumidity > HUM_OFF && t.bombaEncendida) {
            t.bombaEncendida = false;
            t.accionAutoApagado = true;
        }
    }

    // Seguridad: tiempo máximo
    if (t.bombaEncendida && (millis() - t.tiempoInicioRiego > MAX_RIEGO_MS)) {
        t.bombaEncendida = false;
        t.seguridadTiempo = true;
    }

    // Pines
    digitalWrite(t.relayPin, t.bombaEncendida ? HIGH : LOW);
    digitalWrite(t.ledPin, t.bombaEncendida ? HIGH : LOW);

    // Volumen
    if (t.bombaEncendida) {
        unsigned long ahora = millis();
        if (ahora - t.tiempoAnteriorVol >= INTERVALO_VOL) {
            t.tiempoAnteriorVol = ahora;
            t.volumenAcumulado += t.caudalTotal_Lps;
            if (t.volumenAcumulado >= t.volumenObjetivo_L) {
                t.bombaEncendida = false;
                t.seguridadVolumen = true;
            }
        }
    }
}




// =============================
// SETUP & LOOP
// =============================
void setup() {
    Serial.begin(115200);

    setupWifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

    for (int i = 0; i < NUM_TERRENOS; i++) {
        inicializarTerreno(terrenos[i]);
    }
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    for (int i = 0; i < NUM_TERRENOS; i++) {
        actualizarTerreno(terrenos[i]);
    }

    static unsigned long lastPublish = 0;
    if (millis() - lastPublish > 5000) { // cada 5s
        lastPublish = millis();
        publishEstado();
        logEstado();
    }
}
