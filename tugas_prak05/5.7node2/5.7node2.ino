#include <painlessMesh.h>

#define MESH_PREFIX   "Samsung"
#define MESH_PASSWORD "not my style"
#define MESH_PORT     5555

painlessMesh mesh;
Scheduler userScheduler;

void sendMessage() {
  int adcValue = analogRead(A0);
  
  // Gunakan char array statis
  char msg[64];
  snprintf(msg, sizeof(msg), "{\"tipe\":\"cahaya_node\",\"adc\":%d}", adcValue);

  // Kirim sebagai String langsung dari buffer
  mesh.sendBroadcast(String(msg));
  Serial.printf("Kirim: %s\n", msg);
}

Task taskSendMessage(TASK_SECOND * 3, TASK_FOREVER, &sendMessage);

void setup() {
  Serial.begin(115200);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  
  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();
}

void loop() {
  mesh.update();
}