
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// Konfigurasi Wi-Fi
const char* ssid = "Samsung";
const char* password = "not my style";

// Konfigurasi broker MQTT
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;

// Identitas perangkat
const char* clientId = "ESP8266_LabIoT_25782098_Full";

// Topik telemetri dan kendali
const char* topicPublish =
  "kampus/labiot/25782098/telemetri";

const char* topicSubscribe =
  "kampus/labiot/25782098/kendali";

// Konfigurasi pin
const byte dhtPin = 2;  // D4 / GPIO2
const byte ledPin = 5;  // D1 / GPIO5

#define DHTTYPE DHT22

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

// Interval pengiriman telemetri
unsigned long lastTelemetryTime = 0;
const unsigned long telemetryInterval = 4000;

// Callback untuk menerima perintah LED
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.println("----------------------------------------");
  Serial.print("[KONTROL DITERIMA] Topik: ");
  Serial.println(topic);

  // Membatasi ukuran payload agar buffer aman
  if (length >= 256) {
    Serial.println("Payload terlalu panjang!");
    return;
  }

  char incomingBuffer[256];
  memcpy(incomingBuffer, payload, length);
  incomingBuffer[length] = '\0';

  Serial.print("Payload: ");
  Serial.println(incomingBuffer);

  // Membaca payload JSON
  StaticJsonDocument<256> doc;
  DeserializationError err =
    deserializeJson(doc, incomingBuffer);

  if (err) {
    Serial.println("Gagal membaca JSON!");
    return;
  }

  // Memeriksa perintah LED
  if (!doc["led"].is<int>()) {
    Serial.println("Perintah LED tidak valid!");
    return;
  }

  int val = doc["led"];

  if (val == 1) {
    digitalWrite(ledPin, HIGH);
    Serial.println("[INDIKATOR] LED MENYALA");
  } else if (val == 0) {
    digitalWrite(ledPin, LOW);
    Serial.println("[INDIKATOR] LED PADAM");
  } else {
    Serial.println("Nilai led harus 1 atau 0!");
  }

  Serial.println("----------------------------------------");
}

// Menghubungkan kembali ke broker MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT Broker...");

    if (client.connect(clientId)) {
      Serial.println("Terhubung!");

      if (client.subscribe(topicSubscribe)) {
        Serial.print("Subskripsi berhasil: ");
        Serial.println(topicSubscribe);
      } else {
        Serial.println("Gagal melakukan subskripsi!");
      }
    } else {
      Serial.print("Gagal, kode status: ");
      Serial.println(client.state());
      delay(4000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Menyiapkan LED dengan kondisi awal padam
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // Memulai sensor DHT22
  dht.begin();

  // Menghubungkan ke Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Menghubungkan ke Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi Terkoneksi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Menyiapkan koneksi MQTT dan callback
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }

  // Memproses pesan MQTT yang masuk
  client.loop();

  unsigned long now = millis();

  // Mengirim telemetri setiap 4 detik
  if (now - lastTelemetryTime >= telemetryInterval) {
    lastTelemetryTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h)) {
      Serial.println("[ERROR] Gagal membaca sensor DHT22!");
      return;
    }

    // Membuat JSON telemetri
    StaticJsonDocument<256> docOut;
    docOut["node_id"] = clientId;
    docOut["suhu"] = t;
    docOut["kelembapan"] = h;
    docOut["status_led"] = digitalRead(ledPin);

    // Tambahan Latihan 3
    docOut["uptime_detik"] = millis() / 1000;
    docOut["ip_lokal"] = WiFi.localIP().toString();

    char buffer[256];
    serializeJson(docOut, buffer, sizeof(buffer));

    // Mengirim telemetri ke broker
    bool success = client.publish(topicPublish, buffer);

    if (success) {
      Serial.print("[TELEMETRI TERKIRIM] Topik: ");
      Serial.println(topicPublish);
      Serial.print("Payload: ");
      Serial.println(buffer);
    } else {
      Serial.println("[ERROR] Gagal mengirim telemetri!");
    }
  }
}
