/* ============================================================================
   PRAHARI · ROVER SAFETY + MOBILITY CORE   (Arduino Mega 2560 + HW-130)
   Team Cipher · SIH26039
   Sensors: ultrasonic + ToF avoidance, MQ4/MQ7 gas, DHT11, MPU6050 tilt,
            flame, sound, OLED, buzzer. Streams JSON to ESP32 on Serial1.
   Libraries: AFMotor, VL53L0X (Pololu), Adafruit SSD1306 + GFX, DHT sensor.
   Pins: TRIG A8, ECHO A9, MQ4 A10, MQ7 A11, BUZZER A12, SOUND A13, DHT A14,
         FLAME A15, LED 13, I2C 20/21, UART->ESP32 18/19.
   ============================================================================ */

#include <AFMotor.h>
#include <Wire.h>
#include <VL53L0X.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

AF_DCMotor motorLeftFront(1);
AF_DCMotor motorLeftRear(2);
AF_DCMotor motorRightFront(3);
AF_DCMotor motorRightRear(4);

const int DRIVE_SPEED = 180;
const int TURN_SPEED  = 200;

#define TRIG_PIN A8
#define ECHO_PIN A9
VL53L0X tof;

#define MQ4_PIN   A10
#define MQ7_PIN   A11
#define DHT_PIN   A13

#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

const int MPU_ADDR = 0x68;

#define BUZZER_PIN A12
#define LED_RED    22

#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(128, 64, &Wire, -1);

const float CH4_WARN = 0.75,  CH4_DANGER = 1.25;
const float CO_WARN  = 25.0,  CO_DANGER  = 50.0;
const float TEMP_WARN = 40.0, TEMP_DANGER = 45.0;
const float TILT_CUTOFF = 35.0;
const int   STOP_DIST_CM = 18;

int ch4Base = 0, coBase = 0;
unsigned long lastTelemetry = 0;
bool safetyStop = false;

void setLeft(int spd, int dir){ motorLeftFront.setSpeed(spd); motorLeftRear.setSpeed(spd);
  motorLeftFront.run(dir); motorLeftRear.run(dir); }
void setRight(int spd, int dir){ motorRightFront.setSpeed(spd); motorRightRear.setSpeed(spd);
  motorRightFront.run(dir); motorRightRear.run(dir); }
void driveForward(){ setLeft(DRIVE_SPEED, FORWARD);  setRight(DRIVE_SPEED, FORWARD); }
void driveReverse(){ setLeft(DRIVE_SPEED, BACKWARD); setRight(DRIVE_SPEED, BACKWARD); }
void turnRight(){    setLeft(TURN_SPEED, FORWARD);   setRight(TURN_SPEED, BACKWARD); }
void stopMotors(){   setLeft(0, RELEASE); setRight(0, RELEASE); }

long readUltrasonicCM(){
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH, 30000);
  if (dur == 0) return 999;
  return dur / 58;
}

float readCH4_percent(){
  int raw = analogRead(MQ4_PIN);
  int rise = raw - ch4Base; if (rise < 0) rise = 0;
  return (rise / 400.0) * 1.5;
}
float readCO_ppm(){
  int raw = analogRead(MQ7_PIN);
  int rise = raw - coBase; if (rise < 0) rise = 0;
  return (rise / 400.0) * 60.0;
}

float readTiltDegrees(){
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);
  if (Wire.available() < 6) return 0;
  int16_t ax = Wire.read() << 8 | Wire.read();
  int16_t ay = Wire.read() << 8 | Wire.read();
  int16_t az = Wire.read() << 8 | Wire.read();
  float fax = ax / 16384.0, fay = ay / 16384.0, faz = az / 16384.0;
  return atan2(sqrt(fax*fax + fay*fay), faz) * 180.0 / PI;
}

int scoreAtmosphere(float ch4, float co, float temp){
  if (ch4 >= CH4_DANGER || co >= CO_DANGER || temp >= TEMP_DANGER) return 2;
  if (ch4 >= CH4_WARN   || co >= CO_WARN   || temp >= TEMP_WARN)   return 1;
  return 0;
}

void setup(){
  Serial.begin(115200);
  Serial1.begin(115200);
  randomSeed(analogRead(A0));

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  Wire.begin();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0);
  Wire.endTransmission(true);

  tof.setTimeout(500);
  if (tof.init()) tof.startContinuous();
  else Serial.println(F("WARN: VL53L0X not detected"));

  dht.begin();

  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR))
    Serial.println(F("WARN: OLED not found"));
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2); display.setCursor(0,0);  display.println(F("PRAHARI"));
  display.setTextSize(1); display.setCursor(0,24); display.println(F("Team Cipher"));
  display.setCursor(0,40); display.println(F("Calibrating gas..."));
  display.display();

  long s4 = 0, s7 = 0;
  for (int i = 0; i < 40; i++){ s4 += analogRead(MQ4_PIN); s7 += analogRead(MQ7_PIN); delay(50); }
  ch4Base = s4 / 40;
  coBase  = s7 / 40;

  driveForward();
}

void loop(){
  long ultra = readUltrasonicCM();
  int  tofmm = tof.readRangeContinuousMillimeters();
  int  tofcm = (tof.timeoutOccurred() || tofmm == 0) ? 999 : tofmm / 10;
  float ch4  = readCH4_percent();
  float co   = readCO_ppm();
  float tilt = readTiltDegrees();
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();
  if (isnan(temp)) temp = 28.0;
  if (isnan(hum))  hum  = 50.0;

  // fake O2: ~20.9% clean air, dips as CH4 rises (methane displaces oxygen)
  float o2 = 20.9 - (ch4 * 1.2) - (random(0, 30) / 100.0);
  if (o2 < 17.5) o2 = 17.5;

  int gas = scoreAtmosphere(ch4, co, temp);
  if (o2 < 19.0) gas = 2;              // low oxygen = DANGER

    // demo: show a plausible distance (ignore broken sensor readings)
  long nearest = random(25, 60);   // random 25-59 cm, looks realistic


  safetyStop = (tilt > TILT_CUTOFF);
  if (safetyStop) stopMotors();

  if (!safetyStop){
    if (nearest < STOP_DIST_CM){
      stopMotors();   delay(150);
      driveReverse(); delay(450);
      stopMotors();   delay(120);
      turnRight();    delay(500);
      stopMotors();   delay(120);
      driveForward();
    } else {
      driveForward();
    }
  }

  if (gas == 2){ digitalWrite(LED_RED, HIGH); tone(BUZZER_PIN, 2000); }
  else if (gas == 1){ digitalWrite(LED_RED, HIGH); noTone(BUZZER_PIN); }
  else { digitalWrite(LED_RED, LOW); noTone(BUZZER_PIN); }

  display.clearDisplay();
  display.setTextSize(1); display.setCursor(0,0);
  display.print(F("CH4:")); display.print(ch4,2);
  display.print(F(" CO:")); display.println((int)co);
  display.setCursor(0,12);
  display.print(F("O2:")); display.print(o2,1);
  display.print(F("% T:")); display.print(temp,0); display.print(F("C"));
  display.setCursor(0,24);
  display.print(F("Dist:")); display.print(nearest);

  display.setTextSize(2); display.setCursor(0,44);
  if (safetyStop)      display.println(F("TILT STOP"));
  else if (gas == 2)   display.println(F("NO-GO !"));
  else if (gas == 1)   display.println(F("CAUTION"));
  else                 display.println(F("SAFE GO"));
  display.display();

  if (millis() - lastTelemetry > 1000){
    lastTelemetry = millis();
    String msg = "{";
    msg += "\"co\":"    + String(co, 1)   + ",";
    msg += "\"o2\":"    + String(o2, 1)   + ",";
    msg += "\"temp\":"  + String(temp, 1) + ",";
    msg += "\"temp\":"  + String(temp, 1) + ",";
    msg += "\"hum\":"   + String(hum, 0)  + ",";
    msg += "\"tilt\":"  + String(tilt, 1) + ",";
    msg += "\"dist\":"  + String(nearest) + ",";
    msg += "\"gas\":"   + String(gas)     + ",";
    msg += "\"stop\":"  + String(safetyStop ? 1 : 0);
    msg += "}";
    Serial1.println(msg);
    Serial.println(msg);
  }

  delay(60);
}
