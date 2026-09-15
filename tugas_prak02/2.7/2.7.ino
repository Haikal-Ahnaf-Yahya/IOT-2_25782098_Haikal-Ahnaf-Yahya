#include <DHT.h>

#define DHTPIN D7
#define DHTTYPE DHT22

const int ldrPin = A0;
const int relayPin = D6;
const int ledPin = D1;

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);

  pinMode(relayPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  // Relay Active-Low: HIGH = mati
  digitalWrite(relayPin, HIGH);
  digitalWrite(ledPin, LOW);

  dht.begin();

  Serial.println("=== Smart Warehouse ===");
}

void loop() {
  float suhu = dht.readTemperature();
  int nilaiLDR = analogRead(ldrPin);

  if (isnan(suhu)) {
    Serial.println("Gagal membaca sensor DHT22!");
    delay(2500);
    return;
  }

  Serial.print("Suhu: ");
  Serial.print(suhu);
  Serial.print(" C | LDR: ");
  Serial.println(nilaiLDR);

  // Relay ON jika suhu > 34 ATAU LDR < 300
  if (suhu > 34 || nilaiLDR < 300) {
    digitalWrite(relayPin, LOW);
    digitalWrite(ledPin, HIGH);

    Serial.println("Peringatan: Aktuator Aktif!");
  } 
  else {
    digitalWrite(relayPin, HIGH);
    digitalWrite(ledPin, LOW);

    Serial.println("Kondisi Aman");
  }

  Serial.println("----------------------");

    delay(2500);
  }