#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
#include <sys/time.h>
#include <DHT.h>
#include <max6675.h>
#include "pins.h"
#include "web_page.h"

#ifndef SGK_FW_VERSION
#define SGK_FW_VERSION "dev"
#endif
#ifndef SGK_DEVICE_ID
#define SGK_DEVICE_ID "sgk"
#endif
#ifndef SGK_HEATER_ENABLED
#define SGK_HEATER_ENABLED 0
#endif
#ifndef SGK_BENCH_SIM
#define SGK_BENCH_SIM 0
#endif

static const char *AP_SSID = "GrowKit-PoC";
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_GW(192, 168, 4, 1);
static const IPAddress AP_MASK(255, 255, 255, 0);

static const uint32_t HEARTBEAT_MS = 2000;
static const uint32_t SAMPLE_MS = 5000;
static const uint32_t NVS_CLOCK_MS = 60000;
static const float WATER_LOW_PCT = 20.0f;

static const uint32_t PUMP_ON_MS = 15UL * 60UL * 1000UL;
static const uint32_t PUMP_CYCLE_MS = 60UL * 60UL * 1000UL;
static const uint32_t DOSE_MS_DEFAULT = 500;
static const uint32_t DOSE_MS_MAX = 2000;
static const uint32_t DOSE_SETTLE_MS = 15UL * 60UL * 1000UL;
static const time_t EPOCH_MIN = 1600000000;  // 2020-09

DHT dht(PIN_DHT, DHT11);
MAX6675 thermo(PIN_MAX_SCK, PIN_MAX_CS, PIN_MAX_SO);
WebServer server(80);
Preferences prefs;

enum Mode { MODE_SAFE, MODE_AUTO, MODE_MANUAL };
static Mode mode = MODE_SAFE;

static bool pumpOn = false;
static bool lightOn = false;
static bool heatOn = false;
static bool fanOn = false;
static bool waterLock = true;
static bool manPump = false;
static bool manLight = false;
static bool manFan = false;
static bool clockOk = false;
static uint32_t lastEpoch = 0;
static uint8_t lightOnHour = 6;
static uint8_t lightOnHours = 16;
static char doseActive = 0;
static char doseLast = 0;
static uint32_t dosePulseUntil = 0;
static uint32_t doseSettleUntil = 0;
static uint32_t doseLastMs = DOSE_MS_DEFAULT;

static float airC = NAN, rh = NAN, waterC = NAN, ph = NAN, levelPct = NAN;
static float simLevelPct = 80.0f;
static float simPh = NAN;
static float simAirC = NAN;
static float simWaterC = NAN;

static void outOff(int pin) {
  if (pin < 0) return;
  digitalWrite(pin, LOW);
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

static void relayOff(int pin) {
  if (pin < 0) return;
  digitalWrite(pin, RELAY_OFF_LEVEL);
  pinMode(pin, OUTPUT);
  digitalWrite(pin, RELAY_OFF_LEVEL);
}

static void relayWrite(int pin, bool on) {
  if (pin < 0) return;
  digitalWrite(pin, on ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
}

static bool waterTooLow() {
  return isnan(levelPct) || levelPct < WATER_LOW_PCT;
}

static void dosingOff() {
  const int doses[] = {
    PIN_DOSE_A_IN1, PIN_DOSE_A_IN2, PIN_DOSE_B_IN1, PIN_DOSE_B_IN2,
    PIN_DOSE_C_IN1, PIN_DOSE_C_IN2
  };
  for (int pin : doses) {
    if (pin >= 0) digitalWrite(pin, LOW);
  }
}

static bool dosePins(char ch, int *in1, int *in2) {
  if (ch == 'A' || ch == 'a') {
    *in1 = PIN_DOSE_A_IN1;
    *in2 = PIN_DOSE_A_IN2;
    return *in1 >= 0 && *in2 >= 0;
  }
  if (ch == 'B' || ch == 'b') {
    *in1 = PIN_DOSE_B_IN1;
    *in2 = PIN_DOSE_B_IN2;
    return *in1 >= 0 && *in2 >= 0;
  }
  return false;
}

static void doseMotorOn(char ch) {
  int in1 = -1, in2 = -1;
  if (!dosePins(ch, &in1, &in2)) return;
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
}

static uint32_t doseSettleLeftS(uint32_t now) {
  if ((int32_t)(now - doseSettleUntil) >= 0) return 0;
  return (doseSettleUntil - now + 999) / 1000;
}

static void endDosePulse(uint32_t now, bool startSettle) {
  dosingOff();
  if (dosePulseUntil != 0 && startSettle) {
    doseSettleUntil = now + DOSE_SETTLE_MS;
  }
  dosePulseUntil = 0;
  doseActive = 0;
}

static void tickDose(uint32_t now) {
  if (PIN_DOSE_C_IN1 >= 0) digitalWrite(PIN_DOSE_C_IN1, LOW);
  if (PIN_DOSE_C_IN2 >= 0) digitalWrite(PIN_DOSE_C_IN2, LOW);

  if (dosePulseUntil == 0) {
    dosingOff();
    return;
  }
  if (waterTooLow() || mode != MODE_MANUAL) {
    endDosePulse(now, true);
    return;
  }
  if ((int32_t)(now - dosePulseUntil) >= 0) {
    endDosePulse(now, true);
    return;
  }
}

static void setHeater(bool on) {
  if (!SGK_HEATER_ENABLED) on = false;
  heatOn = on;
  relayWrite(PIN_RLY_HEAT, on);
}

static void allOutputsOff() {
  relayWrite(PIN_RLY_PUMP, false);
  relayWrite(PIN_RLY_LIGHT, false);
  relayWrite(PIN_RLY_FAN, false);
  setHeater(false);
  endDosePulse(millis(), dosePulseUntil != 0);
  dosingOff();
  pumpOn = lightOn = fanOn = false;
  manPump = manLight = manFan = false;
}

static bool scheduleOn(uint32_t now, uint32_t onMs, uint32_t cycleMs) {
  return (now % cycleMs) < onMs;
}

static void formatHm(char *buf, size_t n) {
  if (!clockOk) {
    snprintf(buf, n, "--:--");
    return;
  }
  time_t t = time(nullptr);
  if (t < EPOCH_MIN) {
    snprintf(buf, n, "--:--");
    return;
  }
  struct tm tmnow;
  localtime_r(&t, &tmnow);
  snprintf(buf, n, "%02d:%02d", tmnow.tm_hour, tmnow.tm_min);
}

static bool applyEpoch(time_t epoch) {
  if (epoch < EPOCH_MIN) return false;
  struct timeval tv;
  tv.tv_sec = epoch;
  tv.tv_usec = 0;
  if (settimeofday(&tv, nullptr) != 0) return false;
  lastEpoch = (uint32_t)epoch;
  clockOk = true;
  prefs.putBool("clock_ok", true);
  prefs.putULong("last_epoch", lastEpoch);
  return true;
}

static bool parseIsoLocal(const char *iso, time_t *out) {
  int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
  int n = sscanf(iso, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s);
  if (n < 5) n = sscanf(iso, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s);
  if (n < 5) return false;
  if (y < 2020 || y > 2099 || mo < 1 || mo > 12 || d < 1 || d > 31) return false;
  if (h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 59) return false;
  struct tm tmv;
  memset(&tmv, 0, sizeof(tmv));
  tmv.tm_year = y - 1900;
  tmv.tm_mon = mo - 1;
  tmv.tm_mday = d;
  tmv.tm_hour = h;
  tmv.tm_min = mi;
  tmv.tm_sec = s;
  tmv.tm_isdst = 0;
  time_t epoch = mktime(&tmv);
  if (epoch == (time_t)-1) return false;
  *out = epoch;
  return true;
}

static bool lightScheduleOn() {
  if (!clockOk) return false;
  time_t t = time(nullptr);
  if (t < EPOCH_MIN) return false;
  struct tm tmnow;
  localtime_r(&t, &tmnow);
  const int minutes = tmnow.tm_hour * 60 + tmnow.tm_min;
  const int start = (int)lightOnHour * 60;
  const int dur = (int)lightOnHours * 60;
  const int day = 24 * 60;
  const int elapsed = (minutes - start + day) % day;
  return elapsed < dur;
}

static void nvsSaveSetpoints() {
  prefs.putUChar("mode", (uint8_t)mode);
  prefs.putBool("man_pump", manPump);
  prefs.putBool("man_light", manLight);
  prefs.putBool("man_fan", manFan);
  prefs.putUChar("l_on_h", lightOnHour);
  prefs.putUChar("l_on_hrs", lightOnHours);
}

static void nvsLoad() {
  prefs.begin("sgk", false);
  clockOk = prefs.getBool("clock_ok", false);
  lastEpoch = prefs.getULong("last_epoch", 0);
  const uint8_t savedMode = prefs.getUChar("mode", (uint8_t)MODE_AUTO);
  if (savedMode <= (uint8_t)MODE_MANUAL) mode = (Mode)savedMode;
  else mode = MODE_AUTO;
  manPump = prefs.getBool("man_pump", false);
  manLight = prefs.getBool("man_light", false);
  manFan = prefs.getBool("man_fan", false);
  lightOnHour = prefs.getUChar("l_on_h", 6);
  lightOnHours = prefs.getUChar("l_on_hrs", 16);
  if (lightOnHours == 0 || lightOnHours > 24) lightOnHours = 16;
  if (lightOnHour > 23) lightOnHour = 6;

  if (clockOk && lastEpoch >= (uint32_t)EPOCH_MIN) {
    struct timeval tv;
    tv.tv_sec = (time_t)lastEpoch;
    tv.tv_usec = 0;
    if (settimeofday(&tv, nullptr) != 0) clockOk = false;
  } else {
    clockOk = false;
  }
}

static void persistClockIfDue(uint32_t now) {
  static uint32_t lastSave = 0;
  if (!clockOk) return;
  if (now - lastSave < NVS_CLOCK_MS) return;
  lastSave = now;
  time_t t = time(nullptr);
  if (t < EPOCH_MIN) return;
  lastEpoch = (uint32_t)t;
  prefs.putULong("last_epoch", lastEpoch);
}

static void applyLocalControl(uint32_t now) {
  waterLock = waterTooLow();
  setHeater(false);
  tickDose(now);

  if (mode == MODE_SAFE) {
    pumpOn = false;
    lightOn = false;
    fanOn = false;
    manPump = manLight = manFan = false;
    relayWrite(PIN_RLY_PUMP, false);
    relayWrite(PIN_RLY_LIGHT, false);
    relayWrite(PIN_RLY_FAN, false);
    return;
  }

  if (mode == MODE_MANUAL) {
    if (waterLock) manPump = false;
    pumpOn = manPump;
    lightOn = manLight;
    fanOn = manFan;
    relayWrite(PIN_RLY_PUMP, pumpOn);
    relayWrite(PIN_RLY_LIGHT, lightOn);
    relayWrite(PIN_RLY_FAN, fanOn);
    return;
  }

  lightOn = lightScheduleOn();
  pumpOn = !waterLock && scheduleOn(now, PUMP_ON_MS, PUMP_CYCLE_MS);
  fanOn = false;
  relayWrite(PIN_RLY_LIGHT, lightOn);
  relayWrite(PIN_RLY_PUMP, pumpOn);
  relayWrite(PIN_RLY_FAN, false);
}

static void setupOutputs() {
  const int relays[] = {PIN_RLY_PUMP, PIN_RLY_LIGHT, PIN_RLY_HEAT, PIN_RLY_FAN};
  for (int pin : relays) {
    relayOff(pin);
  }
  const int doses[] = {
    PIN_DOSE_A_IN1, PIN_DOSE_A_IN2, PIN_DOSE_B_IN1, PIN_DOSE_B_IN2,
    PIN_DOSE_C_IN1, PIN_DOSE_C_IN2
  };
  for (int pin : doses) {
    outOff(pin);
  }
  if (PIN_LED_STATUS >= 0) {
    digitalWrite(PIN_LED_STATUS, LOW);
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);
  }
  if (PIN_US_TRIG >= 0) {
    digitalWrite(PIN_US_TRIG, LOW);
    pinMode(PIN_US_TRIG, OUTPUT);
    digitalWrite(PIN_US_TRIG, LOW);
  }
  if (PIN_US_ECHO >= 0) {
    pinMode(PIN_US_ECHO, INPUT);
  }
}

static float readLevelPct() {
  if (PIN_US_TRIG < 0 || PIN_US_ECHO < 0) return NAN;
  digitalWrite(PIN_US_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_US_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_US_TRIG, LOW);
  unsigned long us = pulseIn(PIN_US_ECHO, HIGH, 30000UL);
  if (us == 0) return NAN;
  float cm = us / 58.0f;
  // Empty ~25 cm, full ~4 cm — tune after install
  float pct = (25.0f - cm) * 100.0f / 21.0f;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

static float readPh() {
  if (PIN_PH_ADC < 0) return NAN;
  uint32_t acc = 0;
  for (int i = 0; i < 16; i++) acc += analogRead(PIN_PH_ADC);
  float v = (acc / 16.0f) * 3.3f / 4095.0f;
  // Placeholder linear map — replace after buffer calibration
  return 7.0f + ((2.5f - v) * 3.5f);
}

static void applyBenchSim() {
  if (!SGK_BENCH_SIM) return;
  if (isnan(levelPct) && !isnan(simLevelPct)) levelPct = simLevelPct;
  if (isnan(ph) && !isnan(simPh)) ph = simPh;
  if (isnan(airC) && !isnan(simAirC)) airC = simAirC;
  if (isnan(waterC) && !isnan(simWaterC)) waterC = simWaterC;
}

static void sampleSensors() {
  airC = dht.readTemperature();
  rh = dht.readHumidity();
  waterC = thermo.readCelsius();
  ph = readPh();
  levelPct = readLevelPct();
  applyBenchSim();
}

static const char *modeName(Mode m) {
  switch (m) {
    case MODE_AUTO: return "auto";
    case MODE_MANUAL: return "manual";
    default: return "safe";
  }
}

static void jsonFloat(String &s, const char *key, float v, uint8_t digits) {
  s += '"';
  s += key;
  s += "\":";
  if (isnan(v)) s += "null";
  else s += String(v, (unsigned int)digits);
}

static void handleRoot() {
  server.send_P(200, "text/html", PAGE_HTML);
}

static void handleStatus() {
  char hm[8];
  formatHm(hm, sizeof(hm));
  String s;
  s.reserve(640);
  s += "{\"device_id\":\"";
  s += SGK_DEVICE_ID;
  s += "\",\"fw\":\"";
  s += SGK_FW_VERSION;
  s += "\",\"mode\":\"";
  s += modeName(mode);
  s += "\",\"ip\":\"";
  s += WiFi.softAPIP().toString();
  s += "\",\"uptime_s\":";
  s += String(millis() / 1000UL);
  s += ",\"clock_ok\":";
  s += clockOk ? 1 : 0;
  s += ",\"clock\":\"";
  s += hm;
  s += "\",";
  jsonFloat(s, "air_c", airC, 1);
  s += ',';
  jsonFloat(s, "rh", rh, 0);
  s += ',';
  jsonFloat(s, "water_c", waterC, 1);
  s += ',';
  jsonFloat(s, "ph", ph, 2);
  s += ',';
  jsonFloat(s, "level_pct", levelPct, 0);
  s += ",\"pump\":";
  s += pumpOn ? 1 : 0;
  s += ",\"light\":";
  s += lightOn ? 1 : 0;
  s += ",\"heat\":";
  s += heatOn ? 1 : 0;
  s += ",\"fan\":";
  s += fanOn ? 1 : 0;
  s += ",\"water_lock\":";
  s += waterLock ? 1 : 0;
  s += ",\"heater_en\":";
  s += SGK_HEATER_ENABLED ? 1 : 0;
  s += ",\"bench_sim\":";
  s += SGK_BENCH_SIM ? 1 : 0;
  s += ",\"dose\":\"";
  if (doseActive) s += doseActive;
  else s += "off";
  s += "\",\"dose_last\":\"";
  if (doseLast) s += doseLast;
  s += "\",\"dose_ms\":";
  s += String(doseLastMs);
  s += ",\"dose_settle_s\":";
  s += String(doseSettleLeftS(millis()));
  s += '}';
  server.send(200, "application/json", s);
}

static void handleMode() {
  const String m = server.arg("m");
  if (m == "safe") {
    mode = MODE_SAFE;
    allOutputsOff();
  } else if (m == "auto") {
    mode = MODE_AUTO;
  } else if (m == "manual") {
    mode = MODE_MANUAL;
    manPump = pumpOn;
    manLight = lightOn;
    manFan = fanOn;
  } else {
    server.send(400, "text/plain", "bad mode");
    return;
  }
  applyLocalControl(millis());
  nvsSaveSetpoints();
  server.send(200, "text/plain", "ok");
}

static void handleAct() {
  if (mode != MODE_MANUAL) {
    server.send(409, "text/plain", "not manual");
    return;
  }
  const String ch = server.arg("ch");
  const bool on = server.arg("on") == "1";
  if (ch == "pump") manPump = on;
  else if (ch == "light") manLight = on;
  else if (ch == "fan") manFan = on;
  else {
    server.send(400, "text/plain", "bad ch");
    return;
  }
  applyLocalControl(millis());
  nvsSaveSetpoints();
  server.send(200, "text/plain", "ok");
}

static void handleDose() {
  if (mode != MODE_MANUAL) {
    server.send(409, "text/plain", "not manual");
    return;
  }
  if (waterTooLow()) {
    server.send(409, "text/plain", "water_lock");
    return;
  }
  const uint32_t now = millis();
  if (dosePulseUntil != 0) {
    server.send(409, "text/plain", "busy");
    return;
  }
  if (doseSettleLeftS(now) > 0) {
    server.send(409, "text/plain", "settle");
    return;
  }

  String chs = server.arg("ch");
  if (chs.length() == 0) chs = server.arg("channel");
  if (chs.length() == 0) {
    server.send(400, "text/plain", "bad ch");
    return;
  }
  char ch = chs.charAt(0);
  if (ch == 'a') ch = 'A';
  if (ch == 'b') ch = 'B';
  if (ch == 'C' || ch == 'c') {
    server.send(400, "text/plain", "dose C off");
    return;
  }
  int in1 = -1, in2 = -1;
  if (!dosePins(ch, &in1, &in2)) {
    server.send(400, "text/plain", "bad ch");
    return;
  }

  uint32_t ms = DOSE_MS_DEFAULT;
  String mss = server.arg("ms");
  if (mss.length() == 0) mss = server.arg("pulse");
  if (mss.length() == 0) mss = server.arg("len");
  if (mss.length() > 0) {
    ms = (uint32_t)strtoul(mss.c_str(), nullptr, 10);
    if (ms == 0) ms = DOSE_MS_DEFAULT;
  }
  if (ms > DOSE_MS_MAX) ms = DOSE_MS_MAX;

  doseLastMs = ms;
  doseLast = ch;
  doseActive = ch;
  dosePulseUntil = now + ms;
  doseMotorOn(ch);
  server.send(200, "text/plain", "ok");
}

static bool parseSimFloat(const char *name, float *out) {
  if (!server.hasArg(name) || server.arg(name).length() == 0) return false;
  *out = server.arg(name).toFloat();
  return true;
}

static void handleSim() {
  if (!SGK_BENCH_SIM) {
    server.send(404, "text/plain", "bench_sim off");
    return;
  }
  float v;
  if (parseSimFloat("level_pct", &v)) {
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    simLevelPct = v;
  }
  if (parseSimFloat("ph", &v)) simPh = v;
  if (parseSimFloat("air_c", &v)) simAirC = v;
  if (parseSimFloat("water_c", &v)) simWaterC = v;
  sampleSensors();
  applyLocalControl(millis());
  server.send(200, "text/plain", "ok");
}

static void handleClock() {
  time_t epoch = 0;
  bool ok = false;
  if (server.hasArg("iso") && server.arg("iso").length() > 0) {
    ok = parseIsoLocal(server.arg("iso").c_str(), &epoch);
  } else if (server.hasArg("epoch") && server.arg("epoch").length() > 0) {
    epoch = (time_t)strtoul(server.arg("epoch").c_str(), nullptr, 10);
    ok = epoch >= EPOCH_MIN;
  }
  if (!ok || !applyEpoch(epoch)) {
    server.send(400, "text/plain", "bad time");
    return;
  }
  applyLocalControl(millis());
  server.send(200, "text/plain", "ok");
}

static void handleNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

static void heartbeat() {
  char hm[8];
  formatHm(hm, sizeof(hm));
  Serial.print(F("device_id="));
  Serial.print(SGK_DEVICE_ID);
  Serial.print(F(" fw="));
  Serial.print(SGK_FW_VERSION);
  Serial.print(F(" mode="));
  Serial.print(modeName(mode));
  Serial.print(F(" uptime_s="));
  Serial.print(millis() / 1000UL);
  Serial.print(F(" clock_ok="));
  Serial.print(clockOk ? 1 : 0);
  Serial.print(F(" clock="));
  Serial.print(hm);
  Serial.print(F(" air_c="));
  Serial.print(airC, 1);
  Serial.print(F(" rh="));
  Serial.print(rh, 0);
  Serial.print(F(" water_c="));
  Serial.print(waterC, 1);
  Serial.print(F(" ph="));
  Serial.print(ph, 2);
  Serial.print(F(" level_pct="));
  Serial.print(levelPct, 0);
  Serial.print(F(" pump="));
  Serial.print(pumpOn ? 1 : 0);
  Serial.print(F(" light="));
  Serial.print(lightOn ? 1 : 0);
  Serial.print(F(" heat="));
  Serial.print(heatOn ? 1 : 0);
  Serial.print(F(" fan="));
  Serial.print(fanOn ? 1 : 0);
  Serial.print(F(" water_lock="));
  Serial.print(waterLock ? 1 : 0);
  Serial.print(F(" heater_en="));
  Serial.print(SGK_HEATER_ENABLED ? 1 : 0);
  Serial.print(F(" bench_sim="));
  Serial.print(SGK_BENCH_SIM ? 1 : 0);
  Serial.print(F(" dose="));
  if (doseActive) Serial.print(doseActive);
  else Serial.print(F("off"));
  Serial.print(F(" settle_s="));
  Serial.print(doseSettleLeftS(millis()));
  Serial.print(F(" sta="));
  Serial.println(WiFi.softAPgetStationNum());
}

static void setupApAndWeb() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GW, AP_MASK);
  const bool ok = WiFi.softAP(AP_SSID);
  Serial.print(F("ssid="));
  Serial.print(AP_SSID);
  Serial.print(F(" ap ip="));
  Serial.print(WiFi.softAPIP());
  Serial.print(F(" softAP="));
  Serial.println(ok ? F("ok") : F("FAIL"));

  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/mode", HTTP_POST, handleMode);
  server.on("/act", HTTP_POST, handleAct);
  server.on("/clock", HTTP_POST, handleClock);
  server.on("/dose", HTTP_POST, handleDose);
  server.on("/sim", HTTP_POST, handleSim);
  server.onNotFound(handleNotFound);
  server.begin();
}

void setup() {
  setupOutputs();
  allOutputsOff();
  mode = MODE_SAFE;
  Serial.begin(115200);
  delay(200);
  setenv("TZ", "GMT0", 1);
  tzset();
  dht.begin();
  analogReadResolution(12);
  Serial.println(F("sgk boot SAFE all outputs OFF"));
  Serial.print(F("fw "));
  Serial.print(SGK_FW_VERSION);
  Serial.println(F(" board=esp32dev usb=CP2102"));
  nvsLoad();
  Serial.print(F("nvs clock_ok="));
  Serial.print(clockOk ? 1 : 0);
  Serial.print(F(" last_epoch="));
  Serial.print(lastEpoch);
  Serial.print(F(" mode="));
  Serial.println(modeName(mode));
  Serial.println(F("sched light=16h/8h needs clock; pump=15m/45m millis; dose manual pulse only"));
  setupApAndWeb();
  sampleSensors();
  applyLocalControl(millis());
}

void loop() {
  static uint32_t lastBeat = 0;
  static uint32_t lastSample = 0;
  uint32_t now = millis();

  server.handleClient();

  if (now - lastSample >= SAMPLE_MS) {
    lastSample = now;
    sampleSensors();
  }

  applyLocalControl(now);
  persistClockIfDue(now);

  if (now - lastBeat >= HEARTBEAT_MS) {
    lastBeat = now;
    heartbeat();
    if (PIN_LED_STATUS >= 0) {
      digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS));
    }
  }
}
