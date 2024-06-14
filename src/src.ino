#include "motor.h"
#include "trigger.h"

#define MOTOR_L 23
#define MOTOR_R 33
#define BUZZER 19
#define BUTTON 18
#define SENSOR_L 34
#define SENSOR_R 35
#define STATUS_LED 13
#define TRIGGER 4

#define IS_TANK_EMPTY trigger.get_state() == false

Motor motor_l(MOTOR_L);
Motor motor_r(MOTOR_R);
Trigger trigger(TRIGGER);

void setup() {
  Serial.begin(9600);

  pinMode(MOTOR_L, OUTPUT);
  pinMode(MOTOR_R, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(TRIGGER, INPUT_PULLUP);
  pinMode(SENSOR_L, INPUT);
  pinMode(SENSOR_R, INPUT);
}

void loop() {
  if(Serial.available()) {
    uint32_t v = Serial.parseInt();
    Serial.println(v);
    motor_l.run(v);
    motor_r.run(v);
  }
  // if (digitalRead(BUTTON) && f == false) {
  //   f = true;
  //   s = !s;
  // }

  // if(digitalRead(BUTTON) == false) {
  //   f = false;
  // }
  // //Serial.println(s);
  // //Serial.print("BUT:");
  // //Serial.println(digitalRead(BUTTON));
  // //Serial.print("W:");
  Serial.println(IS_TANK_EMPTY);
  //delay(1000);
  //Serial.println("OK");
  // digitalWrite(BUZZER, HIGH);
  // digitalWrite(STATUS_LED, HIGH);
  // delay(1000);
  // digitalWrite(BUZZER, LOW);
  // digitalWrite(STATUS_LED, LOW);
  // digitalWrite(MOTOR_L, HIGH);
  // delay(5000);
  // digitalWrite(MOTOR_L, LOW);
  // delay(1000);
  //digitalWrite(MOTOR_L, s);
  // delay(5000);
  // digitalWrite(MOTOR_R, LOW);
  // delay(1000);
  motor_l.tick();
  motor_r.tick();
  trigger.tick();
}
