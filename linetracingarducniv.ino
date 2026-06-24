#include <Wire.h>
//#include <VL53L0X.h>
//#include "Adafruit_APDS9960.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

const int trigpin = 6;
const int echopin = 7;

#define PCAADDR 0x70 //mux adr

//VL53L0X tof;
//, tofSide;
//Adafruit_APDS9960 color;

//Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);

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

  

  /* pcaselect(3);
    tofSide.setTimeout(500);
    if (!tofSide.init()) {
     Serial.println("Failed to detect and initialize sensor!");
     while (1) {}

    }



  /* pcaselect(2);
    if (color.begin(50, APDS9960_AGAIN_4X)) {}//50, APDS9960_AGAIN_4X)) { }
    else {
       Serial.println("No left color sensor found");
       while (1)
           ;

    }
    color.enableColor(true);*/

  /*pcaselect(0);
    if(!bno.begin())
    {
    /* There was a problem detecting the BNO055 ... check your connections
    Serial.print("Ooops, no BNO055 detected ... Check your wiring or I2C ADDR!");
    while(1);
    }

    bno.setExtCrystalUse(true);*/

  pinMode(20, INPUT);
  while (digitalRead(20));

  Serial1.print('s');

  delay(1000);
  Serial.println("STARTING");
  //bno.setExtCrystalUse(true);
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

  
  digitalWrite(trigpin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigpin, HIGH);
  delayMicroseconds(10);

  digitalWrite(trigpin, LOW);

  duration = pulseIn(echopin, HIGH, 5000);
  distance = (duration / 29) / 2;
  

  /* pcaselect(3);
    Serial.print("Lower TOF: ");
    sideDist = tofSide.readRangeContinuousMillimeters();
    Serial.print(sideDist);
    if (tofSide.timeoutOccurred()) {
    Serial.print("LOWER TIMEOUT");
    }

    Serial.print("  ");*/


  /* pcaselect(0);
    sensors_event_t event;
    bno.getEvent(&event);
    pitch = (float)event.orientation.y;
    Serial.println(pitch);*/


  /*  pcaselect(2);
    while (!color.colorDataReady()) {
        delay(5);  //wait for color dat to be ready
    }

    color.getColorData(&Red, &Green, &Blue, &Clear);*/

  if (Serial1.available()) {
    //Serial1.println((char)Serial1.read());
    //Serial.print((char)Serial1.read());
    //Serial1.read();
    
    //count = Serial1.parseInt();
    
    lspd = Serial1.parseInt();
    rspd = Serial1.parseInt();


    /* if(pitch <= -20) {
       Serial.println("< -20");
       lspd *= 3;
       rspd *= 3;
      }
      else if(pitch <= -16) {
       Serial.println("< -16");
       lspd *= 2;
       rspd *= 2;
      }
      else if(pitch > 15 && tofDist > 125) {
       lspd /= 6;
       rspd /= 6;
      }*/
      
    if (lspd >= 100) {
      lspd = 99;
    }
    if (rspd >= 100) {
      rspd = 99;
    }


    Serial.print(lspd);
    Serial.print(", ");
    Serial.println(rspd);
    Lmotor.speed(lspd);
    Rmotor.speed(rspd);
    
    while (Serial1.available()) {
      Serial1.read();
    }

    Serial.println(distance);

    Serial1.print(distance);
    Serial1.print(',');
    Serial1.print(1);


  }
  else {
    Serial.println("serial1 not available");
  }

  //delay(10);
}
