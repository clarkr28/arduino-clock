void setup() {
  pinMode(13, OUTPUT);
  pinMode(21, OUTPUT);
}

void loop() {
  digitalWrite(13, HIGH);
  digitalWrite(21, HIGH);
  delay(2000);
  digitalWrite(13, LOW);
  digitalWrite(21, LOW);
  delay(2000);
}
