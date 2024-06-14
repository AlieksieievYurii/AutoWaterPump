#ifndef __MOTOR_H__
#define __MOTOR_H__

#define VOLUME_IN_ML_PER_SECOND 35

class Motor {
public:
  Motor(uint8_t pin) {
    _pin = pin;
  }

  void tick(void) {
    if (_time_to_finish != 0) {
      digitalWrite(_pin, HIGH);
      if (millis() >= _time_to_finish) {
        _time_to_finish = 0;
        digitalWrite(_pin, LOW);
      }
    } else {
      digitalWrite(_pin, LOW);
    }
  }

  void run(uint32_t volume_in_ml) {
    _time_to_finish = millis() + (((float)volume_in_ml / VOLUME_IN_ML_PER_SECOND) * 1000);
  }

private:
  uint8_t _pin = 0;
  uint32_t _time_to_finish = 0;
};

#endif