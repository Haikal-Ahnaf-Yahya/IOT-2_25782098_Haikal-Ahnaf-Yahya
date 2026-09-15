const int buttonPin = 4;
const int ledPin = 5;

int buttonState = 0;
int lastButtonState = LOW;
int ledState = LOW;

void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);

  // Saat pertama dinyalakan, LED mati
  digitalWrite(ledPin, LOW);
}

void loop() {
  buttonState = digitalRead(buttonPin);

  // Mendeteksi tombol baru ditekan
  if (buttonState == HIGH && lastButtonState == LOW) {
    
    // Membalik kondisi LED
    ledState = !ledState;
    digitalWrite(ledPin, ledState);

    if (ledState == HIGH) {
      Serial.println("Tombol ditekan -> LED ON");
    } else {
      Serial.println("Tombol ditekan -> LED OFF");
    }

    delay(50); // debounce tombol
  }

  lastButtonState = buttonState;
}s