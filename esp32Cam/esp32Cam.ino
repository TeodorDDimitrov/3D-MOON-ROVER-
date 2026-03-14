#include <WiFi.h>
#include <BluetoothSerial.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "nvs_flash.h"

BluetoothSerial SerialBT;

String receivedSSID     = "";
String receivedPassword = "";
bool   btActive         = false;
bool   cameraStarted    = false;

httpd_handle_t stream_httpd = NULL;
httpd_handle_t web_httpd    = NULL;

#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

#define PART_BOUNDARY "frame"

void handleWifiScan() {
  WiFi.disconnect(false);
  delay(300);
  SerialBT.println("SCAN_START");

  int n = WiFi.scanNetworks(false, false, false, 300);

  if (n <= 0) {
    SerialBT.println(n == 0 ? "SSID:No networks found" : "SSID:Scan failed");
  } else {
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.length() > 0) SerialBT.println("SSID:" + ssid);
      delay(10);
    }
  }

  SerialBT.println("SCAN_END");
  WiFi.scanDelete();
  WiFi.mode(WIFI_STA);
}

bool connectToWiFi() {
  Serial.println("Connecting to: " + receivedSSID);
  SerialBT.println("Connecting...");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false);
  delay(200);
  WiFi.begin(receivedSSID.c_str(), receivedPassword.c_str());

  for (int i = 0; i < 60; i++) {
    if (WiFi.status() == WL_CONNECTED) break;
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    String ip = WiFi.localIP().toString();
    Serial.println("IP: " + ip);
    SerialBT.println("WiFi Connected!");
    SerialBT.println("IP:" + ip);
    SerialBT.println("VIEW:http://" + ip + "/");
    SerialBT.println("STREAM:http://" + ip + ":81/stream");
    return true;
  }

  Serial.println("WiFi FAILED");
  switch (WiFi.status()) {
    case WL_NO_SSID_AVAIL:  SerialBT.println("Network not found"); break;
    case WL_CONNECT_FAILED: SerialBT.println("Wrong password");    break;
    default: SerialBT.println("Error: " + String(WiFi.status()));  break;
  }
  return false;
}

void handleCommand(const String &command) {
  if (command == "SCAN") {
    handleWifiScan();

  } else if (command.startsWith("WIFI:")) {
    receivedSSID = command.substring(5);
    receivedSSID.trim();
    SerialBT.println("SSID set: " + receivedSSID);

  } else if (command.startsWith("PASS:")) {
    receivedPassword = command.substring(5);
    receivedPassword.trim();
    SerialBT.println("Password received");

    if (receivedSSID.length() == 0) {
      SerialBT.println("ERROR: Send WIFI: first");
      return;
    }

    if (connectToWiFi()) {
      delay(200);
      SerialBT.end();
      btActive = false;
      startPhase2();
    }

  } else if (command == "GET_IP") {
    if (WiFi.status() == WL_CONNECTED) {
      String ip = WiFi.localIP().toString();
      SerialBT.println("IP:" + ip);
      SerialBT.println("STREAM:http://" + ip + ":81/stream");
    } else {
      SerialBT.println("ERROR");
    }

  } else if (command == "GET_CAM_INFO") {
    if (WiFi.status() == WL_CONNECTED) {
      SerialBT.println("STREAM:http://" + WiFi.localIP().toString() + ":81/stream,RES:320x240,FPS:15");
    } else {
      SerialBT.println("ERROR");
    }

  } else {
    SerialBT.println("Unknown: " + command);
  }
}

// ─────────────────────────────────────────────────────────────────────────────

bool initCamera() {
  camera_config_t cfg;
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer   = LEDC_TIMER_0;
  cfg.pin_d0       = Y2_GPIO_NUM;
  cfg.pin_d1       = Y3_GPIO_NUM;
  cfg.pin_d2       = Y4_GPIO_NUM;
  cfg.pin_d3       = Y5_GPIO_NUM;
  cfg.pin_d4       = Y6_GPIO_NUM;
  cfg.pin_d5       = Y7_GPIO_NUM;
  cfg.pin_d6       = Y8_GPIO_NUM;
  cfg.pin_d7       = Y9_GPIO_NUM;
  cfg.pin_xclk     = XCLK_GPIO_NUM;
  cfg.pin_pclk     = PCLK_GPIO_NUM;
  cfg.pin_vsync    = VSYNC_GPIO_NUM;
  cfg.pin_href     = HREF_GPIO_NUM;
  cfg.pin_sscb_sda = SIOD_GPIO_NUM;
  cfg.pin_sscb_scl = SIOC_GPIO_NUM;
  cfg.pin_pwdn     = PWDN_GPIO_NUM;
  cfg.pin_reset    = RESET_GPIO_NUM;
  cfg.xclk_freq_hz = 20000000;
  cfg.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    cfg.frame_size   = FRAMESIZE_VGA;
    cfg.jpeg_quality = 10;
    cfg.fb_count     = 2;
  } else {
    cfg.frame_size   = FRAMESIZE_QQVGA;
    cfg.jpeg_quality = 12;
    cfg.fb_count     = 1;
  }

  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  // Rotate image 180° by enabling both vertical flip and horizontal mirror
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_vflip(s, 1);   // flip vertically
    s->set_hmirror(s, 1); // flip horizontally
  }

  return true;
}

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb  = NULL;
  esp_err_t    res = ESP_OK;
  size_t       jpg_len = 0;
  uint8_t     *jpg_buf = NULL;
  char         part[128];

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=" PART_BOUNDARY);
  if (res != ESP_OK) return res;

  httpd_resp_set_hdr(req, "Connection", "keep-alive");

  while (true) {
    fb = NULL; jpg_buf = NULL; jpg_len = 0;

    fb = esp_camera_fb_get();
    if (!fb) {
      res = ESP_FAIL;
    } else {
      if (fb->format != PIXFORMAT_JPEG) {
        bool ok = frame2jpg(fb, 80, &jpg_buf, &jpg_len);
        esp_camera_fb_return(fb);
        fb = NULL;
        if (!ok) res = ESP_FAIL;
      } else {
        jpg_len = fb->len;
        jpg_buf = fb->buf;
      }
    }

    if (res == ESP_OK) {
      size_t hlen = snprintf(part, sizeof(part),
        "--" PART_BOUNDARY "\r\n"
        "Content-Type: image/jpeg\r\n"
        "Content-Length: %u\r\n\r\n",
        (unsigned)jpg_len);
      res = httpd_resp_send_chunk(req, part, (ssize_t)hlen);
    }
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)jpg_buf, (ssize_t)jpg_len);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, "\r\n", 2);

    if (fb)           { esp_camera_fb_return(fb); fb = NULL; }
    else if (jpg_buf) { free(jpg_buf); jpg_buf = NULL; }

    if (res != ESP_OK) break;
  }
  return res;
}

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32-CAM Live Stream</title>
  <style>
    body { margin:0; background:#111; display:flex; flex-direction:column;
           align-items:center; justify-content:center; height:100vh;
           font-family:sans-serif; color:#eee; }
    h2   { margin-bottom:12px; }
    img  { max-width:100%; border:2px solid #444; border-radius:6px; }
    p    { margin-top:10px; font-size:0.85rem; color:#888; }
  </style>
</head>
<body>
  <h2>&#128247; ESP32-CAM Live</h2>
  <img src=":81/stream" onerror="this.alt='Stream unavailable — check port 81'">
  <p>Raw stream: <a href=":81/stream" style="color:#6bf">:81/stream</a></p>
  <script>
    document.querySelectorAll('[src],[href]').forEach(el => {
      ['src','href'].forEach(attr => {
        const v = el.getAttribute(attr);
        if (v && v.startsWith(':81/'))
          el.setAttribute(attr, 'http://' + location.hostname + v);
      });
    });
  </script>
</body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

void startCameraServer() {
  {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port    = 81;
    cfg.stack_size     = 8192;
    cfg.ctrl_port      = 32768;

    httpd_uri_t stream_uri = { "/stream", HTTP_GET, stream_handler, NULL };

    if (httpd_start(&stream_httpd, &cfg) == ESP_OK)
      httpd_register_uri_handler(stream_httpd, &stream_uri);
    else
      Serial.println("Stream server failed");
  }
  {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port    = 80;
    cfg.stack_size     = 4096;
    cfg.ctrl_port      = 32769;

    httpd_uri_t index_uri = { "/", HTTP_GET, index_handler, NULL };

    if (httpd_start(&web_httpd, &cfg) == ESP_OK)
      httpd_register_uri_handler(web_httpd, &index_uri);
    else
      Serial.println("Web server failed");
  }

  Serial.println("Stream: http://" + WiFi.localIP().toString() + ":81/stream");
}

void startPhase2() {
  if (!initCamera()) {
    Serial.println("Camera FAILED — halting");
    while (1) delay(1000);
  }

  startCameraServer();
  cameraStarted = true;

  pinMode(4, OUTPUT);
  for (int i = 0; i < 3; i++) {
    digitalWrite(4, HIGH); delay(200);
    digitalWrite(4, LOW);  delay(200);
  }

  Serial.println("Ready");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Booting...");

  esp_err_t nvsRet = nvs_flash_init();
  if (nvsRet == ESP_ERR_NVS_NO_FREE_PAGES || nvsRet == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false);
  delay(300);

  if (!SerialBT.begin("ESP32_CAM_Setup")) {
    Serial.println("BT init FAILED — halting");
    while (1) delay(1000);
  }
  btActive = true;

  Serial.println("BT ready — waiting for commands");
}

void loop() {
  if (btActive && SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) handleCommand(cmd);
  }
  delay(20);
}
