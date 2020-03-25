
#define IN1 4
#define IN2 5
#define IN3 6
#define IN4 7

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  delay(1000);
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("Starting");

  relaytest(IN1);
  relaytest(IN2);
  relaytest(IN3);
  relaytest(IN4);
}

void relaytest(int pin) {
  Serial.print("IN");
  Serial.println(pin);
  Serial.println("  on");
  digitalWrite(pin, HIGH);
  delay(1000);
  digitalWrite(pin, LOW);
}
