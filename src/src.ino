#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "time.h"
#include "sntp.h"
#include <ESPmDNS.h>

#include "motor.h"
#include "trigger.h"
#include "pulse.h"

#define MOTOR_L 23
#define MOTOR_R 33
#define BUZZER 19
#define BUTTON 18
#define SENSOR_L 34
#define SENSOR_R 35
#define STATUS_LED 13
#define TRIGGER 4

#define METHOD_NOT_ALLOWED_CODE 405
#define BAD_REQUEST_CODE 400
#define OK_CODE 200

#define NAME "water-pump"

#define CONFIG_NAMESPACE "c"
#define LEFT_PUMP_CONFIG_NAMESPACE "left"
#define RIGHT_PUMP_CONFIG_NAMESPACE "right"

#define KEY_TIMEZONE "tz"
#define KEY_DAYS "ds"
#define KEY_TIME_HOUR "th"
#define KEY_TIME_MINUTES "tm"
#define KEY_VOLUME "v"
#define KEY_SSID "ssid"
#define KEY_PASSWORD "pass"

#define IS_TANK_EMPTY trigger.get_state() == false

#define DEBUG

WebServer server(80);
Preferences preferences;
struct tm timeinfo;

Motor motor_l(MOTOR_L);
Motor motor_r(MOTOR_R);
Trigger trigger(TRIGGER);
Pulse led(STATUS_LED);
Pulse buzzer(BUZZER);


const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const char* default_time_zone = "CET-1CEST";

uint8_t right_days = 0;
uint8_t right_hours = 0;
uint8_t right_minutes = 0;
uint16_t right_volume = 0;
bool right_pump_handled = false;

uint8_t left_days = 0;
uint8_t left_hours = 0;
uint8_t left_minutes = 0;
uint16_t left_volume = 0;
bool left_pump_handled = false;

StaticJsonDocument<250> jsonDocument;

bool pairing_mode = false;

void load_left_pump_config(void) {
  preferences.begin(LEFT_PUMP_CONFIG_NAMESPACE, true);
  left_days = preferences.getUInt(KEY_DAYS, 0);
  left_hours = preferences.getUInt(KEY_TIME_HOUR, 0);
  left_minutes = preferences.getUInt(KEY_TIME_MINUTES, 0);
  left_volume = preferences.getUInt(KEY_VOLUME, 0);
  preferences.end();
}

void load_right_pump_config(void) {
  preferences.begin(RIGHT_PUMP_CONFIG_NAMESPACE, true);
  right_days = preferences.getUInt(KEY_DAYS, 0);
  right_hours = preferences.getUInt(KEY_TIME_HOUR, 0);
  right_minutes = preferences.getUInt(KEY_TIME_MINUTES, 0);
  right_volume = preferences.getUInt(KEY_VOLUME, 0);
  preferences.end();
}

void help() {
  String message = "Doesn't match endpoints:\n";
  message += "/set - set scheduler!\n";
  message += "/get - get config!\n";
  message += "/wifi - set WIFI cred!\n";
  server.send(404, "text/plain", message);
}

void validate_and_set(uint8_t target, StaticJsonDocument<100>& doc) {
  if (!doc.containsKey(KEY_DAYS) || !doc.containsKey(KEY_TIME_HOUR) || !doc.containsKey(KEY_TIME_MINUTES) || !doc.containsKey(KEY_VOLUME)) {
    server.send(BAD_REQUEST_CODE, "text/plain", "Bad config");
    return;
  }

  uint8_t ds = doc[KEY_DAYS];

  if (ds > 0x7F) {
    server.send(BAD_REQUEST_CODE, "text/plain", "Bad days format");
    return;
  }

  uint8_t th = doc[KEY_TIME_HOUR];

  if (th > 23) {
    server.send(BAD_REQUEST_CODE, "text/plain", "Bad hour");
    return;
  }

  uint8_t tm = doc[KEY_TIME_MINUTES];

  if (tm > 59) {
    server.send(BAD_REQUEST_CODE, "text/plain", "Bad minutes");
    return;
  }

  uint16_t volume = doc[KEY_VOLUME];

  if (target == MOTOR_L) {
    preferences.begin(LEFT_PUMP_CONFIG_NAMESPACE, false);
  } else if (target == MOTOR_R) {
    preferences.begin(RIGHT_PUMP_CONFIG_NAMESPACE, false);
  } else
    return;

  preferences.putUInt(KEY_DAYS, ds);
  preferences.putUInt(KEY_TIME_HOUR, th);
  preferences.putUInt(KEY_TIME_MINUTES, tm);
  preferences.putUInt(KEY_VOLUME, volume);

  preferences.end();

  if (target == MOTOR_L) {
    load_left_pump_config();
  } else if (target == MOTOR_R) {
    load_right_pump_config();
  }
}

void update_scheduler() {
  StaticJsonDocument<100> internal_buff;
  if (server.method() != HTTP_POST) {
    server.send(METHOD_NOT_ALLOWED_CODE, "text/plain", "Only POST");
    return;
  }

  const String body = server.arg("plain");

  DeserializationError err = deserializeJson(jsonDocument, body);

  if (err) {
    String error = "Failed to parse: ";
    error += err.f_str();
    server.send(BAD_REQUEST_CODE, "text/plain", error);
    return;
  }

  if (jsonDocument.containsKey("left")) {
    const String data = jsonDocument["left"];
    deserializeJson(internal_buff, data);
    validate_and_set(MOTOR_L, internal_buff);
  }

  if (jsonDocument.containsKey("right")) {
    const String data = jsonDocument["right"];
    deserializeJson(internal_buff, data);
    validate_and_set(MOTOR_R, internal_buff);
  }

  if (jsonDocument.containsKey(KEY_TIMEZONE)) {
    const String time_zone = jsonDocument[KEY_TIMEZONE];
    preferences.begin(CONFIG_NAMESPACE, false);
    preferences.putString(KEY_TIMEZONE, time_zone);
    preferences.end();
    setup_and_sync_time();
  }

  server.send(OK_CODE, "text/plain", "OK");
}

void get_data() {
  JsonDocument left;
  preferences.begin(LEFT_PUMP_CONFIG_NAMESPACE, true);
  left[KEY_DAYS] = preferences.getUInt(KEY_DAYS, 0);
  left[KEY_TIME_HOUR] = preferences.getUInt(KEY_TIME_HOUR, 0);
  left[KEY_TIME_MINUTES] = preferences.getUInt(KEY_TIME_MINUTES, 0);
  left[KEY_VOLUME] = preferences.getUInt(KEY_VOLUME, 0);
  preferences.end();

  JsonDocument right;
  preferences.begin(RIGHT_PUMP_CONFIG_NAMESPACE, true);
  right[KEY_DAYS] = preferences.getUInt(KEY_DAYS, 0);
  right[KEY_TIME_HOUR] = preferences.getUInt(KEY_TIME_HOUR, 0);
  right[KEY_TIME_MINUTES] = preferences.getUInt(KEY_TIME_MINUTES, 0);
  right[KEY_VOLUME] = preferences.getUInt(KEY_VOLUME, 0);
  preferences.end();

  preferences.begin(CONFIG_NAMESPACE, true);
  String time_zone = preferences.getString(KEY_TIMEZONE, default_time_zone);
  preferences.end();

  getLocalTime(&timeinfo);
  char current_time[40];
  strftime(current_time, sizeof(current_time), "%A, %B %d %Y %H:%M:%S", &timeinfo);

  JsonDocument resp;

  resp["left"] = left;
  resp["right"] = right;
  resp["ct"] = current_time;
  resp["tz"] = time_zone;

  char output[200];
  serializeJson(resp, output);

  server.send(OK_CODE, "text/plain", output);
}

void wifi() {

  if (server.method() == HTTP_POST) {
    String ssid = server.arg(KEY_SSID);
    String password = server.arg(KEY_PASSWORD);

    preferences.begin(CONFIG_NAMESPACE, false);
    preferences.putString(KEY_SSID, ssid);
    preferences.putString(KEY_PASSWORD, password);
    preferences.end();

    server.send(OK_CODE, "text/html", "<p>OK. Reboot!</p>");
  } else {
    String m = "<!DOCTYPE html> <html lang=\"en\"> <head> <meta charset=\"UTF-8\"> <title>WiFi Config</title> </head> <body> <form action=\"/wifi\" method=\"POST\"> <label for=\"ssid\">SSID:</label> <input type=\"text\" id=\"ssid\" name=\"ssid\" required><br> <label for=\"pass\">Password:</label> <input type=\"password\" id=\"pass\" name=\"pass\" required><br> <input type=\"submit\" value=\"Submit\"> </form> </body> </html>";
    server.send(OK_CODE, "text/html", m);
  }
}

void setup_and_sync_time(void) {
  sntp_servermode_dhcp(1);
  preferences.begin(CONFIG_NAMESPACE, true);
  String time_zone = preferences.getString(KEY_TIMEZONE, default_time_zone);
  preferences.end();
  char buffer[20] = { 0 };
  time_zone.toCharArray(buffer, sizeof(buffer));
  configTzTime(buffer, ntpServer1, ntpServer2);
}

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

  pairing_mode = digitalRead(BUTTON);

  if (pairing_mode) {
    WiFi.softAP(NAME);
    led.set(PULSE_1000MS);
  } else {
    WiFi.mode(WIFI_AP_STA);

    preferences.begin(CONFIG_NAMESPACE, true);
    String ssid = preferences.getString(KEY_SSID, "");
    String password = preferences.getString(KEY_PASSWORD, "");
    preferences.end();

    WiFi.begin(ssid, password);

    setup_and_sync_time();

    led.on();
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
#ifdef DEBUG
      Serial.print(".");
#endif
    }
    led.off();
#ifdef DEBUG
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
#endif

    if (!MDNS.begin(NAME)) {
      led.set(PULSE_100MS);
    }
  }
  server.on("/set", update_scheduler);
  server.on("/get", get_data);
  server.on("/wifi", wifi);
  server.onNotFound(help);
  server.begin();

  if (!pairing_mode) {
    buzzer.on();
    led.on();

    while (!getLocalTime(&timeinfo)) {
#ifdef DEBUG
      Serial.println("No time available (yet)");
#endif
      delay(500);
    }

    buzzer.off();
    led.off();

#ifdef DEBUG
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
#endif
  }

  load_left_pump_config();
  load_right_pump_config();

#ifdef DEBUG
  Serial.println("Left pump:");
  Serial.print("- ds:");
  Serial.println(left_days);
  Serial.print("- th:");
  Serial.println(left_hours);
  Serial.print("- tm:");
  Serial.println(left_minutes);
  Serial.print("- vol:");
  Serial.println(left_volume);

  Serial.println("Right pump:");
  Serial.print("- ds:");
  Serial.println(right_days);
  Serial.print("- th:");
  Serial.println(right_hours);
  Serial.print("- tm:");
  Serial.println(right_minutes);
  Serial.print("- vol:");
  Serial.println(right_volume);
#endif
}

void loop() {
  if (!pairing_mode) {
    getLocalTime(&timeinfo);

    if (IS_TANK_EMPTY) {
      led.set(PULSE_1000MS);
      buzzer.set(PULSE_1000MS);
    } else {
      led.set(PULSE_STOP);
      buzzer.set(PULSE_STOP);
    }

    if (!IS_TANK_EMPTY && (right_days & (1 << timeinfo.tm_wday)) && right_hours == timeinfo.tm_hour && right_minutes == timeinfo.tm_min) {
      if (!right_pump_handled) {
        motor_r.run(right_volume);
        right_pump_handled = true;
      }
    } else {
      right_pump_handled = false;
    }

    if (!IS_TANK_EMPTY && (left_days & (1 << timeinfo.tm_wday)) && left_hours == timeinfo.tm_hour && left_minutes == timeinfo.tm_min) {
      if (!left_pump_handled) {
        motor_l.run(left_volume);
        left_pump_handled = true;
      }
    } else {
      left_pump_handled = false;
    }
  }

  motor_l.tick();
  motor_r.tick();
  trigger.tick();
  led.tick();
  buzzer.tick();
  server.handleClient();
}
