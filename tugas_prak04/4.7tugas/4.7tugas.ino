#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>

const char* ssid = "Samsung";
const char* password = "not my style";

const byte dhtPin = 2;
const byte buttonPin = 4;
const byte ledPin = 12;
const byte pwmLedPin = 5;

DHT dht(dhtPin, DHT22);

bool ledState = false;
String currentTemp = "--";
String currentHum = "--";

int pwmValue = 0;
int buttonState = LOW;
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
      font-family: Arial, sans-serif;
      text-align: center;
      margin: 0;
      padding: 15px;
      background: #f7f7f7;
    }
    .card {
      background: white;
      margin: 18px auto;
      padding: 20px;
      max-width: 340px;
      border-radius: 12px;
      box-shadow: 0 2px 8px #0001;
    }
    button {
      padding: 12px 25px;
      font-size: 18px;
      border: 0;
      border-radius: 6px;
      cursor: pointer;
      color: white;
    }
    .btn-on { background-color: #4CAF50; }
    .btn-off { background-color: #f44336; }
    input[type=range] {
      width: 100%;
      margin: 18px 0;
    }
  </style>
</head>
<body>
  <h1>Smart Room</h1>

  <div class="card">
    <h2>Suhu: <span id="tempValue">--</span> °C</h2>
  </div>

  <div class="card">
    <h2>Kelembapan: <span id="humValue">--</span> %</h2>
  </div>

  <div class="card">
    <h2>LED ON/OFF: <span id="ledStatus">OFF</span></h2>
    <button id="toggleBtn" class="btn-off" onclick="toggleLed()">Turn ON</button>
  </div>

  <div class="card">
    <h2>Kontrol Intensitas LED PWM</h2>
    <input type="range" min="0" max="1023" value="0"
           id="pwmSlider" oninput="sendPWM(this.value)">
    <p>Nilai PWM: <span id="pwmValue">0</span> / 1023</p>
    <p>Intensitas: <span id="pwmPercent">0</span>%</p>
  </div>

  <script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;

    window.addEventListener('load', onLoad);

    function onLoad() {
      initWebSocket();
    }

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen = onOpen;
      websocket.onclose = onClose;
      websocket.onmessage = onMessage;
    }

    function onOpen() {
      console.log('WebSocket Terkoneksi');
    }

    function onClose() {
      setTimeout(initWebSocket, 2000);
    }

    function toggleLed() {
      if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send('toggle');
      }
    }

    function sendPWM(value) {
      document.getElementById('pwmValue').textContent = value;
      document.getElementById('pwmPercent').textContent =
        Math.round((Number(value) / 1023) * 100);

      if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send('pwm,' + value);
      }
    }

    function onMessage(event) {
      var dataObj = JSON.parse(event.data);

      if (dataObj.suhu !== undefined) {
        document.getElementById('tempValue').textContent = dataObj.suhu;
      }

      if (dataObj.hum !== undefined) {
        document.getElementById('humValue').textContent = dataObj.hum;
      }

      if (dataObj.led !== undefined) {
        var btn = document.getElementById('toggleBtn');
        var status = document.getElementById('ledStatus');

        if (dataObj.led == "1") {
          status.textContent = "ON";
          btn.textContent = "Turn OFF";
          btn.className = "btn-on";
        } else {
          status.textContent = "OFF";
          btn.textContent = "Turn ON";
          btn.className = "btn-off";
        }
      }

      if (dataObj.pwm !== undefined) {
        document.getElementById('pwmSlider').value = dataObj.pwm;
        document.getElementById('pwmValue').textContent = dataObj.pwm;
        document.getElementById('pwmPercent').textContent =
          Math.round((Number(dataObj.pwm) / 1023) * 100);
      }
    }
  </script>
</body>
</html>
)rawliteral";

void notifyClients() {
  String jsonString = "{\"led\":\"" + String(ledState ? 1 : 0) + "\",";
  jsonString += "\"suhu\":\"" + currentTemp + "\",";
  jsonString += "\"hum\":\"" + currentHum + "\",";
  jsonString += "\"pwm\":" + String(pwmValue) + "}";
  ws.textAll(jsonString);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;

  if (info->final && info->index == 0 &&
      info->len == len && info->opcode == WS_TEXT) {

    String message;
    message.reserve(len);
    for (size_t i = 0; i < len; i++) {
      message += (char)data[i];
    }

    if (message == "toggle") {
      ledState = !ledState;
      digitalWrite(ledPin, ledState ? HIGH : LOW);
      notifyClients();
    } else if (message.startsWith("pwm,")) {
      int value = message.substring(4).toInt();
      value = constrain(value, 0, 1023);
      pwmValue = value;
      analogWrite(pwmLedPin, pwmValue);
      notifyClients();
      Serial.print("PWM: ");
      Serial.println(pwmValue);
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len) {
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

    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(pwmLedPin, OUTPUT);

  digitalWrite(ledPin, LOW);
  analogWriteRange(1023);
  analogWrite(pwmLedPin, 0);

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();
}

void loop() {
  ws.cleanupClients();

  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if (millis() - lastDebounceTime > debounceDelay) {
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
