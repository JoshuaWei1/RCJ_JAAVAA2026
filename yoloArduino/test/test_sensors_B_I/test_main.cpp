#include <Arduino.h>
#include <Wire.h>
#include <unity.h>

#define SDA_PIN 0
#define SCL_PIN 1
#define MUXADDR 0x70
#define OLEDADDR 0x3D
#define APPDSADDR 0x39
#define TOFADDR 0x29

void setUp(void)
{
  // set stuff up here
}

void tearDown(void)
{
  // clean stuff up here
}


int tsaselect( int channel)
{
  Wire.beginTransmission(MUXADDR);
  Wire.write(1 << channel);
  return Wire.endTransmission();

}

void test_led_builtin_pin_number(void)
{
  TEST_ASSERT_EQUAL(3, LED_BUILTIN);
}


void test_OLED(void){
  Wire.beginTransmission(OLEDADDR);
  int err = Wire.endTransmission();
  delay(10);

  TEST_ASSERT_EQUAL(0, err);
}

void test_COLOR(void){
  Wire.beginTransmission(APPDSADDR);
  int err = Wire.endTransmission();
  delay(10);

  TEST_ASSERT_EQUAL(0, err);
}

void test_MUX(void){
  Wire.beginTransmission(MUXADDR);
  int err = Wire.endTransmission();
  delay(10);

  TEST_ASSERT_EQUAL(0, err);
}

void test_MUXChanSelect(void){
  int err = tsaselect(2);
  TEST_ASSERT_EQUAL(0, err);
}

void test_TOF(void){
  //int err = tsaselect(0);
  //TEST_ASSERT_EQUAL(0, err);
  Wire.beginTransmission(TOFADDR);
  int err = Wire.endTransmission();
  delay(10);

  TEST_ASSERT_EQUAL(0, err);
}

void test_led_state_high(void)
{
  digitalWrite(LED_BUILTIN, HIGH);
  TEST_ASSERT_EQUAL(HIGH, digitalRead(LED_BUILTIN));
}

void test_led_state_low(void)
{
  digitalWrite(LED_BUILTIN, LOW);
  TEST_ASSERT_EQUAL(LOW, digitalRead(LED_BUILTIN));
}

void setup()
{
  // NOTE!!! Wait for >2 secs
  // if board doesn't support software reset via Serial.DTR/RTS
  delay(2000);

  Wire.setSDA(SDA_PIN);
  Wire.setSCL(SCL_PIN);
  Wire.begin();
  
  pinMode(LED_BUILTIN, OUTPUT);

  UNITY_BEGIN(); // IMPORTANT LINE!
  RUN_TEST(test_led_builtin_pin_number);
}


uint8_t max_blinks = 2;

void loop()
{
  if (max_blinks--)
  {
    // RUN_TEST(test_led_state_high);
    // delay(50);
    // RUN_TEST(test_led_state_low);
    // delay(50);
    RUN_TEST(test_OLED);
    delay(50);
    RUN_TEST(test_COLOR);
    delay(50);
    RUN_TEST(test_TOF);
    delay(50);
    // RUN_TEST(test_MUX);
    // delay(50);
    // RUN_TEST(test_MUXChanSelect);
    // delay(50); 

  }
  else 
  {
    UNITY_END(); // stop unit testing
  }
}