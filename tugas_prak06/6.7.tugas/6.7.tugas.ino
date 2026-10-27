
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// ==================================================
// KONFIGURASI WI-FI DAN MQTT
// ==================================================
const char* ssid = "Samsung";
const char* password = "not my style";

const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;

const char* clientId = "ESP8266_LabIoT_25782098_Safety";

// Topik MQTT
const char* topicTelemetry =
  "kampus/labiot/25782098/telemetri";

const char* topicControl =
  "kampus/labiot/25782098/kendali";

const char* topicAlert =
  "kampus/labiot/25782098/alert";

const char* topicStatus =
  "kampus/labiot/25782098/status";

// Pesan LWT, dikirim broker saat koneksi
// terputus secara tidak normal.
const char* lwtMessage =
  "{\"status\":\"OFFLINE\","
  "\"keterangan\":\"Perangkat Terputus Tak Terduga\"}";

// ==================================================
// KONFIGURASI SENSOR DAN AKTUATOR
// ==================================================
#define DHTPIN 2       // D4 / GPIO2
#define DHTTYPE DHT22  // Ganti ke DHT11 jika memakai DHT11

const byte ledPin = 5; // D1 / GPIO5
const float batasSuhu = 31.0;

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

// ==================================================
// PENGATURAN INTERVAL NON-BLOCKING
// ==================================================
unsigned long waktuBacaSensor = 0;
unsigned long waktuTelemetri = 0;
unsigned long waktuCobaWiFi = 0;
unsigned long waktuCobaMQTT = 0;

const unsigned long intervalSensor = 2000;
const unsigned long intervalTelemetri = 4000;
const unsigned long intervalKoneksi = 5000;

// ==================================================
// STATUS SISTEM
// ==================================================
float suhu = NAN;
float kelembapan = NAN;

bool sensorValid = false;
bool alertTerkirim = false;
bool ledManual = false;

// ==================================================
// MENGATUR LED BERDASARKAN KESELAMATAN
// ==================================================
void perbaruiLED() {
  // Fail-safe: LED menyala jika sensor belum valid
  // atau suhu melewati batas keselamatan.
  if (!sensorValid || suhu > batasSuhu) {
    digitalWrite(ledPin, HIGH);
  } else {
    digitalWrite(ledPin, ledManual ? HIGH : LOW);
  }
}

// ==================================================
// CALLBACK PERINTAH KENDALI DARI MQTT
// ==================================================
void callback(char* topic, byte* payload, unsigned int length) {
  if (String(topic) != topicControl) {
    return;
  }

  if (length >= 256) {
    Serial.println("[KENDALI] Payload terlalu panjang.");
    return;
  }

  char pesan[256];
  memcpy(pesan, payload, length);
  pesan[length] = '\0';

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, pesan);

  if (error || !doc["led"].is<int>()) {
    Serial.println("[KENDALI] JSON tidak valid.");
    return;
  }

  int perintah = doc["led"];

  // Kendali manual hanya diizinkan saat suhu aman.
  if (!sensorValid || suhu > batasSuhu) {
    Serial.println(
      "[SAFETY] Kendali manual ditolak. "
      "Suhu berbahaya atau sensor belum valid."
    );
    perbaruiLED();
    return;
  }

  if (perintah == 1) {
    ledManual = true;
    Serial.println("[KENDALI] LED dinyalakan manual.");
  } else if (perintah == 0) {
    ledManual = false;
    Serial.println("[KENDALI] LED dimatikan manual.");
  } else {
    Serial.println("[KENDALI] Nilai led harus 0 atau 1.");
    return;
  }

  perbaruiLED();
}

// ==================================================
// MENJALANKAN RULE ENGINE KESELAMATAN
// ==================================================
void periksaKeselamatan() {
  if (!sensorValid) {
    perbaruiLED();
    return;
  }

  if (suhu > batasSuhu) {
    // Aktifkan LED tanpa menunggu perintah operator.
    perbaruiLED();

    // Kirim satu peringatan untuk setiap kejadian panas.
    // Jika publish gagal, coba kembali pada putaran berikutnya.
    if (!alertTerkirim && client.connected()) {
      StaticJsonDocument<256> alert;
      alert["status"] = "BAHAYA";
      alert["parameter"] = "OVERHEAT";
      alert["nilai_suhu"] = suhu;

      char buffer[256];
      serializeJson(alert, buffer, sizeof(buffer));

      if (client.publish(topicAlert, buffer)) {
        alertTerkirim = true;
        Serial.print("[ALERT TERKIRIM] ");
        Serial.println(buffer);
      } else {
        Serial.println("[ALERT] Gagal mengirim peringatan.");
      }
    }
  } else {
    // Reset latch setelah suhu kembali aman.
    // Jika suhu naik lagi, peringatan baru dapat dikirim.
    alertTerkirim = false;
    perbaruiLED();
  }
}

// ==================================================
// MEMBACA SENSOR DHT TANPA DELAY
// ==================================================
void bacaSensor() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    sensorValid = false;
    Serial.println("[ERROR] Pembacaan DHT gagal.");
    perbaruiLED();
    return;
  }

  suhu = t;
  kelembapan = h;
  sensorValid = true;

  Serial.print("[SENSOR] Suhu: ");
  Serial.print(suhu);
  Serial.print(" C | Kelembapan: ");
  Serial.print(kelembapan);
  Serial.println(" %");

  periksaKeselamatan();
}

// ==================================================
// MENGIRIM STATUS ONLINE DENGAN RETAIN
// ==================================================
void kirimStatusOnline() {
  StaticJsonDocument<128> doc;
  doc["status"] = "ONLINE";
  doc["keterangan"] = "Perangkat Beroperasi Normal";

  char buffer[128];
  serializeJson(doc, buffer, sizeof(buffer));

  if (client.publish(topicStatus, buffer, true)) {
    Serial.println("[STATUS] ONLINE dipublikasikan.");
  } else {
    Serial.println("[STATUS] Gagal memublikasikan ONLINE.");
  }
}

// ==================================================
// MENGHUBUNGKAN KE MQTT DAN MENDAFTARKAN LWT
// ==================================================
void cobaKoneksiMQTT() {
  if (WiFi.status() != WL_CONNECTED || client.connected()) {
    return;
  }

  unsigned long sekarang = millis();

  if (sekarang - waktuCobaMQTT < intervalKoneksi) {
    return;
  }

  waktuCobaMQTT = sekarang;
  Serial.println("[MQTT] Mencoba menghubungkan broker...");

  // LWT: QoS 1, retained true.
  bool berhasil = client.connect(
    clientId,
    topicStatus,
    1,
    true,
    lwtMessage
  );

  if (berhasil) {
    Serial.println("[MQTT] Terhubung ke broker.");

    if (client.subscribe(topicControl)) {
      Serial.println("[MQTT] Subskripsi kendali berhasil.");
    }

    kirimStatusOnline();

    // Jika suhu masih berbahaya saat koneksi pulih,
    // izinkan pengiriman alert setelah koneksi tersedia.
    periksaKeselamatan();
  } else {
    Serial.print("[MQTT] Gagal, kode: ");
    Serial.println(client.state());
  }
}

// ==================================================
// MENGIRIM TELEMETRI RUTIN DALAM FORMAT JSON
// ==================================================
void kirimTelemetri() {
  if (!client.connected() || !sensorValid) {
    return;
  }

  StaticJsonDocument<384> doc;
  doc["node_id"] = clientId;
  doc["suhu"] = suhu;
  doc["kelembapan"] = kelembapan;
  doc["status_led"] = digitalRead(ledPin);
  doc["kondisi_suhu"] = (suhu > batasSuhu) ? "PANAS" : "NORMAL";
  doc["uptime_detik"] = millis() / 1000;
  doc["ip_lokal"] = WiFi.localIP().toString();

  char buffer[384];
  serializeJson(doc, buffer, sizeof(buffer));

  if (client.publish(topicTelemetry, buffer)) {
    Serial.print("[TELEMETRI] ");
    Serial.println(buffer);
  } else {
    Serial.println("[TELEMETRI] Pengiriman gagal.");
  }
}

// ==================================================
// SETUP
// ==================================================
void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH); // Fail-safe saat awal mulai

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  client.setKeepAlive(30);
  client.setSocketTimeout(2);
  client.setBufferSize(512);

  Serial.println();
  Serial.println("=== INDUSTRIAL SAFETY SYSTEM ===");
  Serial.println("[WiFi] Memulai koneksi...");
}

// ==================================================
// LOOP: SEMUA PROSES BERDASARKAN MILLIS()
// ==================================================
void loop() {
  unsigned long sekarang = millis();

  // Coba sambungkan ulang Wi-Fi tanpa menunggu lama.
  if (WiFi.status() != WL_CONNECTED) {
    if (sekarang - waktuCobaWiFi >= intervalKoneksi) {
      waktuCobaWiFi = sekarang;
      Serial.println("[WiFi] Mencoba koneksi ulang...");
      WiFi.begin(ssid, password);
    }
  } else {
    cobaKoneksiMQTT();
  }

  // Wajib sering dipanggil untuk memproses MQTT.
  if (client.connected()) {
    client.loop();
  }

  // Baca sensor sesuai interval DHT.
  if (sekarang - waktuBacaSensor >= intervalSensor) {
    waktuBacaSensor = sekarang;
    bacaSensor();
  }

  // Kirim telemetri rutin setiap 4 detik.
  if (sekarang - waktuTelemetri >= intervalTelemetri) {
    waktuTelemetri = sekarang;
    kirimTelemetri();
  }
}
