#pragma once
// ESP32-WROOM DevKit (CP2102 USB-C). UART0 GPIO1/3 reserved for USB serial.
// GPIO12 and GPIO15 unused (strap pins). No EC or CO2 — not on the kit drawing.

#define PIN_LED_STATUS      2

#define PIN_DHT             32
#define PIN_PH_ADC          33
#define PIN_US_TRIG         25
#define PIN_US_ECHO         26
#define PIN_MAX_SCK          5
#define PIN_MAX_CS          18
#define PIN_MAX_SO          19

#define PIN_RLY_PUMP        23
#define PIN_RLY_LIGHT       22
#define PIN_RLY_HEAT        21
#define PIN_RLY_FAN         17
#define RELAY_ON_LEVEL      LOW
#define RELAY_OFF_LEVEL     HIGH

#define PIN_DOSE_A_IN1      27
#define PIN_DOSE_A_IN2      14
#define PIN_DOSE_B_IN1      16
#define PIN_DOSE_B_IN2       4
#define PIN_DOSE_C_IN1      -1
#define PIN_DOSE_C_IN2      -1
