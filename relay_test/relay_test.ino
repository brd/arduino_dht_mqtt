
#define IN1 4
#define IN2 5
#define IN3 6
#define IN4 7

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  delay(1000);
  pinMode(IN1,OUTPUT);
  pinMode(IN2,OUTPUT);
  pinMode(IN3,OUTPUT);
  pinMode(IN4,OUTPUT);
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
  delay(5000);
  Serial.println("  off");
  digitalWrite(pin, LOW);
  delay(5000);
}
