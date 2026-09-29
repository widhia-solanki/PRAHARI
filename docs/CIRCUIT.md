════════════════════════════════════════════════════════════════════
  PRAHARI — COMPLETE WIRING DIAGRAM
════════════════════════════════════════════════════════════════════

GOLDEN RULES:
1. ALL grounds common: battery(-), Mega GND, ESP32 GND, breadboard GND,
   every sensor GND — tied together.
2. Shield covers A0-A7; we use A8-A15 + comm header 18/19/20/21.
3. OLED + ToF + MPU share I2C SDA(20)/SCL(21).

─────────────────────── POWER ───────────────────────
2x 18650 (7.4V)  +  →  HW-130 EXT_PWR (+)
                 -  →  HW-130 EXT_PWR (-)  = system GND
Set HW-130 jumper so battery powers motors + 5V reg.

Breadboard power hub:
  HW-130 "5V"  screw  →  breadboard RED (+) rail
  HW-130 "Gnd" screw  →  breadboard BLUE (-) rail
All sensor VCC → RED rail ; all sensor GND → BLUE rail.

─────────────────────── MOTORS (HW-130 screw terminals) ───────────────────────
Left-front  → M1      Right-front → M3
Left-rear   → M2      Right-rear  → M4

─────────────────────── ULTRASONIC (HC-SR04) ───────────────────────
VCC→5V   GND→GND   TRIG→A8   ECHO→A9

─────────────────────── I2C BUS (3 devices share it) ───────────────────────
Mega SDA(20) → breadboard SDA row
Mega SCL(21) → breadboard SCL row

OLED:      VCC→5V  GND→GND  SDA→SDA row  SCL→SCL row   (0x3C)
ToF:       VIN→5V  GND→GND  SDA→SDA row  SCL→SCL row   (0x29)
MPU-6050:  VCC→5V  GND→GND  SDA→SDA row  SCL→SCL row   (0x68)

─────────────────────── GAS ───────────────────────
MQ-4 (CH4):  VCC→5V  GND→GND  AO→A10   (use AO, ignore DO)
MQ-7 (CO):   VCC→5V  GND→GND  AO→A11

─────────────────────── DHT11 (KY-015) ───────────────────────
S→A14    +→5V    -→GND

─────────────────────── FLAME (KY-026) ───────────────────────
+→5V   G→GND   D0→A15   (LOW = flame; adjust pot)

─────────────────────── SOUND (KY-037/038) ───────────────────────
+→5V   G→GND   A0→A13   (analog peak jumps on clap)

─────────────────────── BUZZER (KY-006/012) ───────────────────────
+(S)→A12    -→GND

─────────────────────── RED LED ───────────────────────
LED+ —[220Ω]— pin 13    LED- → GND

─────────────────────── MEGA ↔ ESP32 (data to cloud) ───────────────────────
Mega TX1(18) —[1k]—+—→ ESP32 GPIO16 (RX)
                   |
                 [2.2k]
                   |
                  GND            (5V → ~3.4V, safe)

ESP32 GPIO17(TX) ————→ Mega RX1(19)   (direct)
Mega GND ————————————— ESP32 GND       (REQUIRED)
ESP32 → own USB power bank.

─────────────────────── ESP32-CAM (independent!) ───────────────────────
Power: MB base USB (power bank).  Data: WiFi ONLY (no wires to rover).
Same 2.4GHz hotspot → streams to dashboard.

═══════════════════ MEGA PIN SUMMARY ═══════════════════
A8  ultrasonic TRIG     A13 sound A0
A9  ultrasonic ECHO     A14 DHT11 signal
A10 MQ-4 (CH4)          A15 flame D0
A11 MQ-7 (CO)           13  red LED
A12 buzzer              18  TX1 → ESP32 RX16 (divider)
20  SDA                 19  RX1 ← ESP32 TX17
21  SCL                 M1-M4 motors
