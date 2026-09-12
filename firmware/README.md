# Smart GrowKit firmware

ESP32-WROOM DevKit (DIY Electronics USB-C, **CP2102**).
VS Code + PlatformIO. Local control only. No MQTT.

Boots **SAFE** (all relays and dose motors OFF / LOW), then restores NVS setpoints
(default **AUTO**). Heater stays off (`SGK_HEATER_ENABLED=0`). Dose C stays off.
No auto pH.

SoftAP SSID: **GrowKit-PoC** (open). Page: **http://192.168.4.1**
Firmware: **0.6.1-poc**

## Clock and schedules

- Set the device clock from the page: **Set clock from phone**, or type a local time and **Set this time**.
- POST `/clock?iso=2026-09-12T14:30:00` (local) or `/clock?epoch=1770000000`.
- `clock_ok` and current **HH:MM** are on the page and in the serial heartbeat.
- If the clock has **never** been set, `clock_ok=false`. AUTO **does not** run the light 16/8 schedule (light stays off). Pump 15/45 still runs on `millis()`.
- After a clock is set, AUTO light is 16 h ON / 8 h OFF in **local** time, default **06:00–22:00**.
- Last clock and setpoints (mode, manual pump/light/fan, light window) are saved in **NVS**. A reboot restores the last saved clock (it will be stale after a long power-off — set it again from the phone).

## Manual dose pulses (A / B only)

- Only in **manual** mode. POST `/dose?ch=A&ms=500` or `/dose?ch=B&ms=500` (`ms` default 500, max 2000).
- Blocked if low water (`< 20%`) or level is NAN, if a pulse is already running, or during the **15 min settle** after any pulse.
- Motors: IN1 HIGH / IN2 LOW for the pulse, then **both LOW**. Same LOW state at boot. Dose C never runs.
- Page and serial show `dose` (`off` / `A` / `B`) and `settle_s`.

### Prime a pump into a measuring cup

Use this before a reservoir is connected. Do **not** pulse into a dry reservoir line you care about.

1. Power the kit. Join **GrowKit-PoC** and open **http://192.168.4.1**.
2. Switch to **manual**. Heater stays off.
3. Level must be valid and ≥ 20%. On a dry bench the ultrasonic is NAN and pulses are blocked unless **`SGK_BENCH_SIM=1`** (default simulated `level_pct=80`). Without that flag, hold the sensor over a target ~4–20 cm away so `level_pct` is not NAN and not below 20%.
4. Pull the **Dose A** or **Dose B** outlet tube off the reservoir fitting and put the open end into a dry measuring cup / syringe barrel.
5. Confirm the matching inlet tube is in the dose bottle (pH down = A, pH up = B).
6. Set **pulse ms** (start at 500). Tap **Dose A** or **Dose B** once. The motor runs that long, then both pins go LOW.
7. Read the millilitres in the cup. That is your pulse volume for that `ms` (tubing fill may make the first pulse short — discard or note it as prime, not calibration).
8. Wait until **settle_s** reaches 0 (15 minutes) before the next pulse. Do not hold the button; one tap is one pulse.
9. Repeat until the line is primed and you have a stable ml / 500 ms figure. Then put the outlet back on the reservoir.

## Phone steps

1. Flash this firmware (USB data cable). Serial **115200**.
2. Wait until serial shows `ap ip=192.168.4.1` and `ssid=GrowKit-PoC`.
3. On the phone, open Wi‑Fi and join **GrowKit-PoC**. There is no password.
4. If the phone says “No internet” / “Use mobile data”, **stay on this network**. Turn off mobile data if it keeps dropping the kit Wi‑Fi.
5. Open the browser (Safari / Chrome) and go to **http://192.168.4.1** — type the address; do not search for it.
6. The page refreshes live every 2 s (`/status` JSON). Confirm `clock_ok`, HH:MM, `dose`, and `settle_s`.
7. Tap **Set clock from phone** (or enter a local time). Light 16/8 can then run in AUTO.
8. **safe** — all actuators off, dose motors LOW.
9. **auto** — pump 15/45 from power-on. Light 16/8 only if `clock_ok`. Fan off. No dosing. Low water still locks pump + dosing off.
10. **manual** — tap pump / light / fan to toggle. Pulse Dose A / B as above. Heater and Dose C stay off.
11. If `LOW WATER` is shown (level &lt; 20% or no valid reading), pump and dose pulses are blocked until the reservoir/sensor is OK.

## Bench sim (`SGK_BENCH_SIM`)

Compile flag, default **0** (off). Does not enable auto-dose or heater.

When **`SGK_BENCH_SIM=1`**, if a live sensor is NAN the firmware uses the last `POST /sim` values from **RAM** (not NVS). Defaults: `level_pct=80`, `ph` / `air_c` / `water_c` unset (stay NAN until posted). Reboot restores those RAM defaults.

```ini
; firmware/platformio.ini  [env:esp32dev] build_flags
-D SGK_BENCH_SIM=1
```

```powershell
# after joining GrowKit-PoC
# POST /sim?level_pct=80&ph=6.2&air_c=22&water_c=20
```

The SoftAP page shows a sim form only when the flag is 1. `/sim` returns 404 when the flag is 0. Use this to test manual dose pulses dry (`level_pct=80` clears the NAN water lock).

## Bench commands

```powershell
cd C:\Users\oluhl\smart-growkit\firmware
pio run -e esp32dev
pio run -e esp32dev -t upload
pio device monitor -e esp32dev
```

If `pio` is not on PATH:

```powershell
cd C:\Users\oluhl\smart-growkit\firmware
C:\Users\oluhl\.platformio\penv\Scripts\pio.exe run -e esp32dev
C:\Users\oluhl\.platformio\penv\Scripts\pio.exe run -e esp32dev -t upload
```

If upload fails: hold BOOT, start upload, release when writing starts.

## Pins (`src/pins.h`)

| Function | GPIO |
|---|---|
| Status LED | 2 |
| DHT11 | 32 |
| pH analog | 33 |
| Ultrasonic TRIG/ECHO | 25 / 26 |
| MAX6675 SCK/CS/SO | 5 / 18 / 19 |
| Relay pump / light / heat / fan | 23 / 22 / 21 / 17 |
| Dose A | 27 (IN1) / 14 (IN2) — pulse, then LOW |
| Dose B | 16 (IN1) / 4 (IN2) — pulse, then LOW |
| Dose C | disabled |

UART0 (GPIO1/3) is serial only. GPIO12/15 not used as outputs.
