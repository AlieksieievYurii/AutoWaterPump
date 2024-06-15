#ifndef __TRIGGER_H__
#define __TRIGGER_H__

#define MEASURE_TIME 2000

class Trigger {
public:
  Trigger(uint8_t pin) {
    _pin = pin;
  }

  bool get_state(void) {
    return _state;
  }

  void tick(void) {
    if (digitalRead(_pin) != _state) {

      if (!_measure) {
        _trigger_time = millis();
        _measure = true;
      }
    } else {
      _measure = false;
      _trigger_time = 0;
    }

    if (_measure && millis() - _trigger_time >= MEASURE_TIME) {
      _state = digitalRead(_pin);
      _measure = false;
    }
  }

private:
  bool _measure = false;
  uint8_t _pin = 0;
  uint32_t _trigger_time = 0;
  volatile bool _state = false;
};

#endif