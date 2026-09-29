/* ============================================================================
   PRAHARI · ESP32-CAM Live MJPEG Stream
   Team Cipher · SIH26039
   Board: AI Thinker ESP32-CAM

   Serial Monitor Baud rate: 115200

   After boot:
   http://<ESP32-IP>:81/stream
   ============================================================================ */

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include "esp_timer.h"

// ─────────────────────────────────────────────────────────────────────────────
// WIFI
// ─────────────────────────────────────────────────────────────────────────────

const char* WIFI_SSID = "Eshan & Sandhia_2.4G";
const char* WIFI_PASS = "Bpnplwse@8";

// ─────────────────────────────────────────────────────────────────────────────
// AI THINKER ESP32-CAM PIN MAP
// ─────────────────────────────────────────────────────────────────────────────

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0

#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ─────────────────────────────────────────────────────────────────────────────
// HTTP SERVER
// ─────────────────────────────────────────────────────────────────────────────

httpd_handle_t stream_httpd = NULL;

static const char* STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=frame";
static const char* STREAM_BOUNDARY = "\r\n--frame\r\n";
static const char* STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// ─────────────────────────────────────────────────────────────────────────────
// MJPEG STREAM HANDLER
// ─────────────────────────────────────────────────────────────────────────────

static esp_err_t stream_handler(httpd_req_t *req)
{
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  char part_buf[64];

  res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  // Allow cross-origin so your dashboard can embed it
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while (true)
  {
    fb = esp_camera_fb_get();
    if (!fb)
    {
      Serial.println("[ERROR] Camera capture failed");
      res = ESP_FAIL;
      break;
    }

    // 1) boundary
    if (httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY)) != ESP_OK)
    {
      esp_camera_fb_return(fb);
      break;
    }

    // 2) part header
    size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);
    if (httpd_resp_send_chunk(req, part_buf, hlen) != ESP_OK)
    {
      esp_camera_fb_return(fb);
      break;
    }

    // 3) jpeg data
    if (httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len) != ESP_OK)
    {
      esp_camera_fb_return(fb);
      break;
    }

    esp_camera_fb_return(fb);

    // Yield so the watchdog does not trip
    delay(1);
  }

  return res;
}

// ─────────────────────────────────────────────────────────────────────────────
// START CAMERA SERVER
// ─────────────────────────────────────────────────────────────────────────────

void startCameraServer()
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port     = 81;
  config.ctrl_port       = 32768;
  config.stack_size      = 8192;   // give the handler enough stack

  httpd_uri_t stream_uri;
  stream_uri.uri      = "/stream";
  stream_uri.method   = HTTP_GET;
  stream_uri.handler  = stream_handler;
  stream_uri.user_ctx = NULL;

  if (httpd_start(&stream_httpd, &config) == ESP_OK)
  {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    Serial.println("[OK] Camera server started");
  }
  else
  {
    Serial.println("[ERROR] Camera server failed to start");
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// CAMERA INITIALIZATION
// ─────────────────────────────────────────────────────────────────────────────

bool initCamera()
{
  // IMPORTANT: zero-initialize so grab_mode / fb_location are not garbage
  camera_config_t config = {};

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk  = XCLK_GPIO_NUM;
  config.pin_pclk  = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href  = HREF_GPIO_NUM;

  // Newer core naming
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn  = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // These two were the missing pieces that can hang init:
  config.grab_mode    = CAMERA_GRAB_LATEST;

  // ─────────────────────────────────────────────────────────────────────────
  // PSRAM CHECK
  // ─────────────────────────────────────────────────────────────────────────

  if (psramFound())
  {
    Serial.println("[CAM] PSRAM detected");

    // START SMALL to prove it boots. Bump to FRAMESIZE_VGA once you see the URL.
    config.frame_size   = FRAMESIZE_QVGA;   // 320x240
    config.jpeg_quality = 12;
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;

    Serial.println("[CAM] QVGA / quality 12 / 2 frame buffers (PSRAM)");
  }
  else
  {
    Serial.println("[CAM] WARNING: PSRAM not detected");

    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 14;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;

    Serial.println("[CAM] QVGA / quality 14 / 1 frame buffer (DRAM)");
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("[ERROR] Camera init failed: 0x%x\n", err);
    return false;
  }

  // ─────────────────────────────────────────────────────────────────────────
  // CAMERA SENSOR SETTINGS
  // ─────────────────────────────────────────────────────────────────────────

  sensor_t* s = esp_camera_sensor_get();
  if (s != NULL)
  {
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_whitebal(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_gain_ctrl(s, 1);

    // If your image is upside down, flip it:
    // s->set_vflip(s, 1);
    // s->set_hmirror(s, 1);
  }

  Serial.println("[OK] Camera initialized!");
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// WIFI CONNECTION
// ─────────────────────────────────────────────────────────────────────────────

bool connectToWiFi()
{
  Serial.println();
  Serial.println("==========================================");
  Serial.print("[WIFI] Connecting to: ");
  Serial.println(WIFI_SSID);
  Serial.println("==========================================");

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);              // helps stream stability
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    attempts++;

    if (attempts >= 40)
    {
      Serial.println();
      Serial.println("[ERROR] WiFi connection timeout!");
      return false;
    }
  }

  Serial.println();
  Serial.println("[OK] WiFi connected!");
  Serial.print("[WIFI] ESP32 IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("[WIFI] RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────────────────────────────────────

void setup()
{
  Serial.begin(115200);   // Serial Monitor MUST be 115200
  delay(2000);

  Serial.println();
  Serial.println("==========================================");
  Serial.println("       PRAHARI ESP32-CAM");
  Serial.println("       Live MJPEG Stream");
  Serial.println("       Team Cipher · SIH26039");
  Serial.println("==========================================");

  Serial.println();
  Serial.println("[1] Initializing camera...");
  if (!initCamera())
  {
    Serial.println("[FATAL] Camera initialization failed.");
    Serial.println("[FATAL] Check:");
    Serial.println("       - Camera ribbon cable seated fully");
    Serial.println("       - AI Thinker board selected");
    Serial.println("       - Use a proper 5V power supply");
    return;
  }

  Serial.println();
  Serial.println("[2] Connecting WiFi...");
  if (!connectToWiFi())
  {
    Serial.println("[FATAL] WiFi failed. Check SSID/password.");
    return;
  }

  Serial.println();
  Serial.println("[3] Starting camera server...");
  startCameraServer();

  Serial.println();
  Serial.println("==========================================");
  Serial.println("          PRAHARI CAMERA READY");
  Serial.println("==========================================");
  Serial.print("Stream URL: http://");
  Serial.print(WiFi.localIP());
  Serial.println(":81/stream");
  Serial.println();
  Serial.println("Paste this URL into your dashboard.");
  Serial.println("==========================================");
}

// ─────────────────────────────────────────────────────────────────────────────
// LOOP
// ─────────────────────────────────────────────────────────────────────────────

void loop()
{
  delay(10000);
}
