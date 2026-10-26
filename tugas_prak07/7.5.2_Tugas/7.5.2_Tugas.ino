#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// 1. Kredensial Wi-Fi
const char* ssid = "Samsung";
const char* password = "not my style";

// 2. Konfigurasi Broker MQTT
const char* mqttServer = "10.196.1.113";
const int mqttPort = 1883;

// Autentikasi MQTT
const char* mqttUser = "admin";
const char* mqttPassword = "112233";

// 3. Identitas dan Topik MQTT
const char* clientId = "ESP8266_Local_Node01";
const char* topicPubTele = "kampus/labiot/telemetri";
const char* topicSubCmd = "kampus/labiot/kendali";

// 4. Pin perangkat keras
const byte dhtPin = 2;  // D4 / GPIO2
const byte ledPin = 5;  // D1 / GPIO5

#define DHTTYPE DHT22

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTeleTime = 0;
const unsigned long teleInterval = 4000;

// Menerima instruksi dari Node-RED
void onMessageCallback(char* topic, byte* payload, unsigned int length) {
  char incomingBuffer[length + 1];
  memcpy(incomingBuffer, payload, length);
  incomingBuffer[length] = '\0';

  Serial.println("\n----------------------------------------");
  Serial.printf("[INSTRUKSI MASUK] Topik: %s\n", topic);
  Serial.printf("[PAYLOAD]: %s\n", incomingBuffer);

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, incomingBuffer);

  if (err) {
    Serial.printf("[ERROR] JSON tidak valid: %s\n", err.c_str());
    return;
  }

  if (doc.containsKey("led")) {
    int stateLED = doc["led"];
    digitalWrite(ledPin, stateLED == 1 ? HIGH : LOW);

    Serial.printf("[AKTUATOR] LED %s\n",
                  stateLED == 1 ? "MENYALA" : "PADAM");
  } else {
    Serial.println("[WARNING] Kunci 'led' tidak ditemukan!");
  }

  Serial.println("----------------------------------------");
}

// Menghubungkan ke Wi-Fi
void connectToWiFi() {
  Serial.printf("\n[Wi-Fi] Menghubungkan ke %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n[Wi-Fi] Berhasil terhubung!");
  Serial.print("[Wi-Fi] IP NodeMCU: ");
  Serial.println(WiFi.localIP());
}

// Menghubungkan ke MQTT dengan autentikasi
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.printf("[MQTT] Menghubungkan ke %s...\n", mqttServer);

    if (client.connect(clientId, mqttUser, mqttPassword)) {
      Serial.println("[MQTT] Berhasil terhubung dengan autentikasi!");

      if (client.subscribe(topicSubCmd)) {
        Serial.printf("[MQTT] Berlangganan: %s\n", topicSubCmd);
      } else {
        Serial.println("[MQTT] Gagal berlangganan topik kendali.");
      }
    } else {
      Serial.printf("[MQTT] Gagal, rc=%d. Coba lagi 4 detik.\n",
                    client.state());
      delay(4000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  dht.begin();
  connectToWiFi();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(onMessageCallback);
  client.setBufferSize(512);
}

// Program utama
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  if (!client.connected()) {
    reconnectMQTT();
  }

  client.loop();

  unsigned long now = millis();

  if (now - lastTeleTime >= teleInterval) {
    lastTeleTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      StaticJsonDocument<256> docOut;

      docOut["node_id"] = clientId;
      docOut["suhu"] = t;
      docOut["kelembapan"] = h;
      docOut["status_led"] = digitalRead(ledPin);

      char teleBuffer[256];
      serializeJson(docOut, teleBuffer);

      if (client.publish(topicPubTele, teleBuffer)) {
        Serial.printf("[TELEMETRI TERKIRIM] %s\n", teleBuffer);
      } else {
        Serial.println("[ERROR] Gagal mengirim telemetri!");
      }
    } else {
      Serial.println("[ERROR] Gagal membaca sensor DHT!");
    }
  }
}
