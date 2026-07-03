#include <Arduino.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <TAMC_GT911.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <lvgl.h>
#include <time.h>

LV_FONT_DECLARE(lifetodo_pingfang_24);
LV_FONT_DECLARE(lifetodo_pingfang_28);

#if __has_include("app_config.h")
#include "app_config.h"
#else
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define LIFETODO_HOME_ID "demo-home"
#define LIFETODO_DEVICE_ID "entry"
#define LIFETODO_BASE_URL "http://120.55.46.251"
#endif

#if ARDUINO_USB_CDC_ON_BOOT
#define LOG_SERIAL Serial0
#else
#define LOG_SERIAL Serial
#endif

namespace {
constexpr uint16_t SCREEN_W = 800;
constexpr uint16_t SCREEN_H = 480;
constexpr int32_t RGB_PCLK_HZ = 16000000;
constexpr int I2C_SDA = 8;
constexpr int I2C_SCL = 9;
constexpr uint8_t CH422G_WR_SET_ADDR = 0x24;
constexpr uint8_t CH422G_WR_IO_ADDR = 0x38;
constexpr uint8_t CH422G_IO_OE = 0x01;
constexpr uint8_t EXIO_TP_RST = 1;
constexpr uint8_t EXIO_LCD_BL = 2;
constexpr uint8_t EXIO_LCD_RST = 3;
constexpr uint32_t TASK_CLICK_REARM_RELEASE_MS = 220;
constexpr uint32_t CLOUD_SYNC_INTERVAL_MS = 300000;
constexpr uint32_t CLOUD_SYNC_RETRY_BASE_MS = 15000;
constexpr uint32_t CLOUD_SYNC_RETRY_MAX_MS = 120000;
constexpr uint32_t SYNC_FAILURE_REPORT_RETRY_MS = 60000;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 12000;
constexpr uint32_t CLOUD_SYNC_RETRY_DELAY_MS = 1200;
constexpr uint32_t TIME_SYNC_TIMEOUT_MS = 9000;
constexpr uint16_t DNS_PORT = 53;
constexpr uint8_t CLOUD_SYNC_ATTEMPTS = 3;
constexpr uint8_t SYNC_FAILURE_NOTIFY_THRESHOLD = 3;
constexpr size_t MAX_TASKS = 12;
constexpr size_t TASKS_PER_PAGE = 3;
constexpr size_t MAX_COMPLETIONS = 40;
constexpr size_t MAX_PENDING_COMPLETIONS = 8;
uint8_t ch422g_output = 0xff;

const char GOOGLE_GTS_ROOT_R1[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFYjCCBEqgAwIBAgIQd70NbNs2+RrqIQ/E8FjTDTANBgkqhkiG9w0BAQsFADBX
MQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTEQMA4GA1UE
CxMHUm9vdCBDQTEbMBkGA1UEAxMSR2xvYmFsU2lnbiBSb290IENBMB4XDTIwMDYx
OTAwMDA0MloXDTI4MDEyODAwMDA0MlowRzELMAkGA1UEBhMCVVMxIjAgBgNVBAoT
GUdvb2dsZSBUcnVzdCBTZXJ2aWNlcyBMTEMxFDASBgNVBAMTC0dUUyBSb290IFIx
MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAthECix7joXebO9y/lD63
ladAPKH9gvl9MgaCcfb2jH/76Nu8ai6Xl6OMS/kr9rH5zoQdsfnFl97vufKj6bwS
iV6nqlKr+CMny6SxnGPb15l+8Ape62im9MZaRw1NEDPjTrETo8gYbEvs/AmQ351k
KSUjB6G00j0uYODP0gmHu81I8E3CwnqIiru6z1kZ1q+PsAewnjHxgsHA3y6mbWwZ
DrXYfiYaRQM9sHmklCitD38m5agI/pboPGiUU+6DOogrFZYJsuB6jC511pzrp1Zk
j5ZPaK49l8KEj8C8QMALXL32h7M1bKwYUH+E4EzNktMg6TO8UpmvMrUpsyUqtEj5
cuHKZPfmghCN6J3Cioj6OGaK/GP5Afl4/Xtcd/p2h/rs37EOeZVXtL0m79YB0esW
CruOC7XFxYpVq9Os6pFLKcwZpDIlTirxZUTQAs6qzkm06p98g7BAe+dDq6dso499
iYH6TKX/1Y7DzkvgtdizjkXPdsDtQCv9Uw+wp9U7DbGKogPeMa3Md+pvez7W35Ei
Eua++tgy/BBjFFFy3l3WFpO9KWgz7zpm7AeKJt8T11dleCfeXkkUAKIAf5qoIbap
sZWwpbkNFhHax2xIPEDgfg1azVY80ZcFuctL7TlLnMQ/0lUTbiSw1nH69MG6zO0b
9f6BQdgAmD06yK56mDcYBZUCAwEAAaOCATgwggE0MA4GA1UdDwEB/wQEAwIBhjAP
BgNVHRMBAf8EBTADAQH/MB0GA1UdDgQWBBTkrysmcRorSCeFL1JmLO/wiRNxPjAf
BgNVHSMEGDAWgBRge2YaRQ2XyolQL30EzTSo//z9SzBgBggrBgEFBQcBAQRUMFIw
JQYIKwYBBQUHMAGGGWh0dHA6Ly9vY3NwLnBraS5nb29nL2dzcjEwKQYIKwYBBQUH
MAKGHWh0dHA6Ly9wa2kuZ29vZy9nc3IxL2dzcjEuY3J0MDIGA1UdHwQrMCkwJ6Al
oCOGIWh0dHA6Ly9jcmwucGtpLmdvb2cvZ3NyMS9nc3IxLmNybDA7BgNVHSAENDAy
MAgGBmeBDAECATAIBgZngQwBAgIwDQYLKwYBBAHWeQIFAwIwDQYLKwYBBAHWeQIF
AwMwDQYJKoZIhvcNAQELBQADggEBADSkHrEoo9C0dhemMXoh6dFSPsjbdBZBiLg9
NR3t5P+T4Vxfq7vqfM/b5A3Ri1fyJm9bvhdGaJQ3b2t6yMAYN/olUazsaL+yyEn9
WprKASOshIArAoyZl+tJaox118fessmXn1hIVw41oeQa1v1vg4Fv74zPl6/AhSrw
9U5pCZEt4Wi4wStz6dTZ/CLANx8LZh1J7QJVj2fhMtfTJr9w4z30Z209fOU0iOMy
+qduBmpvvYuR7hZL6Dupszfnw0Skfths18dG9ZKb59UhvmaSGZRVbNQpsg3BZlvi
d0lIKO2d1xozclOzgjXPYovJJIultzkMu34qQb9Sz/yilrbCgj8=
-----END CERTIFICATE-----
)EOF";

// Waveshare ESP32-S3-Touch-LCD-4.3 RGB and touch pins.
// Source: Waveshare wiki pinout for ESP32-S3-Touch-LCD-4.3.
Arduino_ESP32RGBPanel *rgb_bus = new Arduino_ESP32RGBPanel(
    5, 3, 46, 7,
    1, 2, 42, 41, 40,
    39, 0, 45, 48, 47, 21, 14,
    38, 18, 17, 10,
    0, 20, 10, 10,
    0, 10, 10, 10,
    0, RGB_PCLK_HZ, false, 0, 0);

Arduino_RGB_Display *display = new Arduino_RGB_Display(
    SCREEN_W, SCREEN_H, rgb_bus, 0, true);

TAMC_GT911 touch(I2C_SDA, I2C_SCL, 4, -1, SCREEN_W, SCREEN_H);

lv_disp_draw_buf_t draw_buf;
lv_color_t *buf1;
lv_color_t *buf2;
lv_disp_drv_t disp_drv;
lv_indev_drv_t indev_drv;

lv_obj_t *root;
lv_obj_t *status_label;
lv_obj_t *summary_label;
lv_obj_t *task_grid;
lv_obj_t *brightness_overlay;
lv_obj_t *brightness_panel;
lv_obj_t *brightness_slider;
lv_obj_t *brightness_value_label;
lv_obj_t *brightness_gesture_zone;
lv_obj_t *control_wifi_status_label;
lv_obj_t *control_sync_status_label;
lv_obj_t *wifi_setup_panel;
lv_obj_t *wifi_setup_title;
lv_obj_t *wifi_setup_body;
lv_obj_t *wifi_setup_hint;
lv_obj_t *wifi_setup_steps;
lv_obj_t *wifi_setup_close_btn;
lv_obj_t *wifi_disconnect_btn;
lv_obj_t *wifi_reconnect_btn;
lv_obj_t *wifi_reprovision_btn;
lv_obj_t *empty_state_panel;
lv_obj_t *empty_state_title;
lv_obj_t *empty_state_body;
lv_obj_t *task_page_label;
uint32_t last_heartbeat_ms = 0;
uint32_t last_touch_log_ms = 0;
uint32_t last_cloud_sync_ms = 0;
uint32_t next_cloud_sync_ms = 0;
uint32_t next_sync_failure_report_ms = 0;
uint32_t wifi_connect_started_ms = 0;
int last_wifi_status_code = -1;
bool brightness_panel_open = false;
bool provisioning_active = false;
bool wifi_connect_pending = false;
uint8_t brightness_percent = 100;
bool cloud_ready = false;
uint8_t cloud_sync_failure_streak = 0;
bool sync_failure_report_pending = false;
char last_sync_error[64] = "";
char today_key[11] = "2026-07-01";
String pending_wifi_ssid;
String pending_wifi_password;
String provisioning_ap_ssid;
String scanned_wifi_options;
uint32_t last_wifi_scan_ms = 0;

Preferences wifi_preferences;
DNSServer dns_server;
WebServer wifi_server(80);

struct Task {
  char id[40];
  char member[32];
  char title[64];
  char label[32];
  bool done;
  uint32_t color;
};

struct TaskView {
  lv_obj_t *card = nullptr;
  lv_obj_t *accent = nullptr;
  lv_obj_t *check = nullptr;
  lv_obj_t *member = nullptr;
  lv_obj_t *title = nullptr;
  lv_obj_t *label = nullptr;
  lv_obj_t *status = nullptr;
};

struct PendingCompletion {
  char task_id[40];
  bool completed;
  uint8_t attempts;
  uint32_t next_try_ms;
};

Task tasks[MAX_TASKS] = {};
constexpr size_t TASK_COUNT = MAX_TASKS;
TaskView task_views[TASK_COUNT];
size_t task_count = 0;
size_t task_page = 0;
char completion_keys[MAX_COMPLETIONS][64];
size_t completion_count = 0;
PendingCompletion pending_completions[MAX_PENDING_COMPLETIONS];
size_t pending_completion_count = 0;
bool task_click_armed = true;
bool touch_down = false;
int16_t touch_start_x = 0;
int16_t touch_start_y = 0;
int16_t touch_last_x = 0;
int16_t touch_last_y = 0;
uint32_t touch_release_started_ms = 0;

void task_click(lv_event_t *event);
void set_brightness_panel_open(bool open);
void update_brightness_visuals();
void disable_scroll(lv_obj_t *obj);
void make_static_touch_obj(lv_obj_t *obj);
void apply_task_view(size_t index);
void update_summary();
void render_tasks();
void update_task_page();
void task_grid_gesture(lv_event_t *event);
bool sync_from_cloud();
bool push_completion_to_cloud(const char *task_id, bool completed);
void queue_completion_sync(const char *task_id, bool completed);
void process_pending_completion_sync();
void process_sync_failure_report();
void update_control_center_status();
void set_wifi_setup_visible(bool visible, const char *title, const char *body, const char *hint);
void start_wifi_provisioning();
void stop_wifi_provisioning();
bool connect_saved_wifi();
void handle_wifi_services();
String saved_wifi_ssid();
void wifi_setup_close_event(lv_event_t *event);
void wifi_disconnect_event(lv_event_t *event);
void wifi_reconnect_event(lv_event_t *event);
void wifi_reprovision_event(lv_event_t *event);
void show_wifi_status_panel();
void refresh_wifi_state_labels();

void set_cjk_font(lv_obj_t *obj) {
  lv_obj_set_style_text_font(obj, &lifetodo_pingfang_24, 0);
}

void set_cjk_title_font(lv_obj_t *obj) {
  lv_obj_set_style_text_font(obj, &lifetodo_pingfang_28, 0);
}

void disable_scroll(lv_obj_t *obj) {
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN |
                             LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                             LV_OBJ_FLAG_SCROLL_ON_FOCUS);
}

void make_static_touch_obj(lv_obj_t *obj) {
  disable_scroll(obj);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_obj_set_style_transform_width(obj, 0, LV_STATE_PRESSED);
  lv_obj_set_style_transform_height(obj, 0, LV_STATE_PRESSED);
  lv_obj_set_style_transform_zoom(obj, 256, LV_STATE_PRESSED);
  lv_obj_set_style_translate_x(obj, 0, LV_STATE_PRESSED);
  lv_obj_set_style_translate_y(obj, 0, LV_STATE_PRESSED);
  lv_obj_set_style_shadow_width(obj, 0, LV_STATE_PRESSED);
}

bool ch422g_write(uint8_t addr, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(value);
  uint8_t err = Wire.endTransmission();
  if (err != 0) {
    LOG_SERIAL.printf("CH422G write failed addr=0x%02x err=%u\n", addr, err);
    return false;
  }
  return true;
}

void ch422g_set_output(uint8_t pin, bool level) {
  if (level) {
    ch422g_output |= (1 << pin);
  } else {
    ch422g_output &= ~(1 << pin);
  }
  ch422g_write(CH422G_WR_IO_ADDR, ch422g_output);
}

void init_expander_outputs() {
  LOG_SERIAL.println("CH422G init begin");
  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  ch422g_write(CH422G_WR_SET_ADDR, CH422G_IO_OE);
  ch422g_output = 0xff;
  ch422g_write(CH422G_WR_IO_ADDR, ch422g_output);

  ch422g_set_output(EXIO_TP_RST, true);
  ch422g_set_output(EXIO_LCD_BL, true);
  ch422g_set_output(EXIO_LCD_RST, false);
  delay(10);
  ch422g_set_output(EXIO_LCD_RST, true);
  delay(120);
  LOG_SERIAL.println("CH422G init done, LCD reset high, backlight high");
}

void copy_text(char *dest, size_t size, const char *value) {
  if (size == 0) return;
  strlcpy(dest, value ? value : "", size);
}

uint32_t parse_color(const char *value, uint32_t fallback) {
  if (!value || value[0] == '\0') return fallback;
  const char *hex = value[0] == '#' ? value + 1 : value;
  return strtoul(hex, nullptr, 16);
}

String api_url(const char *path) {
  String base = LIFETODO_BASE_URL;
  if (base.endsWith("/")) {
    base.remove(base.length() - 1);
  }
  return base + path;
}

String state_api_url() {
  return api_url("/api/state?home=") + LIFETODO_HOME_ID;
}

String completions_api_url() {
  return api_url("/api/completions?home=") + LIFETODO_HOME_ID;
}

String sync_failure_api_url() {
  return api_url("/api/devices/sync-failure?home=") + LIFETODO_HOME_ID;
}

bool begin_lifetodo_http(HTTPClient &http, WiFiClient &plain_client, WiFiClientSecure &secure_client, const String &url) {
  if (url.startsWith("https://")) {
    secure_client.setInsecure();
    secure_client.setHandshakeTimeout(20);
    secure_client.setTimeout(20);
    return http.begin(secure_client, url);
  }
  return http.begin(plain_client, url);
}

bool update_today_key() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 200)) {
    return false;
  }
  strftime(today_key, sizeof(today_key), "%Y-%m-%d", &timeinfo);
  return true;
}

bool wait_for_time_sync() {
  uint32_t start = millis();
  while (millis() - start < TIME_SYNC_TIMEOUT_MS) {
    if (update_today_key()) {
      LOG_SERIAL.printf("Time synced today=%s\n", today_key);
      return true;
    }
    lv_timer_handler();
    delay(250);
  }
  LOG_SERIAL.println("Time sync timed out");
  return false;
}

int parse_http_month(const String &month) {
  static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  for (int i = 0; i < 12; i++) {
    if (month == months[i]) return i + 1;
  }
  return 0;
}

bool update_today_from_http_date(const String &date_header) {
  if (date_header.length() < 16) return false;

  int comma = date_header.indexOf(',');
  int day_start = comma >= 0 ? comma + 2 : 0;
  if (date_header.length() < static_cast<size_t>(day_start + 11)) return false;

  int day = date_header.substring(day_start, day_start + 2).toInt();
  String month_text = date_header.substring(day_start + 3, day_start + 6);
  int month = parse_http_month(month_text);
  int year = date_header.substring(day_start + 7, day_start + 11).toInt();
  int hour = date_header.length() >= static_cast<size_t>(day_start + 14)
                 ? date_header.substring(day_start + 12, day_start + 14).toInt()
                 : 0;

  if (day <= 0 || month <= 0 || year <= 0) return false;

  struct tm server_tm = {};
  server_tm.tm_year = year - 1900;
  server_tm.tm_mon = month - 1;
  server_tm.tm_mday = day;
  server_tm.tm_hour = hour + 8;
  server_tm.tm_isdst = -1;
  mktime(&server_tm);
  strftime(today_key, sizeof(today_key), "%Y-%m-%d", &server_tm);
  LOG_SERIAL.printf("Time from HTTP Date today=%s header=%s\n", today_key, date_header.c_str());
  return true;
}

time_t parse_date(const char *date) {
  if (!date || strlen(date) < 10) return 0;
  struct tm tm_date = {};
  tm_date.tm_year = atoi(date) - 1900;
  tm_date.tm_mon = atoi(date + 5) - 1;
  tm_date.tm_mday = atoi(date + 8);
  tm_date.tm_isdst = -1;
  return mktime(&tm_date);
}

bool completion_key_exists(const char *key) {
  for (size_t i = 0; i < completion_count; i++) {
    if (strcmp(completion_keys[i], key) == 0) return true;
  }
  return false;
}

void set_completion_key(const char *key, bool completed) {
  for (size_t i = 0; i < completion_count; i++) {
    if (strcmp(completion_keys[i], key) == 0) {
      if (!completed) {
        for (size_t j = i; j + 1 < completion_count; j++) {
          copy_text(completion_keys[j], sizeof(completion_keys[j]), completion_keys[j + 1]);
        }
        completion_count--;
      }
      return;
    }
  }

  if (completed && completion_count < MAX_COMPLETIONS) {
    copy_text(completion_keys[completion_count], sizeof(completion_keys[completion_count]), key);
    completion_count++;
  }
}

String task_completion_key(const char *task_id) {
  return String(task_id) + "_" + today_key;
}

bool is_due_today(JsonObject task_fields) {
  if (task_fields["enabled"].is<bool>() && !task_fields["enabled"].as<bool>()) {
    return false;
  }

  JsonObject recurrence = task_fields["recurrence"].as<JsonObject>();
  const char *type = recurrence["type"] | "";
  time_t today = parse_date(today_key);
  if (today == 0) return true;

  if (strcmp(type, "intervalDays") == 0) {
    int every = recurrence["every"] | 1;
    const char *anchor = recurrence["anchorDate"] | today_key;
    time_t anchor_time = parse_date(anchor);
    if (anchor_time == 0 || every <= 0) return false;
    long diff = static_cast<long>((today - anchor_time) / 86400);
    return diff >= 0 && diff % every == 0;
  }

  struct tm today_tm = {};
  localtime_r(&today, &today_tm);
  if (strcmp(type, "monthlyDate") == 0) {
    int day = recurrence["day"] | 1;
    return today_tm.tm_mday == day;
  }

  if (strcmp(type, "weekly") == 0) {
    JsonArray days = recurrence["daysOfWeek"].as<JsonArray>();
    for (JsonVariant day : days) {
      if ((day | -1) == today_tm.tm_wday) return true;
    }
    return false;
  }

  return true;
}

const char *member_name_for(const char *member_id, JsonArray members, const char *fallback) {
  for (JsonVariant member : members) {
    JsonObject fields = member.as<JsonObject>();
    const char *id = fields["id"] | "";
    if (strcmp(id, member_id) == 0) {
      return fields["name"] | fallback;
    }
  }
  return fallback;
}

uint32_t member_color_for(const char *member_id, JsonArray members, uint32_t fallback) {
  for (JsonVariant member : members) {
    JsonObject fields = member.as<JsonObject>();
    const char *id = fields["id"] | "";
    if (strcmp(id, member_id) == 0) {
      return parse_color(fields["color"] | "", fallback);
    }
  }
  return fallback;
}

void read_completion_keys(JsonObject completions) {
  completion_count = 0;
  for (JsonPair item : completions) {
    if (completion_count >= MAX_COMPLETIONS) break;
    copy_text(completion_keys[completion_count], sizeof(completion_keys[completion_count]), item.key().c_str());
    completion_count++;
  }
}

uint32_t sync_retry_delay_ms() {
  uint32_t delay_ms = CLOUD_SYNC_RETRY_BASE_MS;
  for (uint8_t i = 1; i < cloud_sync_failure_streak && delay_ms < CLOUD_SYNC_RETRY_MAX_MS; i++) {
    delay_ms *= 2;
  }
  return min(delay_ms, CLOUD_SYNC_RETRY_MAX_MS);
}

void schedule_next_cloud_sync(bool success) {
  uint32_t delay_ms = success ? CLOUD_SYNC_INTERVAL_MS : sync_retry_delay_ms();
  next_cloud_sync_ms = millis() + delay_ms;
}

void record_sync_failure(const char *error) {
  copy_text(last_sync_error, sizeof(last_sync_error), error && error[0] ? error : "sync failed");
  if (cloud_sync_failure_streak < 255) cloud_sync_failure_streak++;
  if (cloud_sync_failure_streak >= SYNC_FAILURE_NOTIFY_THRESHOLD) {
    sync_failure_report_pending = true;
  }
  schedule_next_cloud_sync(false);
}

void record_sync_success() {
  cloud_sync_failure_streak = 0;
  copy_text(last_sync_error, sizeof(last_sync_error), "");
  schedule_next_cloud_sync(true);
}

bool apply_cloud_document(const String &payload) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    LOG_SERIAL.printf("LifeTodo API JSON parse failed: %s\n", err.c_str());
    return false;
  }

  JsonObject state = doc["state"].as<JsonObject>();
  if (state.isNull()) return false;

  JsonArray members = state["members"].as<JsonArray>();
  JsonArray cloud_tasks = state["tasks"].as<JsonArray>();
  read_completion_keys(state["completions"].as<JsonObject>());

  size_t next_count = 0;
  for (JsonVariant item : cloud_tasks) {
    if (next_count >= TASK_COUNT) break;
    JsonObject task_fields = item.as<JsonObject>();
    if (task_fields.isNull()) continue;
    if (task_fields["enabled"].is<bool>() && !task_fields["enabled"].as<bool>()) continue;

    Task &task = tasks[next_count];
    const char *id = task_fields["id"] | "";
    const char *title = task_fields["title"] | "事项";
    const char *label = task_fields["nextDate"] | "";
    const char *assignee_id = task_fields["assigneeId"] | "";
    String done_key = task_completion_key(id);

    copy_text(task.id, sizeof(task.id), id);
    copy_text(task.title, sizeof(task.title), title);
    if (label && label[0] != '\0') {
      String next_label = String("下次 ") + label;
      copy_text(task.label, sizeof(task.label), next_label.c_str());
    } else {
      copy_text(task.label, sizeof(task.label), task_fields["label"] | "");
    }
    copy_text(task.member, sizeof(task.member), member_name_for(assignee_id, members, "成员"));
    task.color = member_color_for(assignee_id, members, 0xef7f65);
    task.done = completion_key_exists(done_key.c_str());
    next_count++;
  }

  task_count = next_count;
  size_t max_page = task_count == 0 ? 0 : (task_count - 1) / TASKS_PER_PAGE;
  if (task_page > max_page) task_page = max_page;
  cloud_ready = true;
  LOG_SERIAL.printf("Cloud sync ok home=%s today=%s tasks=%u completions=%u\n",
                    LIFETODO_HOME_ID, today_key, static_cast<unsigned>(task_count),
                    static_cast<unsigned>(completion_count));
  return true;
}

bool sync_from_cloud() {
  if (WiFi.status() != WL_CONNECTED) return false;
  wait_for_time_sync();

  lv_label_set_text(status_label, cloud_ready ? "飞书已连接" : "正在同步");
  copy_text(last_sync_error, sizeof(last_sync_error), "");

  String url = state_api_url();
  LOG_SERIAL.printf("LifeTodo API GET url=%s\n", url.c_str());
  for (uint8_t attempt = 1; attempt <= CLOUD_SYNC_ATTEMPTS; attempt++) {
    WiFiClient plain_client;
    WiFiClientSecure client;

    HTTPClient http;
    http.setTimeout(20000);
    http.setReuse(false);
    http.useHTTP10(true);
    http.addHeader("Connection", "close");
    const char *header_keys[] = {"Date"};
    http.collectHeaders(header_keys, 1);
    if (!begin_lifetodo_http(http, plain_client, client, url)) {
      LOG_SERIAL.printf("LifeTodo API HTTP begin failed attempt=%u\n", attempt);
      copy_text(last_sync_error, sizeof(last_sync_error), "http begin failed");
      lv_label_set_text(status_label, "同步失败");
      delay(CLOUD_SYNC_RETRY_DELAY_MS);
      continue;
    }

    int code = http.GET();
    String date_header = http.header("Date");
    String payload = http.getString();
    http.end();

    if (code != 200) {
      LOG_SERIAL.printf("LifeTodo API GET failed attempt=%u code=%d error=%s payload=%s\n",
                        attempt, code, http.errorToString(code).c_str(), payload.c_str());
      copy_text(last_sync_error, sizeof(last_sync_error), http.errorToString(code).c_str());
      lv_label_set_text(status_label, cloud_ready ? "飞书已连接" : "同步重试");
      delay(CLOUD_SYNC_RETRY_DELAY_MS);
      continue;
    }

    update_today_from_http_date(date_header);
    bool ok = apply_cloud_document(payload);
    if (ok) {
      record_sync_success();
      update_summary();
      render_tasks();
      lv_label_set_text(status_label, task_count == 0 ? "暂无待办" : "飞书已连接");
      return true;
    }

    copy_text(last_sync_error, sizeof(last_sync_error), "invalid response");
    lv_label_set_text(status_label, cloud_ready ? "飞书已连接" : "同步重试");
    delay(CLOUD_SYNC_RETRY_DELAY_MS);
  }

  record_sync_failure(last_sync_error);
  lv_label_set_text(status_label, cloud_ready ? "飞书已连接" : "同步失败");
  update_control_center_status();
  return false;
}

bool push_completion_to_cloud(const char *task_id, bool completed) {
  if (WiFi.status() != WL_CONNECTED) return false;

  JsonDocument body;
  body["taskId"] = task_id;
  body["date"] = today_key;
  body["completed"] = completed;
  body["source"] = "device-esp32";

  String payload;
  serializeJson(body, payload);

  WiFiClient plain_client;
  WiFiClientSecure client;
  HTTPClient http;
  String url = completions_api_url();
  LOG_SERIAL.printf("LifeTodo API POST url=%s\n", url.c_str());
  http.setTimeout(20000);
  http.setReuse(false);
  http.useHTTP10(true);
  if (!begin_lifetodo_http(http, plain_client, client, url)) {
    LOG_SERIAL.println("LifeTodo API POST begin failed");
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  int code = http.POST(payload);
  String response = http.getString();
  http.end();

  if (code < 200 || code >= 300) {
    LOG_SERIAL.printf("LifeTodo API POST failed code=%d error=%s payload=%s\n",
                      code, http.errorToString(code).c_str(), response.c_str());
    return false;
  }
  LOG_SERIAL.printf("LifeTodo completion updated task=%s completed=%s\n", task_id, completed ? "true" : "false");
  return true;
}

void queue_completion_sync(const char *task_id, bool completed) {
  for (size_t i = 0; i < pending_completion_count; i++) {
    if (strcmp(pending_completions[i].task_id, task_id) == 0) {
      pending_completions[i].completed = completed;
      pending_completions[i].attempts = 0;
      pending_completions[i].next_try_ms = millis() + 700;
      return;
    }
  }

  if (pending_completion_count >= MAX_PENDING_COMPLETIONS) {
    pending_completion_count = MAX_PENDING_COMPLETIONS - 1;
    for (size_t i = 0; i + 1 < pending_completion_count; i++) {
      pending_completions[i] = pending_completions[i + 1];
    }
  }

  PendingCompletion &item = pending_completions[pending_completion_count++];
  copy_text(item.task_id, sizeof(item.task_id), task_id);
  item.completed = completed;
  item.attempts = 0;
  item.next_try_ms = millis() + 700;
}

void remove_pending_completion(size_t index) {
  if (index >= pending_completion_count) return;
  for (size_t i = index; i + 1 < pending_completion_count; i++) {
    pending_completions[i] = pending_completions[i + 1];
  }
  pending_completion_count--;
}

void process_pending_completion_sync() {
  if (pending_completion_count == 0 || WiFi.status() != WL_CONNECTED) return;

  uint32_t now_ms = millis();
  PendingCompletion &item = pending_completions[0];
  if (now_ms < item.next_try_ms) return;

  lv_label_set_text(status_label, "正在同步更改");
  bool ok = push_completion_to_cloud(item.task_id, item.completed);
  if (ok) {
    remove_pending_completion(0);
    lv_label_set_text(status_label, pending_completion_count == 0 ? "飞书已连接" : "继续同步");
    return;
  }

  item.attempts++;
  if (item.attempts >= 4) {
    lv_label_set_text(status_label, "同步稍后重试");
    item.attempts = 0;
    item.next_try_ms = now_ms + 30000;
  } else {
    lv_label_set_text(status_label, "同步重试");
    item.next_try_ms = now_ms + 3000;
  }
}

bool report_sync_failure_to_cloud() {
  if (WiFi.status() != WL_CONNECTED) return false;

  JsonDocument body;
  body["deviceId"] = LIFETODO_DEVICE_ID;
  body["failures"] = cloud_sync_failure_streak;
  body["error"] = last_sync_error[0] ? last_sync_error : "sync failed";
  body["source"] = "device-esp32";

  String payload;
  serializeJson(body, payload);

  WiFiClient plain_client;
  WiFiClientSecure client;
  HTTPClient http;
  String url = sync_failure_api_url();
  LOG_SERIAL.printf("LifeTodo sync failure report url=%s\n", url.c_str());
  http.setTimeout(12000);
  http.setReuse(false);
  http.useHTTP10(true);
  if (!begin_lifetodo_http(http, plain_client, client, url)) {
    LOG_SERIAL.println("LifeTodo sync failure report begin failed");
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  int code = http.POST(payload);
  String response = http.getString();
  http.end();

  if (code < 200 || code >= 300) {
    LOG_SERIAL.printf("LifeTodo sync failure report failed code=%d error=%s payload=%s\n",
                      code, http.errorToString(code).c_str(), response.c_str());
    return false;
  }
  LOG_SERIAL.println("LifeTodo sync failure reported");
  return true;
}

void process_sync_failure_report() {
  if (!sync_failure_report_pending || WiFi.status() != WL_CONNECTED) return;
  uint32_t now_ms = millis();
  if (now_ms < next_sync_failure_report_ms) return;

  if (report_sync_failure_to_cloud()) {
    sync_failure_report_pending = false;
    next_sync_failure_report_ms = 0;
  } else {
    next_sync_failure_report_ms = now_ms + SYNC_FAILURE_REPORT_RETRY_MS;
  }
}

void flush_display(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  display->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(&color_p->full), w, h);
  lv_disp_flush_ready(drv);
}

void read_touch(lv_indev_drv_t *, lv_indev_data_t *data) {
  touch.read();
  if (touch.isTouched) {
    int16_t x = touch.points[0].x;
    int16_t y = touch.points[0].y;
    if (!touch_down) {
      touch_start_x = x;
      touch_start_y = y;
    }
    touch_down = true;
    touch_release_started_ms = 0;
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
    touch_last_x = x;
    touch_last_y = y;
    if (millis() - last_touch_log_ms > 250) {
      LOG_SERIAL.printf("Touch x=%d y=%d\n", data->point.x, data->point.y);
      last_touch_log_ms = millis();
    }
  } else {
    if (touch_down) {
      touch_down = false;
      touch_release_started_ms = millis();
      int16_t dx = touch_last_x - touch_start_x;
      int16_t dy = touch_last_y - touch_start_y;
      if (abs(dy) >= 30 && abs(dy) >= abs(dx) * 2) {
        if (touch_start_y <= 74) {
          set_brightness_panel_open(dy > 0);
        } else if (task_count > TASKS_PER_PAGE && touch_start_y >= 110 && touch_start_y <= 430) {
          size_t max_page = (task_count - 1) / TASKS_PER_PAGE;
          if (dy < 0 && task_page < max_page) {
            task_page++;
            update_task_page();
          } else if (dy > 0 && task_page > 0) {
            task_page--;
            update_task_page();
          }
        }
      }
    }
    if (!task_click_armed && touch_release_started_ms != 0 &&
        millis() - touch_release_started_ms > TASK_CLICK_REARM_RELEASE_MS) {
      task_click_armed = true;
    }
    data->state = LV_INDEV_STATE_REL;
  }
}

uint8_t remaining_count() {
  uint8_t remaining = 0;
  for (size_t i = 0; i < task_count; i++) {
    if (!tasks[i].done) remaining++;
  }
  return remaining;
}

void update_summary() {
  static char buffer[96];
  uint8_t remaining = remaining_count();
  if (task_count == 0) {
    snprintf(buffer, sizeof(buffer), cloud_ready ? "今天没有待办" : "正在连接飞书");
  } else if (remaining == 0) {
    snprintf(buffer, sizeof(buffer), "全部完成 · 共 %u 件", static_cast<unsigned>(task_count));
  } else {
    snprintf(buffer, sizeof(buffer), "%u 件待完成 · 共 %u 件", remaining, static_cast<unsigned>(task_count));
  }
  lv_label_set_text(summary_label, buffer);
}

void style_panel(lv_obj_t *obj, uint32_t color, bool done) {
  lv_color_t bg = lv_color_hex(done ? 0xf0eee8 : 0xffffff);
  lv_color_t border = lv_color_hex(done ? 0xe0dacf : 0xeee7da);

  lv_obj_set_style_radius(obj, 10, 0);
  lv_obj_set_style_bg_color(obj, bg, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(obj, border, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_style_shadow_width(obj, done ? 0 : 5, 0);
  lv_obj_set_style_shadow_color(obj, lv_color_hex(color), 0);
  lv_obj_set_style_shadow_opa(obj, done ? LV_OPA_TRANSP : LV_OPA_10, 0);

  lv_obj_set_style_bg_color(obj, lv_color_hex(done ? 0xe9e4db : 0xf8f4ec), LV_STATE_PRESSED);
  lv_obj_set_style_shadow_width(obj, 0, LV_STATE_PRESSED);
}

void apply_task_view(size_t index) {
  if (index >= TASK_COUNT) return;

  size_t start = task_page * TASKS_PER_PAGE;
  if (index < start || index >= start + TASKS_PER_PAGE) return;
  size_t slot = index - start;
  Task &item = tasks[index];
  TaskView &view = task_views[slot];
  if (!view.card || !view.accent || !view.check || !view.member || !view.title || !view.label || !view.status) return;

  lv_obj_clear_flag(view.card, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(view.member, item.member);
  lv_label_set_text(view.title, item.title);
  lv_label_set_text(view.label, item.label);
  style_panel(view.card, item.color, item.done);
  lv_obj_set_style_bg_color(view.accent, lv_color_hex(item.done ? 0xc8c0b4 : item.color), 0);
  lv_obj_set_style_bg_color(view.check, lv_color_hex(item.done ? 0x2f7d4f : 0xf7f1e7), 0);
  lv_obj_set_style_border_color(view.check, lv_color_hex(item.done ? 0x2f7d4f : item.color), 0);
  lv_obj_set_style_text_color(view.member, lv_color_hex(item.done ? 0x8d857b : item.color), 0);
  lv_obj_set_style_text_color(view.title, lv_color_hex(item.done ? 0x8b8378 : 0x171512), 0);
  lv_obj_set_style_text_color(view.label, lv_color_hex(0x8f8678), 0);
  lv_obj_set_style_text_color(view.status, lv_color_hex(item.done ? 0xffffff : item.color), 0);
  lv_label_set_text(view.status, item.done ? "✓" : "");
  lv_obj_invalidate(view.card);
}

void update_task_page() {
  if (task_count == 0) return;

  size_t max_page = (task_count - 1) / TASKS_PER_PAGE;
  if (task_page > max_page) task_page = max_page;
  size_t start = task_page * TASKS_PER_PAGE;

  for (size_t slot = 0; slot < TASKS_PER_PAGE; slot++) {
    size_t task_index = start + slot;
    if (task_index < task_count) {
      apply_task_view(task_index);
    } else if (task_views[slot].card) {
      lv_obj_add_flag(task_views[slot].card, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (task_page_label) {
    if (task_count > TASKS_PER_PAGE) {
      static char page_buffer[24];
      snprintf(page_buffer, sizeof(page_buffer), "%u / %u",
               static_cast<unsigned>(task_page + 1),
               static_cast<unsigned>(max_page + 1));
      lv_label_set_text(task_page_label, page_buffer);
      lv_obj_clear_flag(task_page_label, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(task_page_label, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void render_tasks() {
  lv_obj_clean(task_grid);
  task_page_label = nullptr;
  for (size_t i = 0; i < TASK_COUNT; i++) {
    task_views[i] = TaskView();
  }

  if (task_count == 0) {
    lv_obj_t *panel = lv_obj_create(task_grid);
    lv_obj_set_size(panel, 760, 236);
    make_static_touch_obj(panel);
    lv_obj_set_style_radius(panel, 12, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_80, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xeee7da), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_pad_all(panel, 26, 0);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, cloud_ready ? "今天没有安排" : "正在同步飞书");
    set_cjk_title_font(title);
    lv_obj_set_style_text_color(title, lv_color_hex(0x171512), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 10);

    lv_obj_t *body = lv_label_create(panel);
    lv_label_set_text(body, cloud_ready ? "可以放心做自己的事，新的家庭事项会自动出现。" : "保持设备联网，稍后会自动刷新。");
    set_cjk_font(body);
    lv_obj_set_width(body, 680);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(body, lv_color_hex(0x776f64), 0);
    lv_obj_align(body, LV_ALIGN_TOP_LEFT, 0, 68);
    return;
  }

  for (size_t slot = 0; slot < TASKS_PER_PAGE; slot++) {
    lv_obj_t *btn = lv_obj_create(task_grid);
    lv_obj_set_size(btn, 760, 88);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, slot * 98);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    make_static_touch_obj(btn);
    lv_obj_add_event_cb(btn, task_click, LV_EVENT_CLICKED, reinterpret_cast<void *>(slot));
    task_views[slot].card = btn;

    lv_obj_t *accent = lv_obj_create(btn);
    lv_obj_set_size(accent, 7, 86);
    lv_obj_align(accent, LV_ALIGN_LEFT_MID, 0, 0);
    make_static_touch_obj(accent);
    lv_obj_set_style_radius(accent, 0, 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    task_views[slot].accent = accent;

    lv_obj_t *check = lv_obj_create(btn);
    lv_obj_set_size(check, 42, 42);
    lv_obj_align(check, LV_ALIGN_LEFT_MID, 24, 0);
    make_static_touch_obj(check);
    lv_obj_set_style_radius(check, 21, 0);
    lv_obj_set_style_border_width(check, 2, 0);
    task_views[slot].check = check;

    lv_obj_t *done = lv_label_create(check);
    lv_obj_set_style_text_font(done, &lv_font_montserrat_24, 0);
    lv_obj_center(done);
    task_views[slot].status = done;

    lv_obj_t *member = lv_label_create(btn);
    lv_obj_set_width(member, 110);
    set_cjk_font(member);
    lv_obj_align(member, LV_ALIGN_TOP_RIGHT, -22, 15);
    task_views[slot].member = member;

    lv_obj_t *title = lv_label_create(btn);
    lv_obj_set_width(title, 470);
    set_cjk_title_font(title);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 82, 12);
    task_views[slot].title = title;

    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_width(label, 470);
    set_cjk_font(label);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 82, 48);
    task_views[slot].label = label;
  }

  task_page_label = lv_label_create(task_grid);
  lv_obj_set_style_text_font(task_page_label, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(task_page_label, lv_color_hex(0x9a9184), 0);
  lv_obj_align(task_page_label, LV_ALIGN_BOTTOM_RIGHT, -8, 0);
  update_task_page();
}

void task_grid_gesture(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_GESTURE || task_count <= TASKS_PER_PAGE) return;
  lv_indev_t *indev = static_cast<lv_indev_t *>(lv_event_get_param(event));
  if (!indev) return;

  int16_t dx = touch_last_x - touch_start_x;
  int16_t dy = touch_last_y - touch_start_y;
  if (abs(dy) < 36 || abs(dy) < abs(dx) * 2) return;

  size_t max_page = (task_count - 1) / TASKS_PER_PAGE;
  lv_dir_t dir = lv_indev_get_gesture_dir(indev);
  if (dir == LV_DIR_TOP && task_page < max_page) {
    task_page++;
    update_task_page();
  } else if (dir == LV_DIR_BOTTOM && task_page > 0) {
    task_page--;
    update_task_page();
  }
}

void task_click(lv_event_t *event) {
  size_t slot = reinterpret_cast<size_t>(lv_event_get_user_data(event));
  size_t task_index = task_page * TASKS_PER_PAGE + slot;
  if (task_index >= task_count) return;
  Task *task = &tasks[task_index];
  if (!task_click_armed) return;

  task->done = !task->done;
  String key = task_completion_key(task->id);
  set_completion_key(key.c_str(), task->done);
  task_click_armed = false;
  LOG_SERIAL.printf("Task toggled id=%s done=%s\n", task->id, task->done ? "true" : "false");
  update_summary();

  apply_task_view(task_index);

  lv_label_set_text(status_label, "待同步");
  queue_completion_sync(task->id, task->done);
}

void build_task_grid() {
  task_grid = lv_obj_create(root);
  lv_obj_set_size(task_grid, 760, 302);
  lv_obj_align(task_grid, LV_ALIGN_TOP_MID, 0, 118);
  make_static_touch_obj(task_grid);
  lv_obj_add_flag(task_grid, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(task_grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(task_grid, 0, 0);
  lv_obj_set_style_pad_all(task_grid, 0, 0);

  render_tasks();
}

void brightness_slider_event(lv_event_t *) {
  brightness_percent = lv_slider_get_value(brightness_slider);
  update_brightness_visuals();
  lv_obj_invalidate(brightness_overlay);
}

void update_brightness_visuals() {
  uint8_t dim_opa = brightness_percent >= 100 ? 0 : map(100 - brightness_percent, 0, 80, 0, 210);
  lv_obj_set_style_bg_opa(brightness_overlay, dim_opa, 0);

  static char buffer[8];
  snprintf(buffer, sizeof(buffer), "%u%%", brightness_percent);
  lv_label_set_text(brightness_value_label, buffer);
}

void update_control_center_status() {
  if (control_wifi_status_label) {
    lv_label_set_text(control_wifi_status_label, WiFi.status() == WL_CONNECTED ? "已连接" : (provisioning_active ? "配网中" : "未连接"));
  }
  if (control_sync_status_label) {
    lv_label_set_text(control_sync_status_label, cloud_ready ? "飞书已连接" : "正在同步");
  }
}

void refresh_wifi_state_labels() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  const char *text = provisioning_active ? "Wi-Fi 配网中" : (connected ? "Wi-Fi 已连接" : "Wi-Fi 未连接");
  if (status_label) {
    lv_label_set_text(status_label, text);
  }
  update_control_center_status();
}

void set_brightness_panel_open(bool open) {
  brightness_panel_open = open;
  if (open) {
    update_control_center_status();
  }
  lv_obj_clear_flag(brightness_panel, LV_OBJ_FLAG_HIDDEN);
  if (open) {
    lv_obj_add_flag(brightness_overlay, LV_OBJ_FLAG_CLICKABLE);
  } else {
    lv_obj_clear_flag(brightness_overlay, LV_OBJ_FLAG_CLICKABLE);
  }
  lv_obj_move_foreground(brightness_overlay);
  lv_obj_move_foreground(brightness_panel);
  lv_obj_set_y(brightness_panel, open ? 22 : -456);
}

void control_wifi_event(lv_event_t *) {
  set_brightness_panel_open(false);
  show_wifi_status_panel();
}

void control_sync_event(lv_event_t *) {
  set_brightness_panel_open(false);
  sync_from_cloud();
}

void brightness_gesture_event(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_CLICKED) {
    lv_obj_t *target = lv_event_get_target(event);
    if (target == brightness_overlay) {
      set_brightness_panel_open(false);
    } else if (target == brightness_gesture_zone) {
      set_brightness_panel_open(!brightness_panel_open);
    }
    return;
  }

  lv_indev_t *indev = static_cast<lv_indev_t *>(lv_event_get_param(event));
  if (!indev) return;

  lv_dir_t dir = lv_indev_get_gesture_dir(indev);
  int16_t dx = touch_last_x - touch_start_x;
  int16_t dy = touch_last_y - touch_start_y;
  if (abs(dy) < 42 || abs(dy) < abs(dx) * 2) return;

  if (dir == LV_DIR_BOTTOM) {
    set_brightness_panel_open(true);
  } else if (dir == LV_DIR_TOP) {
    set_brightness_panel_open(false);
  }
}

void build_brightness_controls() {
  lv_obj_t *top_layer = lv_layer_top();

  brightness_overlay = lv_obj_create(top_layer);
  lv_obj_set_size(brightness_overlay, SCREEN_W, SCREEN_H);
  lv_obj_align(brightness_overlay, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(brightness_overlay, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(brightness_overlay, 0, 0);
  lv_obj_set_style_border_width(brightness_overlay, 0, 0);
  lv_obj_set_style_radius(brightness_overlay, 0, 0);
  lv_obj_set_style_pad_all(brightness_overlay, 0, 0);
  make_static_touch_obj(brightness_overlay);
  lv_obj_clear_flag(brightness_overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(brightness_overlay, brightness_gesture_event, LV_EVENT_CLICKED, nullptr);

  brightness_panel = lv_obj_create(top_layer);
  lv_obj_set_size(brightness_panel, 760, 430);
  lv_obj_align(brightness_panel, LV_ALIGN_TOP_MID, 0, -456);
  make_static_touch_obj(brightness_panel);
  lv_obj_set_style_radius(brightness_panel, 18, 0);
  lv_obj_set_style_bg_color(brightness_panel, lv_color_hex(0x2f312f), 0);
  lv_obj_set_style_bg_opa(brightness_panel, LV_OPA_90, 0);
  lv_obj_set_style_border_color(brightness_panel, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_border_opa(brightness_panel, LV_OPA_20, 0);
  lv_obj_set_style_border_width(brightness_panel, 1, 0);
  lv_obj_set_style_pad_all(brightness_panel, 20, 0);
  lv_obj_add_event_cb(brightness_panel, brightness_gesture_event, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *wifi_card = lv_obj_create(brightness_panel);
  lv_obj_set_size(wifi_card, 330, 158);
  lv_obj_align(wifi_card, LV_ALIGN_TOP_LEFT, 0, 0);
  make_static_touch_obj(wifi_card);
  lv_obj_add_flag(wifi_card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(wifi_card, 24, 0);
  lv_obj_set_style_bg_color(wifi_card, lv_color_hex(0x6b6f6d), 0);
  lv_obj_set_style_bg_opa(wifi_card, LV_OPA_70, 0);
  lv_obj_set_style_border_width(wifi_card, 1, 0);
  lv_obj_set_style_border_color(wifi_card, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_border_opa(wifi_card, LV_OPA_20, 0);
  lv_obj_add_event_cb(wifi_card, control_wifi_event, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *wifi_icon = lv_obj_create(wifi_card);
  lv_obj_set_size(wifi_icon, 74, 74);
  lv_obj_align(wifi_icon, LV_ALIGN_TOP_LEFT, 12, 12);
  make_static_touch_obj(wifi_icon);
  lv_obj_clear_flag(wifi_icon, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(wifi_icon, 37, 0);
  lv_obj_set_style_bg_color(wifi_icon, lv_color_hex(0x1f8fff), 0);
  lv_obj_set_style_border_width(wifi_icon, 0, 0);
  lv_obj_t *wifi_text = lv_label_create(wifi_icon);
  lv_label_set_text(wifi_text, "Wi");
  lv_obj_set_style_text_font(wifi_text, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(wifi_text, lv_color_hex(0xffffff), 0);
  lv_obj_center(wifi_text);

  lv_obj_t *wifi_title = lv_label_create(wifi_card);
  lv_label_set_text(wifi_title, "Wi-Fi");
  lv_obj_set_style_text_font(wifi_title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(wifi_title, lv_color_hex(0xfffdfa), 0);
  lv_obj_align(wifi_title, LV_ALIGN_TOP_LEFT, 104, 24);

  control_wifi_status_label = lv_label_create(wifi_card);
  set_cjk_font(control_wifi_status_label);
  lv_obj_set_style_text_color(control_wifi_status_label, lv_color_hex(0xd8d0c3), 0);
  lv_obj_align(control_wifi_status_label, LV_ALIGN_TOP_LEFT, 104, 70);

  lv_obj_t *bt_card = lv_obj_create(brightness_panel);
  lv_obj_set_size(bt_card, 150, 150);
  lv_obj_align(bt_card, LV_ALIGN_TOP_LEFT, 0, 178);
  make_static_touch_obj(bt_card);
  lv_obj_set_style_radius(bt_card, 24, 0);
  lv_obj_set_style_bg_color(bt_card, lv_color_hex(0x737674), 0);
  lv_obj_set_style_bg_opa(bt_card, LV_OPA_50, 0);
  lv_obj_set_style_border_width(bt_card, 1, 0);
  lv_obj_set_style_border_color(bt_card, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_border_opa(bt_card, LV_OPA_20, 0);
  lv_obj_t *bt_label = lv_label_create(bt_card);
  lv_label_set_text(bt_label, "BT");
  lv_obj_set_style_text_font(bt_label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(bt_label, lv_color_hex(0xfffdfa), 0);
  lv_obj_center(bt_label);

  lv_obj_t *sync_card = lv_obj_create(brightness_panel);
  lv_obj_set_size(sync_card, 158, 150);
  lv_obj_align(sync_card, LV_ALIGN_TOP_LEFT, 172, 178);
  make_static_touch_obj(sync_card);
  lv_obj_add_flag(sync_card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(sync_card, 24, 0);
  lv_obj_set_style_bg_color(sync_card, lv_color_hex(0x737674), 0);
  lv_obj_set_style_bg_opa(sync_card, LV_OPA_50, 0);
  lv_obj_set_style_border_width(sync_card, 1, 0);
  lv_obj_set_style_border_color(sync_card, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_border_opa(sync_card, LV_OPA_20, 0);
  lv_obj_add_event_cb(sync_card, control_sync_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *sync_label = lv_label_create(sync_card);
  lv_label_set_text(sync_label, "SYNC");
  lv_obj_set_style_text_font(sync_label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(sync_label, lv_color_hex(0xfffdfa), 0);
  lv_obj_align(sync_label, LV_ALIGN_TOP_MID, 0, 36);
  control_sync_status_label = lv_label_create(sync_card);
  set_cjk_font(control_sync_status_label);
  lv_obj_set_style_text_color(control_sync_status_label, lv_color_hex(0xd8d0c3), 0);
  lv_obj_align(control_sync_status_label, LV_ALIGN_BOTTOM_MID, 0, -28);

  lv_obj_t *brightness_card = lv_obj_create(brightness_panel);
  lv_obj_set_size(brightness_card, 170, 328);
  lv_obj_align(brightness_card, LV_ALIGN_TOP_RIGHT, -196, 0);
  make_static_touch_obj(brightness_card);
  lv_obj_set_style_radius(brightness_card, 36, 0);
  lv_obj_set_style_bg_color(brightness_card, lv_color_hex(0x777a78), 0);
  lv_obj_set_style_bg_opa(brightness_card, LV_OPA_60, 0);
  lv_obj_set_style_border_width(brightness_card, 1, 0);
  lv_obj_set_style_border_color(brightness_card, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_border_opa(brightness_card, LV_OPA_20, 0);

  lv_obj_t *label = lv_label_create(brightness_card);
  lv_label_set_text(label, "LIGHT");
  lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xfffdfa), 0);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

  brightness_slider = lv_slider_create(brightness_card);
  lv_obj_set_size(brightness_slider, 70, 210);
  lv_obj_align(brightness_slider, LV_ALIGN_CENTER, 0, 22);
  make_static_touch_obj(brightness_slider);
  lv_slider_set_range(brightness_slider, 20, 100);
  lv_slider_set_value(brightness_slider, brightness_percent, LV_ANIM_OFF);
  lv_obj_set_style_radius(brightness_slider, 34, LV_PART_MAIN);
  lv_obj_set_style_radius(brightness_slider, 34, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0xe7e2d8), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(brightness_slider, LV_OPA_90, LV_PART_MAIN);
  lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0xf1c45b), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0xfffdfa), LV_PART_KNOB);
  lv_obj_set_style_pad_all(brightness_slider, 12, LV_PART_KNOB);
  lv_obj_add_event_cb(brightness_slider, brightness_slider_event, LV_EVENT_VALUE_CHANGED, nullptr);

  brightness_value_label = lv_label_create(brightness_card);
  lv_obj_set_style_text_font(brightness_value_label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(brightness_value_label, lv_color_hex(0xfffdfa), 0);
  lv_obj_align(brightness_value_label, LV_ALIGN_BOTTOM_MID, 0, -18);
  update_brightness_visuals();

  lv_obj_t *device_card = lv_obj_create(brightness_panel);
  lv_obj_set_size(device_card, 170, 328);
  lv_obj_align(device_card, LV_ALIGN_TOP_RIGHT, 0, 0);
  make_static_touch_obj(device_card);
  lv_obj_set_style_radius(device_card, 36, 0);
  lv_obj_set_style_bg_color(device_card, lv_color_hex(0x777a78), 0);
  lv_obj_set_style_bg_opa(device_card, LV_OPA_50, 0);
  lv_obj_set_style_border_width(device_card, 1, 0);
  lv_obj_set_style_border_color(device_card, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_border_opa(device_card, LV_OPA_20, 0);
  lv_obj_t *device_label = lv_label_create(device_card);
  lv_label_set_text(device_label, "LifeTodo");
  lv_obj_set_style_text_font(device_label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(device_label, lv_color_hex(0xfffdfa), 0);
  lv_obj_center(device_label);

  brightness_gesture_zone = lv_obj_create(root);
  lv_obj_set_size(brightness_gesture_zone, SCREEN_W, 66);
  lv_obj_align(brightness_gesture_zone, LV_ALIGN_TOP_MID, 0, 0);
  make_static_touch_obj(brightness_gesture_zone);
  lv_obj_add_flag(brightness_gesture_zone, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(brightness_gesture_zone, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(brightness_gesture_zone, 0, 0);
  lv_obj_set_style_pad_all(brightness_gesture_zone, 0, 0);
  lv_obj_add_event_cb(brightness_gesture_zone, brightness_gesture_event, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *handle = lv_obj_create(brightness_gesture_zone);
  lv_obj_set_size(handle, 180, 14);
  lv_obj_align(handle, LV_ALIGN_TOP_MID, 0, 0);
  make_static_touch_obj(handle);
  lv_obj_clear_flag(handle, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(handle, 7, 0);
  lv_obj_set_style_bg_color(handle, lv_color_hex(0x171512), 0);
  lv_obj_set_style_bg_opa(handle, LV_OPA_30, 0);
  lv_obj_set_style_border_width(handle, 0, 0);
}

void set_wifi_setup_visible(bool visible, const char *title, const char *body, const char *hint) {
  if (!wifi_setup_panel) return;

  if (!visible) {
    lv_obj_add_flag(wifi_setup_panel, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  lv_label_set_text(wifi_setup_title, title ? title : "Wi-Fi 配网");
  static char body_buffer[96];
  if (title && strcmp(title, "设备联网") == 0) {
    snprintf(body_buffer, sizeof(body_buffer), "设备ID：%s", body ? body : "");
  } else if (title && strcmp(title, "Wi-Fi 连接中") == 0) {
    snprintf(body_buffer, sizeof(body_buffer), "正在连接 Wi-Fi");
  } else {
    snprintf(body_buffer, sizeof(body_buffer), "%s", body ? body : "");
  }
  lv_label_set_text(wifi_setup_body, body_buffer);
  lv_label_set_text(wifi_setup_hint, hint ? hint : "");
  lv_label_set_text(wifi_setup_steps, "1 连接热点\n2 打开 192.168.4.1\n3 选择家里 Wi-Fi");
  if (wifi_disconnect_btn) lv_obj_add_flag(wifi_disconnect_btn, LV_OBJ_FLAG_HIDDEN);
  if (wifi_reconnect_btn) lv_obj_add_flag(wifi_reconnect_btn, LV_OBJ_FLAG_HIDDEN);
  if (wifi_reprovision_btn) lv_obj_add_flag(wifi_reprovision_btn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(wifi_setup_panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(wifi_setup_panel);
}

void show_wifi_action_buttons() {
  if (wifi_disconnect_btn) lv_obj_clear_flag(wifi_disconnect_btn, LV_OBJ_FLAG_HIDDEN);
  if (wifi_reconnect_btn) lv_obj_clear_flag(wifi_reconnect_btn, LV_OBJ_FLAG_HIDDEN);
  if (wifi_reprovision_btn) lv_obj_clear_flag(wifi_reprovision_btn, LV_OBJ_FLAG_HIDDEN);
}

void show_wifi_status_panel() {
  if (!wifi_setup_panel) return;

  bool connected = WiFi.status() == WL_CONNECTED;
  String ssid = connected ? WiFi.SSID() : saved_wifi_ssid();
  String ip = connected ? WiFi.localIP().toString() : String("--");
  static char body_buffer[128];
  static char hint_buffer[128];
  snprintf(body_buffer, sizeof(body_buffer), connected ? "当前网络：%s" : "当前未联网", ssid.length() ? ssid.c_str() : "未设置");
  snprintf(hint_buffer, sizeof(hint_buffer), connected ? "IP：%s" : "已记忆网络：%s", ip.c_str(), ssid.length() ? ssid.c_str() : "无");

  lv_label_set_text(wifi_setup_title, "设备联网");
  lv_label_set_text(wifi_setup_body, body_buffer);
  lv_label_set_text(wifi_setup_steps, connected ? "设备已连接家庭 Wi-Fi\n可断开、重连或重新联网" : "设备未连接家庭 Wi-Fi\n可重连已记忆网络或重新联网");
  lv_label_set_text(wifi_setup_hint, hint_buffer);
  show_wifi_action_buttons();
  lv_obj_clear_flag(wifi_setup_panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(wifi_setup_panel);
  refresh_wifi_state_labels();
}

void wifi_setup_close_event(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_CLICKED) {
    set_wifi_setup_visible(false, "", "", "");
  }
}

void wifi_disconnect_event(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  if (provisioning_active) {
    stop_wifi_provisioning();
  }
  WiFi.disconnect(false, false);
  cloud_ready = false;
  refresh_wifi_state_labels();
  show_wifi_status_panel();
}

void wifi_reconnect_event(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  if (saved_wifi_ssid().isEmpty()) {
    show_wifi_status_panel();
    return;
  }
  if (provisioning_active) {
    stop_wifi_provisioning();
  }
  set_wifi_setup_visible(true, "Wi-Fi 连接中", saved_wifi_ssid().c_str(), "正在重连已记忆网络");
  connect_saved_wifi();
  show_wifi_status_panel();
}

void wifi_reprovision_event(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  start_wifi_provisioning();
}

void build_wifi_setup_panel() {
  wifi_setup_panel = lv_obj_create(root);
  lv_obj_set_size(wifi_setup_panel, 640, 300);
  lv_obj_align(wifi_setup_panel, LV_ALIGN_CENTER, 0, 16);
  make_static_touch_obj(wifi_setup_panel);
  lv_obj_clear_flag(wifi_setup_panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(wifi_setup_panel, 8, 0);
  lv_obj_set_style_bg_color(wifi_setup_panel, lv_color_hex(0x171512), 0);
  lv_obj_set_style_bg_opa(wifi_setup_panel, LV_OPA_90, 0);
  lv_obj_set_style_border_width(wifi_setup_panel, 0, 0);
  lv_obj_set_style_pad_all(wifi_setup_panel, 22, 0);

  wifi_setup_title = lv_label_create(wifi_setup_panel);
  set_cjk_title_font(wifi_setup_title);
  lv_obj_set_style_text_color(wifi_setup_title, lv_color_hex(0xfffdfa), 0);
  lv_obj_set_width(wifi_setup_title, 500);
  lv_label_set_long_mode(wifi_setup_title, LV_LABEL_LONG_WRAP);
  lv_obj_align(wifi_setup_title, LV_ALIGN_TOP_LEFT, 0, 0);

  wifi_setup_close_btn = lv_obj_create(wifi_setup_panel);
  lv_obj_set_size(wifi_setup_close_btn, 54, 54);
  lv_obj_align(wifi_setup_close_btn, LV_ALIGN_TOP_RIGHT, 0, -4);
  make_static_touch_obj(wifi_setup_close_btn);
  lv_obj_add_flag(wifi_setup_close_btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(wifi_setup_close_btn, 27, 0);
  lv_obj_set_style_bg_color(wifi_setup_close_btn, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_bg_opa(wifi_setup_close_btn, LV_OPA_20, 0);
  lv_obj_set_style_border_width(wifi_setup_close_btn, 0, 0);
  lv_obj_add_event_cb(wifi_setup_close_btn, wifi_setup_close_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *close_label = lv_label_create(wifi_setup_close_btn);
  lv_label_set_text(close_label, "X");
  lv_obj_set_style_text_font(close_label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(close_label, lv_color_hex(0xfffdfa), 0);
  lv_obj_center(close_label);

  wifi_setup_body = lv_label_create(wifi_setup_panel);
  set_cjk_font(wifi_setup_body);
  lv_obj_set_style_text_color(wifi_setup_body, lv_color_hex(0xf1c45b), 0);
  lv_obj_set_width(wifi_setup_body, 596);
  lv_label_set_long_mode(wifi_setup_body, LV_LABEL_LONG_WRAP);
  lv_obj_align(wifi_setup_body, LV_ALIGN_TOP_LEFT, 0, 56);

  wifi_setup_steps = lv_label_create(wifi_setup_panel);
  set_cjk_font(wifi_setup_steps);
  lv_obj_set_style_text_color(wifi_setup_steps, lv_color_hex(0xfffdfa), 0);
  lv_obj_set_width(wifi_setup_steps, 596);
  lv_label_set_long_mode(wifi_setup_steps, LV_LABEL_LONG_WRAP);
  lv_obj_align(wifi_setup_steps, LV_ALIGN_TOP_LEFT, 0, 112);

  wifi_setup_hint = lv_label_create(wifi_setup_panel);
  set_cjk_font(wifi_setup_hint);
  lv_obj_set_style_text_color(wifi_setup_hint, lv_color_hex(0xd8d0c3), 0);
  lv_obj_set_width(wifi_setup_hint, 596);
  lv_label_set_long_mode(wifi_setup_hint, LV_LABEL_LONG_WRAP);
  lv_obj_align(wifi_setup_hint, LV_ALIGN_TOP_LEFT, 0, 210);

  wifi_disconnect_btn = lv_obj_create(wifi_setup_panel);
  lv_obj_set_size(wifi_disconnect_btn, 176, 48);
  lv_obj_align(wifi_disconnect_btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  make_static_touch_obj(wifi_disconnect_btn);
  lv_obj_add_flag(wifi_disconnect_btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(wifi_disconnect_btn, 8, 0);
  lv_obj_set_style_bg_color(wifi_disconnect_btn, lv_color_hex(0x5f5a52), 0);
  lv_obj_set_style_border_width(wifi_disconnect_btn, 0, 0);
  lv_obj_add_event_cb(wifi_disconnect_btn, wifi_disconnect_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *disconnect_label = lv_label_create(wifi_disconnect_btn);
  lv_label_set_text(disconnect_label, "断开网络");
  set_cjk_font(disconnect_label);
  lv_obj_set_style_text_color(disconnect_label, lv_color_hex(0xfffdfa), 0);
  lv_obj_center(disconnect_label);

  wifi_reconnect_btn = lv_obj_create(wifi_setup_panel);
  lv_obj_set_size(wifi_reconnect_btn, 176, 48);
  lv_obj_align(wifi_reconnect_btn, LV_ALIGN_BOTTOM_MID, 0, 0);
  make_static_touch_obj(wifi_reconnect_btn);
  lv_obj_add_flag(wifi_reconnect_btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(wifi_reconnect_btn, 8, 0);
  lv_obj_set_style_bg_color(wifi_reconnect_btn, lv_color_hex(0x6b8f71), 0);
  lv_obj_set_style_border_width(wifi_reconnect_btn, 0, 0);
  lv_obj_add_event_cb(wifi_reconnect_btn, wifi_reconnect_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *reconnect_label = lv_label_create(wifi_reconnect_btn);
  lv_label_set_text(reconnect_label, "重连");
  set_cjk_font(reconnect_label);
  lv_obj_set_style_text_color(reconnect_label, lv_color_hex(0xfffdfa), 0);
  lv_obj_center(reconnect_label);

  wifi_reprovision_btn = lv_obj_create(wifi_setup_panel);
  lv_obj_set_size(wifi_reprovision_btn, 176, 48);
  lv_obj_align(wifi_reprovision_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  make_static_touch_obj(wifi_reprovision_btn);
  lv_obj_add_flag(wifi_reprovision_btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(wifi_reprovision_btn, 8, 0);
  lv_obj_set_style_bg_color(wifi_reprovision_btn, lv_color_hex(0xf1c45b), 0);
  lv_obj_set_style_border_width(wifi_reprovision_btn, 0, 0);
  lv_obj_add_event_cb(wifi_reprovision_btn, wifi_reprovision_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *reprovision_label = lv_label_create(wifi_reprovision_btn);
  lv_label_set_text(reprovision_label, "重新联网");
  set_cjk_font(reprovision_label);
  lv_obj_set_style_text_color(reprovision_label, lv_color_hex(0x171512), 0);
  lv_obj_center(reprovision_label);

  set_wifi_setup_visible(false, "", "", "");
}

String html_escape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    if (c == '&') escaped += F("&amp;");
    else if (c == '<') escaped += F("&lt;");
    else if (c == '>') escaped += F("&gt;");
    else if (c == '"') escaped += F("&quot;");
    else escaped += c;
  }
  return escaped;
}

String html_attr_escape(const String &value) {
  String escaped = html_escape(value);
  escaped.replace("'", "&#39;");
  return escaped;
}

void refresh_wifi_scan_options(bool force = false) {
  if (!force && scanned_wifi_options.length() && millis() - last_wifi_scan_ms < 30000) {
    return;
  }

  LOG_SERIAL.println("Scanning WiFi networks for provisioning page");
  int count = WiFi.scanNetworks(false, true);
  last_wifi_scan_ms = millis();
  scanned_wifi_options = "";

  if (count <= 0) {
    scanned_wifi_options = F("<option value=''>未扫描到 Wi-Fi，请手动输入</option>");
    return;
  }

  String seen = "|";
  for (int i = 0; i < count; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) continue;
    String marker = "|" + ssid + "|";
    if (seen.indexOf(marker) >= 0) continue;
    seen += ssid + "|";

    scanned_wifi_options += F("<option value='");
    scanned_wifi_options += html_attr_escape(ssid);
    scanned_wifi_options += F("'>");
    scanned_wifi_options += html_escape(ssid);
    scanned_wifi_options += F("（2.4G）");
    if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
      scanned_wifi_options += F(" · 无密码");
    }
    scanned_wifi_options += F("</option>");
  }

  if (!scanned_wifi_options.length()) {
    scanned_wifi_options = F("<option value=''>未扫描到可见 Wi-Fi，请手动输入</option>");
  }
}

String saved_wifi_ssid() {
  wifi_preferences.begin("lifetodo", false);
  String ssid = wifi_preferences.isKey("wifi_ssid") ? wifi_preferences.getString("wifi_ssid") : String(WIFI_SSID);
  wifi_preferences.end();
  return ssid;
}

String saved_wifi_password() {
  wifi_preferences.begin("lifetodo", false);
  String password = wifi_preferences.isKey("wifi_pass") ? wifi_preferences.getString("wifi_pass") : String(WIFI_PASSWORD);
  wifi_preferences.end();
  return password;
}

void save_wifi_credentials(const String &ssid, const String &password) {
  wifi_preferences.begin("lifetodo", false);
  wifi_preferences.putString("wifi_ssid", ssid);
  wifi_preferences.putString("wifi_pass", password);
  wifi_preferences.end();
}

void send_wifi_setup_page() {
  if (wifi_server.hasArg("rescan")) {
    refresh_wifi_scan_options(true);
  } else {
    refresh_wifi_scan_options(false);
  }
  String current_ssid = saved_wifi_ssid();
  String page =
      F("<!doctype html><html lang='zh-CN'><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>LifeTodo Wi-Fi</title><style>"
        "body{margin:0;font-family:-apple-system,BlinkMacSystemFont,'PingFang SC',sans-serif;background:#fffaf0;color:#171512}"
        "main{max-width:520px;margin:0 auto;padding:28px 20px}"
        "h1{font-size:30px;margin:8px 0 10px}.muted{color:#6e675d;line-height:1.55}"
        "form{margin-top:24px}label{display:block;margin:16px 0 8px;font-weight:700}"
        "input,select{box-sizing:border-box;width:100%;height:52px;border:2px solid #d8d0c3;border-radius:8px;padding:0 14px;font-size:18px;background:#fffdfa;color:#171512}"
        "button{width:100%;height:54px;margin-top:24px;border:0;border-radius:8px;background:#171512;color:#fffdfa;font-size:18px;font-weight:700}"
        ".chip,.link{display:inline-flex;align-items:center;gap:8px;margin-top:14px;padding:8px 12px;border-radius:8px;background:#efe7d7;color:#171512;text-decoration:none;border:0;font:inherit}"
        ".hint{font-size:14px;color:#6e675d;margin-top:8px}"
        ".manual{display:none}.manual.show{display:block}"
        ".spinner{display:none;width:14px;height:14px;border:2px solid #c9bfae;border-top-color:#171512;border-radius:50%;animation:spin .8s linear infinite}.loading .spinner{display:inline-block}"
        "@keyframes spin{to{transform:rotate(360deg)}}"
        "</style></head><body><main><h1>LifeTodo 配网</h1>"
        "<p class='muted'>选择家里的 Wi-Fi，输入密码后提交。设备会自动保存并连接，成功后屏幕回到今日任务。</p>");
  page += F("<span class='chip'>设备热点 ");
  page += html_escape(provisioning_ap_ssid);
  page += F("</span> <button class='link' id='rescan' type='button'><span class='spinner'></span><span id='rescanText'>重新扫描</span></button>"
            "<form method='post' action='/api/wifi'>"
            "<label>选择 Wi-Fi</label><select id='ssidSelect' name='ssid_select'>");
  page += scanned_wifi_options;
  page += F("<option value=''>手动输入其他 Wi-Fi</option></select>"
            "<div id='manualSsid' class='manual'><label>手动输入 Wi-Fi 名称</label><input name='ssid_manual' maxlength='32' placeholder='未列出时填写' value='");
  page += html_escape(current_ssid);
  page += F("'><p class='hint'>优先使用上方选择；如果选择“手动输入其他 Wi-Fi”，则使用这里填写的名称。</p></div>"
            "<label>Wi-Fi 密码</label><input name='password' type='password' maxlength='64' autocomplete='current-password'>"
            "<button type='submit'>保存并连接</button></form>"
            "<script>"
            "const select=document.getElementById('ssidSelect');"
            "const manual=document.getElementById('manualSsid');"
            "function syncManual(){manual.classList.toggle('show',select.value==='');}"
            "select.addEventListener('change',syncManual);syncManual();"
            "document.getElementById('rescan').addEventListener('click',function(){"
            "this.classList.add('loading');this.disabled=true;document.getElementById('rescanText').textContent='扫描中';"
            "setTimeout(()=>{location.href='/?rescan=1'},60);"
            "});"
            "</script></main></body></html>");
  wifi_server.send(200, "text/html; charset=utf-8", page);
}

void handle_wifi_status_api() {
  JsonDocument doc;
  doc["connected"] = WiFi.status() == WL_CONNECTED;
  doc["provisioning"] = provisioning_active;
  doc["apSsid"] = provisioning_ap_ssid;
  doc["ip"] = WiFi.localIP().toString();
  String payload;
  serializeJson(doc, payload);
  wifi_server.send(200, "application/json", payload);
}

void handle_wifi_submit() {
  String ssid = wifi_server.arg("ssid_select");
  if (ssid.isEmpty()) {
    ssid = wifi_server.arg("ssid_manual");
  }
  String password = wifi_server.arg("password");
  ssid.trim();
  if (ssid.isEmpty()) {
    wifi_server.send(400, "text/plain; charset=utf-8", "缺少 Wi-Fi 名称");
    return;
  }

  save_wifi_credentials(ssid, password);
  pending_wifi_ssid = ssid;
  pending_wifi_password = password;
  wifi_connect_pending = true;

  wifi_server.send(
      200,
      "text/html; charset=utf-8",
      F("<!doctype html><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<body style=\"font-family:-apple-system,BlinkMacSystemFont,'PingFang SC',sans-serif;background:#fffaf0;color:#171512;padding:28px\">"
        "<h1>已保存</h1><p>设备正在连接 Wi-Fi。成功后屏幕会自动更新。</p></body>"));
  set_wifi_setup_visible(true, "Wi-Fi 连接中", ssid.c_str(), "LifeTodo 192.168.4.1");
}

void configure_wifi_server_routes() {
  wifi_server.on("/", HTTP_GET, send_wifi_setup_page);
  wifi_server.on("/generate_204", HTTP_GET, send_wifi_setup_page);
  wifi_server.on("/gen_204", HTTP_GET, send_wifi_setup_page);
  wifi_server.on("/hotspot-detect.html", HTTP_GET, send_wifi_setup_page);
  wifi_server.on("/library/test/success.html", HTTP_GET, send_wifi_setup_page);
  wifi_server.on("/ncsi.txt", HTTP_GET, send_wifi_setup_page);
  wifi_server.on("/connecttest.txt", HTTP_GET, send_wifi_setup_page);
  wifi_server.on("/redirect", HTTP_GET, send_wifi_setup_page);
  wifi_server.on("/api/wifi", HTTP_GET, handle_wifi_status_api);
  wifi_server.on("/api/wifi", HTTP_POST, handle_wifi_submit);
  wifi_server.onNotFound([]() {
    send_wifi_setup_page();
  });
}

String device_suffix() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  if (mac.length() > 6) {
    return mac.substring(mac.length() - 6);
  }
  return String("SETUP");
}

void start_wifi_provisioning() {
  if (provisioning_active) {
    set_wifi_setup_visible(true, "设备联网", provisioning_ap_ssid.c_str(), "手机连接设备后访问 192.168.4.1 为设备联网");
    return;
  }

  provisioning_ap_ssid = String("LifeTodo-") + device_suffix();
  WiFi.mode(WIFI_AP_STA);
  IPAddress ap_ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(ap_ip, gateway, subnet);
  bool ap_ok = WiFi.softAP(provisioning_ap_ssid.c_str());
  ap_ip = WiFi.softAPIP();
  dns_server.start(DNS_PORT, "*", ap_ip);
  configure_wifi_server_routes();
  wifi_server.begin();
  provisioning_active = true;
  refresh_wifi_scan_options(true);

  LOG_SERIAL.printf("WiFi provisioning %s ap=%s ip=%s\n",
                    ap_ok ? "started" : "failed",
                    provisioning_ap_ssid.c_str(),
                    ap_ip.toString().c_str());
  set_wifi_setup_visible(true, "设备联网", provisioning_ap_ssid.c_str(), "手机连接设备后访问 192.168.4.1 为设备联网");
  refresh_wifi_state_labels();
}

void stop_wifi_provisioning() {
  if (!provisioning_active) return;
  wifi_server.stop();
  dns_server.stop();
  WiFi.softAPdisconnect(true);
  provisioning_active = false;
  set_wifi_setup_visible(false, "", "", "");
  refresh_wifi_state_labels();
  LOG_SERIAL.println("WiFi provisioning stopped");
}

bool connect_wifi_with_credentials(const String &ssid, const String &password) {
  if (ssid.isEmpty()) return false;

  lv_label_set_text(status_label, "Wi-Fi 连接中");
  set_wifi_setup_visible(provisioning_active, "Wi-Fi 连接中", ssid.c_str(), "LifeTodo 192.168.4.1");
  LOG_SERIAL.printf("Connecting WiFi ssid=%s\n", ssid.c_str());

  WiFi.mode(provisioning_active ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  wifi_connect_started_ms = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifi_connect_started_ms < WIFI_CONNECT_TIMEOUT_MS) {
    handle_wifi_services();
    lv_timer_handler();
    delay(50);
  }

  if (WiFi.status() != WL_CONNECTED) {
    LOG_SERIAL.printf("WiFi connect failed status=%d\n", WiFi.status());
    refresh_wifi_state_labels();
    return false;
  }

  LOG_SERIAL.printf("WiFi connected ssid=%s ip=%s gateway=%s dns=%s rssi=%d\n",
                    WiFi.SSID().c_str(),
                    WiFi.localIP().toString().c_str(),
                    WiFi.gatewayIP().toString().c_str(),
                    WiFi.dnsIP().toString().c_str(),
                    WiFi.RSSI());
  stop_wifi_provisioning();
  configTzTime("CST-8", "pool.ntp.org", "time.nist.gov");
  update_today_key();
  refresh_wifi_state_labels();
  delay(1500);
  sync_from_cloud();
  return true;
}

bool connect_saved_wifi() {
  String ssid = pending_wifi_ssid.length() ? pending_wifi_ssid : saved_wifi_ssid();
  String password = pending_wifi_ssid.length() ? pending_wifi_password : saved_wifi_password();
  pending_wifi_ssid = "";
  pending_wifi_password = "";
  return connect_wifi_with_credentials(ssid, password);
}

void handle_wifi_services() {
  if (!provisioning_active) return;
  dns_server.processNextRequest();
  wifi_server.handleClient();
}

void connect_wifi() {
  if (saved_wifi_ssid().isEmpty()) {
    start_wifi_provisioning();
    return;
  }

  if (!connect_saved_wifi()) {
    start_wifi_provisioning();
  }
}

void build_ui() {
  root = lv_obj_create(nullptr);
  lv_scr_load(root);
  disable_scroll(root);
  lv_obj_set_style_bg_color(root, lv_color_hex(0xfffaf0), 0);
  lv_obj_set_style_pad_all(root, 20, 0);

  lv_obj_t *title = lv_label_create(root);
  lv_label_set_text(title, "LifeTodo 今日");
  lv_obj_set_style_text_color(title, lv_color_hex(0x171512), 0);
  set_cjk_title_font(title);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 2);

  status_label = lv_label_create(root);
  lv_label_set_text(status_label, "启动中");
  lv_obj_set_style_text_color(status_label, lv_color_hex(0x6e675d), 0);
  set_cjk_font(status_label);
  lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, -8, 10);

  summary_label = lv_label_create(root);
  lv_obj_set_style_text_color(summary_label, lv_color_hex(0x6e675d), 0);
  set_cjk_font(summary_label);
  lv_obj_align(summary_label, LV_ALIGN_TOP_LEFT, 10, 78);
  update_summary();

  build_task_grid();
  build_brightness_controls();
  build_wifi_setup_panel();

  lv_obj_t *footer = lv_label_create(root);
  lv_label_set_text(footer, "LifeTodo device  lifetodo.xyz");
  lv_obj_set_style_text_color(footer, lv_color_hex(0x6e675d), 0);
  lv_obj_set_style_text_font(footer, &lv_font_montserrat_18, 0);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_LEFT, 8, -4);
}
}  // namespace

void setup() {
  LOG_SERIAL.begin(115200);
  delay(1200);
  LOG_SERIAL.println("LifeTodo ESP32 display starting");
  LOG_SERIAL.printf("Heap=%u PSRAM=%u\n", ESP.getFreeHeap(), ESP.getFreePsram());

  init_expander_outputs();

  LOG_SERIAL.println("Display init begin");
  if (!display->begin()) {
    LOG_SERIAL.println("Display init failed");
    while (true) delay(1000);
  }
  LOG_SERIAL.println("Display init ok");
  display->fillScreen(RED);
  delay(180);
  display->fillScreen(GREEN);
  delay(180);
  display->fillScreen(BLUE);
  delay(180);
  display->fillScreen(BLACK);

  LOG_SERIAL.println("Touch init begin");
  touch.begin();
  // Waveshare's official ESP32-S3-Touch-LCD-4.3 touch config uses no swap
  // and no X/Y mirror. In TAMC_GT911 this corresponds to ROTATION_INVERTED.
  touch.setRotation(ROTATION_INVERTED);
  LOG_SERIAL.println("Touch init ok");

  LOG_SERIAL.println("LVGL init begin");
  lv_init();
  buf1 = static_cast<lv_color_t *>(ps_malloc(SCREEN_W * 40 * sizeof(lv_color_t)));
  buf2 = static_cast<lv_color_t *>(ps_malloc(SCREEN_W * 40 * sizeof(lv_color_t)));
  if (!buf1 || !buf2) {
    LOG_SERIAL.println("LVGL draw buffer allocation failed");
    while (true) delay(1000);
  }

  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, SCREEN_W * 40);
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREEN_W;
  disp_drv.ver_res = SCREEN_H;
  disp_drv.flush_cb = flush_display;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = read_touch;
  lv_indev_drv_register(&indev_drv);

  build_ui();
  connect_wifi();
  LOG_SERIAL.println("LifeTodo ESP32 display ready");
}

void loop() {
  lv_timer_handler();
  handle_wifi_services();
  int wifi_status = WiFi.status();
  if (wifi_status != last_wifi_status_code) {
    last_wifi_status_code = wifi_status;
    refresh_wifi_state_labels();
  }
  if (wifi_connect_pending) {
    wifi_connect_pending = false;
    connect_saved_wifi();
  }
  process_pending_completion_sync();
  process_sync_failure_report();
  if (millis() - last_heartbeat_ms > 5000) {
    LOG_SERIAL.printf("Heartbeat heap=%u psram=%u wifi=%d remaining=%u\n",
                      ESP.getFreeHeap(), ESP.getFreePsram(), WiFi.status(), remaining_count());
    last_heartbeat_ms = millis();
  }
  if (pending_completion_count == 0 && !provisioning_active && WiFi.status() == WL_CONNECTED && millis() >= next_cloud_sync_ms) {
    last_cloud_sync_ms = millis();
    sync_from_cloud();
  }
  delay(5);
}
