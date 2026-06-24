#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

const int trigpin = 6;
const int echopin = 7;

#define PCAADDR 0x70 //mux adr

void pcaselect(uint8_t channel) {
  if (channel > 7) return;
  Wire.beginTransmission(PCAADDR);
  Wire.write(1 << channel);
  int error = Wire.endTransmission();
  if (error == 0) return;
  else {
    while (1) Serial.println("dead");
  }
}

void setup() { 
  Serial1.setRX(17);
  Serial1.setTX(16);
  Serial1.begin(9600);

  Serial.begin(9600);
  Serial.println("Please work");

  Wire.setSDA(0);
  Wire.setSCL(1);
  Wire.begin();

  pinMode(trigpin, OUTPUT);
  pinMode(echopin, INPUT);

  pinMode(20, INPUT);   // Wait for button on cytron motion 2350 pro to be clicked to start
  while (digitalRead(20));

  Serial1.print('s');

  delay(1000);
  Serial.println("STARTING");
}

int cmap(int val) {
  int mapped = map(val, 0, 100, 0, 255);
  return constrain(mapped, 0, 255);
}

struct Motor {
  uint8_t fpin;
  uint8_t rpin;
  void speed(int val);
};

void Motor::speed(int val) {
  int map_speed = cmap(abs(val));
  if(val > 0) {
    analogWrite(fpin, map_speed);
    analogWrite(rpin, 0);
  }
  else {
    analogWrite(fpin, 0);
    analogWrite(rpin, map_speed);
  }
}

Motor Lmotor{8, 9};
Motor Rmotor{10, 11};

int count = 0;
  long duration;
  int distance;//, count;
  char c;
  int lspd, rspd;
  int tofDist, sideDist;
  uint16_t Red, Green, Blue, Clear;
  float pitch;
  
void loop() {
  digitalWrite(trigpin, LOW);  // HC-SR04 distance sensor
  delayMicroseconds(2);

  digitalWrite(trigpin, HIGH);
  delayMicroseconds(10);

  digitalWrite(trigpin, LOW);

  duration = pulseIn(echopin, HIGH, 5000);
  distance = (duration / 29) / 2;

  if (Serial1.available()) {    
    lspd = Serial1.parseInt(); // Reading motor speeds from a buffer
    rspd = Serial1.parseInt();

    if (lspd >= 100) { // Check to prevent high speeds
      lspd = 99;
    }
    if (rspd >= 100) {
      rspd = 99;
    }

    Serial.print(lspd);
    Serial.print(", ");
    Serial.println(rspd);
    Lmotor.speed(lspd); // Sets motors to read speed
    Rmotor.speed(rspd);
    
    while (Serial1.available()) {
      Serial1.read();
    }

    Serial.println(distance);

    Serial1.print(distance); // Sends distance sensor values over buffer
    Serial1.print(',');
    Serial1.print(1);


  }
  else {
    Serial.println("serial1 not available");
  }

  //delay(10);
}
