const int buttonPin = 4;
const int ledPin = 5;

bool ledState = false;      // Menyimpan status LED (false = OFF, true = ON)
int lastButtonState = LOW;  // Menyimpan status tombol sebelumnya

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); // LED mati saat pertama dinyalakan
}

void loop() {
  int currentButtonState = digitalRead(buttonPin);

  // Mendeteksi saat tombol baru mulai ditekan (dari LOW ke HIGH)
  if (lastButtonState == LOW && currentButtonState == HIGH) {
    ledState = !ledState;                   // Membalik status LED (ON <-> OFF)
    digitalWrite(ledPin, ledState);         // Memperbarui kondisi fisik LED

    if (ledState) {
      Serial.println("Tombol ditekan! -> LED ON");
    } else {
      Serial.println("Tombol ditekan! -> LED OFF");
    }

    delay(200); // Debouncing untuk mencegah pembacaan ganda akibat pantulan mekanis
  }

  lastButtonState = currentButtonState; // Menyimpan kondisi tombol saat ini
}