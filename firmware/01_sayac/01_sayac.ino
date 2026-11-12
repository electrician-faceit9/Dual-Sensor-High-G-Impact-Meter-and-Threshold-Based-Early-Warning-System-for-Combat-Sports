uint32_t n = 0;

void setup() {
  Serial.begin(115200);
}

void loop() {
  Serial.println(n++);
  delay(1000);
}
