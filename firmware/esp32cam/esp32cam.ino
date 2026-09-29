/* ============================================================================
   PRAHARI · ESP32-CAM live MJPEG stream
   Team Cipher · SIH26039
   Board: AI Thinker ESP32-CAM  |  Partition: Huge APP (3MB No OTA)
   After boot, Serial prints:  http://<ip>:81/stream  -> paste in dashboard.
   ============================================================================ */

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

const char* WIFI_SSID = "Redmi 12 5G";
const char* WIFI_PASS = "YOUR_HOTSPOT_PASSWORD";   // <-- your real password

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

httpd_handle_t stream_httpd = NULL;

static esp_err_t stream_handler(httpd_req_t *req){
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  char part_buf[64];
  static const char* BOUNDARY = "\r
--frame\r
";
  static const char* CT = "multipart/x-mixed-replace;boundary=frame";

  res = httpd_resp_set_type(req, CT);
  if (res != ESP_OK) return res;

  while (true){
    fb = esp_camera_fb_get();
    if (!fb){ res = ESP_FAIL; break; }
    size_t hlen = snprintf(part_buf, 64,
      "Content-Type: image/jpeg\r
Content-Length: %u\r
\r
", fb->len);
    if (httpd_resp_send_chunk(req, BOUNDARY, strlen(BOUNDARY)) != ESP_OK){ esp_camera_fb_return(fb); break; }
    if (httpd_resp_send_chunk(req, part_buf, hlen) != ESP_OK){ esp_camera_fb_return(fb); break; }
    if (httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len) != ESP_OK){ esp_camera_fb_return(fb); break; }
    esp_camera_fb_return(fb);
  }
  return res;
}

void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81;
  httpd_uri_t stream_uri = { .uri="/stream", .method=HTTP_GET, .handler=stream_handler, .user_ctx=NULL };
  if (httpd_start(&stream_httpd, &config) == ESP_OK){
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

void setup(){
  Serial.begin(115200);
  delay(300);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()){
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 14;
    config.fb_count = 1;
  }

  if (esp_camera_init(&config) != ESP_OK){
    Serial.println("Camera init failed - reseat ribbon cable");
    return;
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED){ delay(400); Serial.print("."); }
  Serial.println();

  startCameraServer();
  Serial.print("Camera stream ready:  http://");
  Serial.print(WiFi.localIP());
  Serial.println(":81/stream");
  Serial.println(">> paste that URL into the dashboard camera box <<");
}

void loop(){
  delay(10000);
}
