// Node 1 - Stasiun Lingkungan (DHT11/DHT22 -> D4 / GPIO2)
#include <painlessMesh.h>
#include <DHT.h>

#define MESH_PREFIX   "Samsung"
#define MESH_PASSWORD "not my style"
#define MESH_PORT     5555

#define DHTPIN  2        // D4 = GPIO2
#define DHTTYPE DHT22    // ganti DHT22 jika memakai DHT22

DHT dht(DHTPIN, DHTTYPE);
Scheduler userScheduler;
painlessMesh mesh;

void sendMessage();
Task taskSendMessage(TASK_SECOND * 3, TASK_FOREVER, &sendMessage);

void sendMessage() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println("Gagal membaca DHT, paket dilewati");
    return;
  }

  char buf[96];
  snprintf(buf, sizeof(buf),
           "{\"tipe\":\"suhu_node\",\"suhu\":%.1f,\"kelembapan\":%.1f}", t, h);
  String msg(buf);
  mesh.sendBroadcast(msg);
  Serial.printf("[TX] %s\n", msg.c_str());
}

void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("[RX] dari %u: %s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("[MESH] Koneksi baru: %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.printf("[MESH] Topologi berubah: %s\n", mesh.subConnectionJson().c_str());
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("[MESH] Waktu disesuaikan, offset=%d\n", offset);
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();
  Serial.printf("Node 1 aktif, ID: %u\n", mesh.getNodeId());
}

void loop() {
  mesh.update();
}
