/* ============================================================================
   PRAHARI · ESP32 GATEWAY  (rover -> cloud MQTT)
   Team Cipher · SIH26039
   Reads JSON from Mega on Serial2 (GPIO16 RX via 1k/2.2k divider from Mega TX18),
   publishes to MQTT broker that the dashboard subscribes to.
   Libraries: PubSubClient (by Nick O'Leary). Board: ESP32 Dev Module.
   Wiring: Mega TX18 -[1k]-+-> ESP32 GPIO16 ; +-[2.2k]-GND ; Mega GND = ESP32 GND
   ============================================================================ */

#include <WiFi.h>
#include <PubSubClient.h>

const char* WIFI_SSID = "";
const char* WIFI_PASS = "";

const char* MQTT_BROKER = "broker.hivemq.com";
const int   MQTT_PORT   = 1883;
const char* MQTT_TOPIC  = "prahari/cipher/telemetry";
const char* MQTT_ID     = "prahari-rover-gateway";

#define RXD2 16
#define TXD2 17

WiFiClient espClient;
PubSubClient mqtt(espClient);

unsigned long lastBreadcrumb = 0;
const unsigned long breadcrumbEvery = 30000;
int lastRssi = 0;

void connectWiFi(){
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi connecting");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000){
    delay(500); Serial.print(".");
  }
  if (WiFi.status()==WL_CONNECTED){
    Serial.print(" connected  IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println(" FAILED - will retry");
  }
}

void connectMQTT(){
  if (mqtt.connected()) return;
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  Serial.print("MQTT connecting...");
  if (mqtt.connect(MQTT_ID)) Serial.println("connected");
  else { Serial.print("failed rc="); Serial.println(mqtt.state()); }
}

float getVal(const String& s, const String& key, float def=0){
  int k = s.indexOf("\"" + key + "\":");
  if (k < 0) return def;
  int start = k + key.length() + 3;
  int end = start;
  while (end < (int)s.length() && (isdigit(s[end]) || s[end]=='.' || s[end]=='-')) end++;
  return s.substring(start, end).toFloat();
}

void setup(){
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  connectWiFi();
  connectMQTT();
  Serial.println("PRAHARI gateway ready.");
}

void loop(){
  connectWiFi();
  connectMQTT();
  mqtt.loop();

  if (Serial2.available()){
    String line = Serial2.readStringUntil((char)10);
    line.trim();
    if (line.startsWith("{") && line.endsWith("}")){
      float ch4  = getVal(line, "ch4");
      float co   = getVal(line, "co");
      float temp = getVal(line, "temp", 28);
      float tilt = getVal(line, "tilt");
      int   stopf= (int)getVal(line, "stop");

      // fake O2: normal air ~20.9%, dips as CH4 rises (methane displaces oxygen)
      float o2 = 20.9 - (ch4 * 1.2) - random(0, 30) / 100.0;
      if (o2 < 17.5) o2 = 17.5;

      lastRssi = (WiFi.status()==WL_CONNECTED) ? WiFi.RSSI() : -100;

      bool drop = false;
      if (lastRssi < -80 && millis() - lastBreadcrumb > breadcrumbEvery){
        drop = true;
        lastBreadcrumb = millis();
      }

      String payload = "{";
      payload += "\"ch4\":"  + String(ch4,2)  + ",";
      payload += "\"co\":"   + String(co,1)   + ",";
      payload += "\"o2\":"   + String(o2,1)   + ",";
      payload += "\"temp\":" + String(temp,1) + ",";
      payload += "\"tilt\":" + String(tilt,1) + ",";
      payload += "\"rssi\":" + String(lastRssi) + ",";
      payload += "\"drop\":" + String(drop ? "true":"false") + ",";
      payload += "\"victim\":false,";
      payload += "\"stop\":" + String(stopf);
      payload += "}";

      if (mqtt.connected()){
        mqtt.publish(MQTT_TOPIC, payload.c_str());
        Serial.println("published: " + payload);
      } else {
        Serial.println("(offline): " + payload);
      }
    }
  }
}
