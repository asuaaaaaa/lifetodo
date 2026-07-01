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
#define LIFETODO_BASE_URL "https://lifetodo.xyz"
#endif

#ifndef LIFETODO_FIREBASE_PROJECT_ID
#define LIFETODO_FIREBASE_PROJECT_ID "lifetodo-47399"
#endif

#ifndef LIFETODO_FIREBASE_API_KEY
#define LIFETODO_FIREBASE_API_KEY "AIzaSyAF_WBUeMoqmyIY4YzepdF7WFHjgYy2Rms"
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
constexpr uint32_t CLOUD_SYNC_INTERVAL_MS = 60000;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 12000;
constexpr uint16_t DNS_PORT = 53;
constexpr size_t MAX_TASKS = 4;
constexpr size_t MAX_COMPLETIONS = 40;
uint8_t ch422g_output = 0xff;

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
lv_obj_t *wifi_setup_panel;
lv_obj_t *wifi_setup_title;
lv_obj_t *wifi_setup_body;
lv_obj_t *wifi_setup_hint;
uint32_t last_heartbeat_ms = 0;
uint32_t last_touch_log_ms = 0;
uint32_t last_cloud_sync_ms = 0;
uint32_t wifi_connect_started_ms = 0;
bool brightness_panel_open = false;
bool provisioning_active = false;
bool wifi_connect_pending = false;
uint8_t brightness_percent = 100;
bool cloud_ready = false;
char today_key[11] = "2026-07-01";
String pending_wifi_ssid;
String pending_wifi_password;
String provisioning_ap_ssid;

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
  lv_obj_t *member = nullptr;
  lv_obj_t *title = nullptr;
  lv_obj_t *status = nullptr;
};

Task tasks[] = {
    {"litter", "妈妈", "铲猫砂盆", "每 3 天", false, 0xef7f65},
    {"toilet", "爸爸", "清洁马桶", "每月", false, 0x5d8fb4},
    {"plants", "小朋友", "给阳台植物浇水", "每 2 天", false, 0x6ba36f},
    {"sheets", "全家", "换床单", "每周", true, 0xf1c45b},
};
constexpr size_t TASK_COUNT = MAX_TASKS;
TaskView task_views[TASK_COUNT];
size_t task_count = 4;
char completion_keys[MAX_COMPLETIONS][64];
size_t completion_count = 0;
bool task_click_armed = true;
bool touch_down = false;
uint32_t touch_release_started_ms = 0;

void task_click(lv_event_t *event);
void set_brightness_panel_open(bool open);
void update_brightness_visuals();
void disable_scroll(lv_obj_t *obj);
void make_static_touch_obj(lv_obj_t *obj);
void apply_task_view(size_t index);
void update_summary();
void render_tasks();
bool sync_from_cloud();
bool push_completions_to_cloud();
void set_wifi_setup_visible(bool visible, const char *title, const char *body, const char *hint);
void start_wifi_provisioning();
void stop_wifi_provisioning();
bool connect_saved_wifi();
void handle_wifi_services();

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

String firestore_document_url() {
  return String("https://firestore.googleapis.com/v1/projects/") +
         LIFETODO_FIREBASE_PROJECT_ID +
         "/databases/(default)/documents/homes/" +
         LIFETODO_HOME_ID +
         "?key=" +
         LIFETODO_FIREBASE_API_KEY;
}

bool update_today_key() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 200)) {
    return false;
  }
  strftime(today_key, sizeof(today_key), "%Y-%m-%d", &timeinfo);
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
  if (task_fields["enabled"]["booleanValue"].is<bool>() &&
      !task_fields["enabled"]["booleanValue"].as<bool>()) {
    return false;
  }

  JsonObject recurrence = task_fields["recurrence"]["mapValue"]["fields"].as<JsonObject>();
  const char *type = recurrence["type"]["stringValue"] | "";
  time_t today = parse_date(today_key);
  if (today == 0) return true;

  if (strcmp(type, "intervalDays") == 0) {
    int every = recurrence["every"]["integerValue"] | 1;
    const char *anchor = recurrence["anchorDate"]["stringValue"] | today_key;
    time_t anchor_time = parse_date(anchor);
    if (anchor_time == 0 || every <= 0) return false;
    long diff = static_cast<long>((today - anchor_time) / 86400);
    return diff >= 0 && diff % every == 0;
  }

  struct tm today_tm = {};
  localtime_r(&today, &today_tm);
  if (strcmp(type, "monthlyDate") == 0) {
    int day = recurrence["day"]["integerValue"] | 1;
    return today_tm.tm_mday == day;
  }

  if (strcmp(type, "weekly") == 0) {
    JsonArray days = recurrence["daysOfWeek"]["arrayValue"]["values"].as<JsonArray>();
    for (JsonVariant day : days) {
      if ((day["integerValue"] | -1) == today_tm.tm_wday) return true;
    }
    return false;
  }

  return true;
}

const char *member_name_for(const char *member_id, JsonArray members, const char *fallback) {
  for (JsonVariant member : members) {
    JsonObject fields = member["mapValue"]["fields"].as<JsonObject>();
    const char *id = fields["id"]["stringValue"] | "";
    if (strcmp(id, member_id) == 0) {
      return fields["name"]["stringValue"] | fallback;
    }
  }
  return fallback;
}

uint32_t member_color_for(const char *member_id, JsonArray members, uint32_t fallback) {
  for (JsonVariant member : members) {
    JsonObject fields = member["mapValue"]["fields"].as<JsonObject>();
    const char *id = fields["id"]["stringValue"] | "";
    if (strcmp(id, member_id) == 0) {
      return parse_color(fields["color"]["stringValue"] | "", fallback);
    }
  }
  return fallback;
}

void read_completion_keys(JsonObject fields) {
  completion_count = 0;
  JsonObject completions = fields["completions"]["mapValue"]["fields"].as<JsonObject>();
  for (JsonPair item : completions) {
    if (completion_count >= MAX_COMPLETIONS) break;
    copy_text(completion_keys[completion_count], sizeof(completion_keys[completion_count]), item.key().c_str());
    completion_count++;
  }
}

bool apply_cloud_document(const String &payload) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    LOG_SERIAL.printf("Firestore JSON parse failed: %s\n", err.c_str());
    return false;
  }

  JsonObject fields = doc["fields"].as<JsonObject>();
  if (fields.isNull()) return false;

  JsonArray members = fields["members"]["arrayValue"]["values"].as<JsonArray>();
  JsonArray cloud_tasks = fields["tasks"]["arrayValue"]["values"].as<JsonArray>();
  read_completion_keys(fields);

  size_t next_count = 0;
  for (JsonVariant item : cloud_tasks) {
    if (next_count >= TASK_COUNT) break;
    JsonObject task_fields = item["mapValue"]["fields"].as<JsonObject>();
    if (task_fields.isNull() || !is_due_today(task_fields)) continue;

    Task &task = tasks[next_count];
    const char *id = task_fields["id"]["stringValue"] | "";
    const char *title = task_fields["title"]["stringValue"] | "事项";
    const char *label = task_fields["label"]["stringValue"] | "";
    const char *assignee_id = task_fields["assigneeId"]["stringValue"] | "";
    String done_key = task_completion_key(id);

    copy_text(task.id, sizeof(task.id), id);
    copy_text(task.title, sizeof(task.title), title);
    copy_text(task.label, sizeof(task.label), label);
    copy_text(task.member, sizeof(task.member), member_name_for(assignee_id, members, "成员"));
    task.color = member_color_for(assignee_id, members, 0xef7f65);
    task.done = completion_key_exists(done_key.c_str());
    next_count++;
  }

  task_count = next_count;
  cloud_ready = true;
  LOG_SERIAL.printf("Cloud sync ok home=%s today=%s tasks=%u completions=%u\n",
                    LIFETODO_HOME_ID, today_key, static_cast<unsigned>(task_count),
                    static_cast<unsigned>(completion_count));
  return true;
}

bool sync_from_cloud() {
  if (WiFi.status() != WL_CONNECTED) return false;
  update_today_key();

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = firestore_document_url();
  if (!http.begin(client, url)) {
    LOG_SERIAL.println("Firestore HTTP begin failed");
    return false;
  }

  int code = http.GET();
  String payload = http.getString();
  http.end();
  if (code != 200) {
    LOG_SERIAL.printf("Firestore GET failed code=%d payload=%s\n", code, payload.c_str());
    return false;
  }

  bool ok = apply_cloud_document(payload);
  if (ok) {
    update_summary();
    render_tasks();
    lv_label_set_text(status_label, "Firebase 已连接");
  }
  return ok;
}

String completion_value_json(const char *key) {
  String key_string(key);
  int separator = key_string.lastIndexOf('_');
  String task_id = separator > 0 ? key_string.substring(0, separator) : key_string;
  String date = separator > 0 ? key_string.substring(separator + 1) : String(today_key);
  String completed_at = date + "T00:00:00.000Z";

  JsonDocument value;
  JsonObject fields = value["mapValue"]["fields"].to<JsonObject>();
  fields["taskId"]["stringValue"] = task_id;
  fields["date"]["stringValue"] = date;
  fields["source"]["stringValue"] = "device-esp32";
  fields["completedAt"]["stringValue"] = completed_at;

  String output;
  serializeJson(value, output);
  return output;
}

bool push_completions_to_cloud() {
  if (WiFi.status() != WL_CONNECTED) return false;

  JsonDocument body;
  JsonObject completion_fields = body["fields"]["completions"]["mapValue"]["fields"].to<JsonObject>();
  for (size_t i = 0; i < completion_count; i++) {
    JsonDocument completion;
    deserializeJson(completion, completion_value_json(completion_keys[i]));
    completion_fields[completion_keys[i]] = completion.as<JsonVariant>();
  }

  String payload;
  serializeJson(body, payload);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = firestore_document_url() + "&updateMask.fieldPaths=completions";
  if (!http.begin(client, url)) {
    LOG_SERIAL.println("Firestore PATCH begin failed");
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  int code = http.PATCH(payload);
  String response = http.getString();
  http.end();

  if (code < 200 || code >= 300) {
    LOG_SERIAL.printf("Firestore PATCH failed code=%d payload=%s\n", code, response.c_str());
    return false;
  }
  LOG_SERIAL.printf("Firestore completions updated count=%u\n", static_cast<unsigned>(completion_count));
  return true;
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
    touch_down = true;
    touch_release_started_ms = 0;
    data->state = LV_INDEV_STATE_PR;
    data->point.x = touch.points[0].x;
    data->point.y = touch.points[0].y;
    if (millis() - last_touch_log_ms > 250) {
      LOG_SERIAL.printf("Touch x=%d y=%d\n", data->point.x, data->point.y);
      last_touch_log_ms = millis();
    }
  } else {
    if (touch_down) {
      touch_down = false;
      touch_release_started_ms = millis();
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
  snprintf(buffer, sizeof(buffer), "%u 件待完成", remaining_count());
  lv_label_set_text(summary_label, buffer);
}

void style_panel(lv_obj_t *obj, uint32_t color, bool done) {
  lv_color_t bg = lv_color_hex(done ? 0xe9e4db : 0xfffdfa);
  lv_color_t border = lv_color_hex(done ? 0xc7c0b5 : color);
  lv_opa_t bg_opa = done ? LV_OPA_70 : LV_OPA_COVER;
  lv_coord_t border_width = 4;
  lv_coord_t shadow_width = done ? 0 : 8;
  lv_opa_t shadow_opa = done ? LV_OPA_TRANSP : LV_OPA_20;

  lv_obj_set_style_radius(obj, 8, 0);
  lv_obj_set_style_bg_color(obj, bg, 0);
  lv_obj_set_style_bg_opa(obj, bg_opa, 0);
  lv_obj_set_style_border_color(obj, border, 0);
  lv_obj_set_style_border_width(obj, border_width, 0);
  lv_obj_set_style_pad_all(obj, 14, 0);
  lv_obj_set_style_shadow_width(obj, shadow_width, 0);
  lv_obj_set_style_shadow_color(obj, lv_color_hex(color), 0);
  lv_obj_set_style_shadow_opa(obj, shadow_opa, 0);

  lv_obj_set_style_bg_color(obj, bg, LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(obj, bg_opa, LV_STATE_PRESSED);
  lv_obj_set_style_border_color(obj, border, LV_STATE_PRESSED);
  lv_obj_set_style_border_width(obj, border_width, LV_STATE_PRESSED);
  lv_obj_set_style_shadow_width(obj, shadow_width, LV_STATE_PRESSED);
  lv_obj_set_style_shadow_opa(obj, shadow_opa, LV_STATE_PRESSED);
}

void apply_task_view(size_t index) {
  if (index >= TASK_COUNT) return;

  Task &item = tasks[index];
  TaskView &view = task_views[index];
  if (!view.card || !view.member || !view.title || !view.status) return;

  style_panel(view.card, item.color, item.done);
  lv_obj_set_style_text_color(view.member, lv_color_hex(item.done ? 0x8d857b : item.color), 0);
  lv_obj_set_style_text_color(view.title, lv_color_hex(item.done ? 0x8b8378 : 0x171512), 0);
  lv_obj_set_style_text_color(view.status, lv_color_hex(item.done ? 0x2f7d4f : item.color), 0);
  lv_label_set_text(view.status, item.done ? "已完成" : "待做");
  lv_obj_invalidate(view.card);
}

void render_tasks() {
  lv_obj_clean(task_grid);
  if (task_count == 0) {
    lv_obj_t *empty = lv_label_create(task_grid);
    lv_label_set_text(empty, cloud_ready ? "今天没有安排" : "正在同步");
    set_cjk_title_font(empty);
    lv_obj_set_style_text_color(empty, lv_color_hex(0x6e675d), 0);
    lv_obj_set_width(empty, 760);
    lv_obj_align(empty, LV_ALIGN_TOP_LEFT, 0, 0);
    return;
  }

  for (size_t i = 0; i < task_count; i++) {
    Task &item = tasks[i];
    lv_obj_t *btn = lv_obj_create(task_grid);
    lv_obj_set_size(btn, 360, 108);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    make_static_touch_obj(btn);
    lv_obj_add_event_cb(btn, task_click, LV_EVENT_PRESSED, &item);
    task_views[i].card = btn;

    lv_obj_t *member = lv_label_create(btn);
    lv_label_set_text(member, item.member);
    lv_obj_set_width(member, 130);
    set_cjk_font(member);
    lv_obj_align(member, LV_ALIGN_TOP_LEFT, 0, 0);
    task_views[i].member = member;

    lv_obj_t *title = lv_label_create(btn);
    lv_label_set_text(title, item.title);
    lv_obj_set_width(title, 248);
    set_cjk_title_font(title);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 14);
    task_views[i].title = title;

    lv_obj_t *done = lv_label_create(btn);
    lv_obj_set_width(done, 108);
    set_cjk_font(done);
    lv_obj_align(done, LV_ALIGN_RIGHT_MID, -4, 0);
    task_views[i].status = done;

    apply_task_view(i);
  }
}

void task_click(lv_event_t *event) {
  Task *task = static_cast<Task *>(lv_event_get_user_data(event));
  if (!task_click_armed) return;

  task->done = !task->done;
  String key = task_completion_key(task->id);
  set_completion_key(key.c_str(), task->done);
  task_click_armed = false;
  LOG_SERIAL.printf("Task toggled id=%s done=%s\n", task->id, task->done ? "true" : "false");
  update_summary();

  for (size_t i = 0; i < TASK_COUNT; i++) {
    if (&tasks[i] == task) {
      apply_task_view(i);
      break;
    }
  }

  push_completions_to_cloud();
}

void build_task_grid() {
  task_grid = lv_obj_create(root);
  lv_obj_set_size(task_grid, 760, 272);
  lv_obj_align(task_grid, LV_ALIGN_TOP_MID, 0, 138);
  disable_scroll(task_grid);
  lv_obj_set_style_bg_opa(task_grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(task_grid, 0, 0);
  lv_obj_set_flex_flow(task_grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(task_grid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_all(task_grid, 0, 0);
  lv_obj_set_style_pad_row(task_grid, 12, 0);
  lv_obj_set_style_pad_column(task_grid, 16, 0);

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

void set_brightness_panel_open(bool open) {
  brightness_panel_open = open;
  lv_obj_set_y(brightness_panel, open ? 10 : -112);
  lv_obj_clear_flag(brightness_panel, LV_OBJ_FLAG_HIDDEN);
  if (open) {
    lv_obj_add_flag(brightness_overlay, LV_OBJ_FLAG_CLICKABLE);
  } else {
    lv_obj_clear_flag(brightness_overlay, LV_OBJ_FLAG_CLICKABLE);
  }
  lv_obj_move_foreground(brightness_overlay);
  lv_obj_move_foreground(brightness_panel);
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
  lv_obj_set_size(brightness_panel, 760, 104);
  lv_obj_align(brightness_panel, LV_ALIGN_TOP_MID, 0, -112);
  make_static_touch_obj(brightness_panel);
  lv_obj_set_style_radius(brightness_panel, 8, 0);
  lv_obj_set_style_bg_color(brightness_panel, lv_color_hex(0x171512), 0);
  lv_obj_set_style_bg_opa(brightness_panel, 242, 0);
  lv_obj_set_style_border_width(brightness_panel, 0, 0);
  lv_obj_set_style_pad_all(brightness_panel, 16, 0);
  lv_obj_add_event_cb(brightness_panel, brightness_gesture_event, LV_EVENT_GESTURE, nullptr);
  lv_obj_add_event_cb(brightness_panel, brightness_gesture_event, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *label = lv_label_create(brightness_panel);
  lv_label_set_text(label, "LIGHT");
  lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xfffdfa), 0);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 8, 0);

  brightness_slider = lv_slider_create(brightness_panel);
  lv_obj_set_size(brightness_slider, 500, 18);
  lv_obj_align(brightness_slider, LV_ALIGN_CENTER, 24, 2);
  make_static_touch_obj(brightness_slider);
  lv_slider_set_range(brightness_slider, 20, 100);
  lv_slider_set_value(brightness_slider, brightness_percent, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x3a3935), LV_PART_MAIN);
  lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0xf1c45b), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0xfffdfa), LV_PART_KNOB);
  lv_obj_set_style_pad_all(brightness_slider, 8, LV_PART_KNOB);
  lv_obj_add_event_cb(brightness_slider, brightness_slider_event, LV_EVENT_VALUE_CHANGED, nullptr);

  brightness_value_label = lv_label_create(brightness_panel);
  lv_obj_set_style_text_font(brightness_value_label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(brightness_value_label, lv_color_hex(0xfffdfa), 0);
  lv_obj_align(brightness_value_label, LV_ALIGN_RIGHT_MID, -10, 0);
  update_brightness_visuals();

  brightness_gesture_zone = lv_obj_create(root);
  lv_obj_set_size(brightness_gesture_zone, SCREEN_W, 66);
  lv_obj_align(brightness_gesture_zone, LV_ALIGN_TOP_MID, 0, 0);
  make_static_touch_obj(brightness_gesture_zone);
  lv_obj_add_flag(brightness_gesture_zone, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(brightness_gesture_zone, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(brightness_gesture_zone, 0, 0);
  lv_obj_set_style_pad_all(brightness_gesture_zone, 0, 0);
  lv_obj_add_event_cb(brightness_gesture_zone, brightness_gesture_event, LV_EVENT_GESTURE, nullptr);
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
  lv_label_set_text(wifi_setup_body, body ? body : "");
  lv_label_set_text(wifi_setup_hint, hint ? hint : "");
  lv_obj_clear_flag(wifi_setup_panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(wifi_setup_panel);
}

void build_wifi_setup_panel() {
  wifi_setup_panel = lv_obj_create(root);
  lv_obj_set_size(wifi_setup_panel, 560, 236);
  lv_obj_align(wifi_setup_panel, LV_ALIGN_CENTER, 0, 24);
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
  lv_obj_set_width(wifi_setup_title, 516);
  lv_label_set_long_mode(wifi_setup_title, LV_LABEL_LONG_WRAP);
  lv_obj_align(wifi_setup_title, LV_ALIGN_TOP_LEFT, 0, 0);

  wifi_setup_body = lv_label_create(wifi_setup_panel);
  lv_obj_set_style_text_font(wifi_setup_body, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(wifi_setup_body, lv_color_hex(0xf1c45b), 0);
  lv_obj_set_width(wifi_setup_body, 516);
  lv_label_set_long_mode(wifi_setup_body, LV_LABEL_LONG_WRAP);
  lv_obj_align(wifi_setup_body, LV_ALIGN_TOP_LEFT, 0, 70);

  wifi_setup_hint = lv_label_create(wifi_setup_panel);
  set_cjk_font(wifi_setup_hint);
  lv_obj_set_style_text_color(wifi_setup_hint, lv_color_hex(0xd8d0c3), 0);
  lv_obj_set_width(wifi_setup_hint, 516);
  lv_label_set_long_mode(wifi_setup_hint, LV_LABEL_LONG_WRAP);
  lv_obj_align(wifi_setup_hint, LV_ALIGN_TOP_LEFT, 0, 122);

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
  String current_ssid = saved_wifi_ssid();
  String page =
      F("<!doctype html><html lang='zh-CN'><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>LifeTodo Wi-Fi</title><style>"
        "body{margin:0;font-family:-apple-system,BlinkMacSystemFont,'PingFang SC',sans-serif;background:#fffaf0;color:#171512}"
        "main{max-width:520px;margin:0 auto;padding:28px 20px}"
        "h1{font-size:30px;margin:8px 0 10px}.muted{color:#6e675d;line-height:1.55}"
        "form{margin-top:24px}label{display:block;margin:16px 0 8px;font-weight:700}"
        "input{box-sizing:border-box;width:100%;height:52px;border:2px solid #d8d0c3;border-radius:8px;padding:0 14px;font-size:18px;background:#fffdfa}"
        "button{width:100%;height:54px;margin-top:24px;border:0;border-radius:8px;background:#171512;color:#fffdfa;font-size:18px;font-weight:700}"
        ".chip{display:inline-block;margin-top:14px;padding:8px 12px;border-radius:8px;background:#efe7d7;color:#171512}"
        "</style></head><body><main><h1>LifeTodo 配网</h1>"
        "<p class='muted'>选择家里的 Wi-Fi，提交后设备会自动保存并连接。连接成功后屏幕会回到今日任务。</p>");
  page += F("<span class='chip'>设备热点 ");
  page += html_escape(provisioning_ap_ssid);
  page += F("</span><form method='post' action='/api/wifi'>"
            "<label>Wi-Fi 名称</label><input name='ssid' required maxlength='32' value='");
  page += html_escape(current_ssid);
  page += F("'><label>Wi-Fi 密码</label><input name='password' type='password' maxlength='64'>"
            "<button type='submit'>保存并连接</button></form></main></body></html>");
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
  String ssid = wifi_server.arg("ssid");
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
  if (provisioning_active) return;

  provisioning_ap_ssid = String("LifeTodo-") + device_suffix();
  WiFi.mode(WIFI_AP_STA);
  bool ap_ok = WiFi.softAP(provisioning_ap_ssid.c_str());
  IPAddress ap_ip = WiFi.softAPIP();
  dns_server.start(DNS_PORT, "*", ap_ip);
  configure_wifi_server_routes();
  wifi_server.begin();
  provisioning_active = true;

  LOG_SERIAL.printf("WiFi provisioning %s ap=%s ip=%s\n",
                    ap_ok ? "started" : "failed",
                    provisioning_ap_ssid.c_str(),
                    ap_ip.toString().c_str());
  set_wifi_setup_visible(true, "设备接入", provisioning_ap_ssid.c_str(), "LifeTodo 192.168.4.1");
  lv_label_set_text(status_label, "Wi-Fi 未连接");
}

void stop_wifi_provisioning() {
  if (!provisioning_active) return;
  wifi_server.stop();
  dns_server.stop();
  WiFi.softAPdisconnect(true);
  provisioning_active = false;
  set_wifi_setup_visible(false, "", "", "");
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
    lv_label_set_text(status_label, "Wi-Fi 未连接");
    return false;
  }

  LOG_SERIAL.printf("WiFi connected ip=%s\n", WiFi.localIP().toString().c_str());
  stop_wifi_provisioning();
  configTzTime("CST-8", "pool.ntp.org", "time.nist.gov");
  update_today_key();
  lv_label_set_text(status_label, "Wi-Fi 已连接");
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
  if (wifi_connect_pending) {
    wifi_connect_pending = false;
    connect_saved_wifi();
  }
  if (millis() - last_heartbeat_ms > 5000) {
    LOG_SERIAL.printf("Heartbeat heap=%u psram=%u wifi=%d remaining=%u\n",
                      ESP.getFreeHeap(), ESP.getFreePsram(), WiFi.status(), remaining_count());
    last_heartbeat_ms = millis();
  }
  if (WiFi.status() == WL_CONNECTED && millis() - last_cloud_sync_ms > CLOUD_SYNC_INTERVAL_MS) {
    last_cloud_sync_ms = millis();
    sync_from_cloud();
  }
  delay(5);
}
