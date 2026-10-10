#include <painlessMesh.h>
#include <ArduinoJson.h>

#define MESH_PREFIX   "Samsung"
#define MESH_PASSWORD "not my style"
#define MESH_PORT     5555

const byte ledPin = 12;   // D6 (GPIO 12)

// Nilai terakhir dari tiap node (awalnya kondisi "normal")
float suhuTerakhir = 0.0;
int   adcTerakhir  = 1023;

Scheduler userScheduler;
painlessMesh mesh;

// Rule Engine: nilai LED ditentukan dari gabungan 2 sensor
void evaluasiRule() {
  bool panas  = (suhuTerakhir > 31.0);
  bool gelap  = (adcTerakhir < 300);

  if (panas || gelap) {
    digitalWrite(ledPin, HIGH);
    Serial.println("[AKTUATOR] LED ON");
  } else {
    digitalWrite(ledPin, LOW);
    Serial.println("[AKTUATOR] LED OFF");
  }
}

void receivedCallback(uint32_t from, String &msg) {
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, msg);

  if (error) {
    Serial.println("[ERROR] JSON tidak valid!");
    return;
  }

  const char* tipe = doc["tipe"];
  if (tipe == nullptr) return;

  if (strcmp(tipe, "suhu_node") == 0) {
    suhuTerakhir = doc["suhu"];
    float hum    = doc["kelembapan"];
    Serial.printf("[NODE 1] Suhu: %.1f C | Kelembapan: %.1f %%\n", suhuTerakhir, hum);
  }
  else if (strcmp(tipe, "cahaya_node") == 0) {
    adcTerakhir = doc["adc"];
    Serial.printf("[NODE 2] ADC Cahaya: %d\n", adcTerakhir);
  }
  else {
    return;   // tipe tidak dikenal, abaikan
  }

  evaluasiRule();
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("--> Koneksi Baru Terdeteksi! Node ID: %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.println("--> Topologi rantai mesh telah diperbarui");
}

void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);

  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

  Serial.println("Node 3 (Hub Aktuator) berjalan. Menunggu data...");
}

void loop() {
  mesh.update();
}