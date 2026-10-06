#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>

const char* ssid = "Samsung";
const char* password = "not my style";

const byte dhtPin = 2;
const byte buttonPin = 4;
const byte ledPin = 12;

DHT dht(dhtPin, DHT22);

bool ledState = false;
String currentTemp = "--";
String currentHum = "--";

int buttonState;
int lastButtonState = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
unsigned long lastTime = 0;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Smart Room</title>

  <style>
    body {
      font-family: Arial;
      text-align: center;
    }

    .card {
      background: #f0f0f0;
      margin: 20px auto;
      padding: 20px;
      max-width: 300px;
      border-radius: 10px;
    }

    button {
      padding: 15px 30px;
      font-size: 20px;
      border-radius: 5px;
      cursor: pointer;
      color: white;
    }

    .btn-on {
      background-color: #4CAF50;
    }

    .btn-off {
      background-color: #f44336;
    }
  </style>
</head>

<body>

  <h1>Smart Room</h1>

  <div class="card">
    <h2>Suhu: <span id="tempValue">--</span> Celcius</h2>
  </div>

  <div class="card">
    <h2>Kelembapan: <span id="humValue">--</span> %</h2>
  </div>

  <div class="card">
    <h2>LED: <span id="ledStatus">OFF</span></h2>
    <button id="toggleBtn" class="btn-off" onclick="toggleLed()">Turn ON</button>
  </div>

  <script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;

    window.addEventListener('load', onLoad);

    function onLoad(event) {
      initWebSocket();
    }

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen = onOpen;
      websocket.onclose = onClose;
      websocket.onmessage = onMessage;
    }

    function onOpen(event) {s
      console.log('WebSocket Terkoneksi');
    }

    function onClose(event) {
      setTimeout(initWebSocket, 2000);
    }

    function toggleLed() {
      websocket.send('toggle');
    }

    function onMessage(event) {
      var dataObj = JSON.parse(event.data);

      if (dataObj.suhu !== undefined) {
        document.getElementById('tempValue').innerHTML = dataObj.suhu;
      }

      if (dataObj.hum !== undefined) {
        document.getElementById('humValue').innerHTML = dataObj.hum;
      }

      if (dataObj.led !== undefined) {
        var btn = document.getElementById('toggleBtn');
        var status = document.getElementById('ledStatus');

        if (dataObj.led == "1") {
          status.innerHTML = "ON";
          btn.innerHTML = "Turn OFF";
          btn.className = "btn-on";
        } else {
          status.innerHTML = "OFF";
          btn.innerHTML = "Turn ON";
          btn.className = "btn-off";
        }
      }
    }
  </script>

</body>
</html>
)rawliteral";

void notifyClients() {
  String jsonString = "{\"led\":\"" + String(ledState ? 1 : 0) + "\", ";
  jsonString += "\"suhu\":\"" + currentTemp + "\", ";
  jsonString += "\"hum\":\"" + currentHum + "\"}";
  ws.textAll(jsonString);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;

  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;

    if (strcmp((char*)data, "toggle") == 0) {
      ledState = !ledState;
      notifyClients();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {

  switch (type) {

    case WS_EVT_CONNECT:
      Serial.printf("Client WebSocket #%u terhubung\n", client->id());
      notifyClients();
      break;

    case WS_EVT_DISCONNECT:
      Serial.printf("Client WebSocket #%u terputus\n", client->id());
      break;

    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nIP Address: " + WiFi.localIP().toString());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();
}

void loop() {
  ws.cleanupClients();

  digitalWrite(ledPin, ledState ? HIGH : LOW);

  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == HIGH) {
        ledState = !ledState;
        digitalWrite(ledPin, ledState ? HIGH : LOW);
        notifyClients();
      }
    }
  }

  lastButtonState = reading;

  if (millis() - lastTime >= 3000) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      currentTemp = String(t, 1);
      currentHum = String(h, 1);
      notifyClients();

      Serial.print("Suhu: ");
      Serial.print(currentTemp);
      Serial.print(" C | Kelembapan: ");
      Serial.print(currentHum);
      Serial.println(" %");
    } else {
      Serial.println("Gagal membaca sensor DHT!");
    }

    lastTime = millis();
  }
}