#ifndef __PULSE_H__
#define __PULSE_H__

#define PULSE_STOP 0
#define PULSE_100MS 1
#define PULSE_1000MS 2

class Pulse {
public:
  Pulse(uint8_t pin) {
    _pin = pin;
  }

  void tick(void) {
    if (_delay != 0 && millis() - _timestamp >= _delay) {
      _state = !_state;
      _timestamp = millis();
    }
    digitalWrite(_pin, _state);
  }

  void on(void) {
    digitalWrite(_pin, HIGH);
  }

  void off(void) {
    digitalWrite(_pin, LOW);
  }

  void set(uint8_t mode) {
    if (mode == _mode)
      return;
    _timestamp = millis();
    _mode = mode;
    if (mode == PULSE_STOP) {
      _state = false;
      _delay = 0;
    } else if (mode == PULSE_100MS) {
      _delay = 100;
    } else if (mode == PULSE_1000MS) {
      _delay = 1000;
    }
  }

private:
  uint8_t _pin = 0;
  uint8_t _mode = 0;
  bool _state = false;
  uint32_t _delay = 0;
  uint32_t _timestamp = 0;
};

#endif