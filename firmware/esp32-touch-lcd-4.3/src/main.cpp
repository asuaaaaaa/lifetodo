#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>
#include <WiFi.h>
#include <lvgl.h>

#if __has_include("app_config.h")
#include "app_config.h"
#else
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define LIFETODO_HOME_ID "demo-home"
#define LIFETODO_DEVICE_ID "entry"
#define LIFETODO_BASE_URL "https://lifetodo.xyz"
#endif

namespace {
constexpr uint16_t SCREEN_W = 800;
constexpr uint16_t SCREEN_H = 480;

// Waveshare ESP32-S3-Touch-LCD-4.3 RGB and touch pins.
// Source: Waveshare wiki pinout for ESP32-S3-Touch-LCD-4.3.
Arduino_ESP32RGBPanel *rgb_bus = new Arduino_ESP32RGBPanel(
    7, 3, 46, 5,
    1, 2, 42, 41, 40,
    39, 0, 45, 48, 47, 21, 14,
    38, 18, 17, 10,
    0, 20, 10, 10,
    0, 10, 10, 10,
    0, 0, 0, 0);

Arduino_RGB_Display *display = new Arduino_RGB_Display(
    SCREEN_W, SCREEN_H, rgb_bus, 0, true);

TAMC_GT911 touch(8, 9, 4, -1, SCREEN_W, SCREEN_H);

lv_disp_draw_buf_t draw_buf;
lv_color_t *buf1;
lv_color_t *buf2;
lv_disp_drv_t disp_drv;
lv_indev_drv_t indev_drv;

lv_obj_t *root;
lv_obj_t *status_label;
lv_obj_t *summary_label;
lv_obj_t *task_grid;

struct Task {
  const char *id;
  const char *member;
  const char *title;
  bool done;
  uint32_t color;
};

Task tasks[] = {
    {"litter", "妈妈", "铲猫砂盆", false, 0xef7f65},
    {"toilet", "爸爸", "清洁马桶", false, 0x5d8fb4},
    {"plants", "小朋友", "给阳台植物浇水", false, 0x6ba36f},
    {"sheets", "全家", "换床单", true, 0xf1c45b},
};

void flush_display(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  display->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(&color_p->full), w, h);
  lv_disp_flush_ready(drv);
}

void read_touch(lv_indev_drv_t *, lv_indev_data_t *data) {
  touch.read();
  if (touch.isTouched) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = touch.points[0].x;
    data->point.y = touch.points[0].y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

uint8_t remaining_count() {
  uint8_t remaining = 0;
  for (const auto &task : tasks) {
    if (!task.done) remaining++;
  }
  return remaining;
}

void update_summary() {
  static char buffer[96];
  snprintf(buffer, sizeof(buffer), "%u 件待完成 · 轻触任务标记完成", remaining_count());
  lv_label_set_text(summary_label, buffer);
}

void style_panel(lv_obj_t *obj, uint32_t color, bool done) {
  lv_obj_set_style_radius(obj, 8, 0);
  lv_obj_set_style_bg_color(obj, lv_color_hex(done ? 0xf3eee5 : 0xfffdfa), 0);
  lv_obj_set_style_border_color(obj, lv_color_hex(color), 0);
  lv_obj_set_style_border_width(obj, done ? 1 : 3, 0);
  lv_obj_set_style_pad_all(obj, 14, 0);
}

void task_click(lv_event_t *event) {
  Task *task = static_cast<Task *>(lv_event_get_user_data(event));
  task->done = !task->done;
  update_summary();
  lv_obj_clean(task_grid);
  // Rebuild keeps the demo simple and mirrors the next network refresh.
  for (auto &item : tasks) {
    lv_obj_t *btn = lv_btn_create(task_grid);
    lv_obj_set_size(btn, 360, 86);
    style_panel(btn, item.color, item.done);
    lv_obj_add_event_cb(btn, task_click, LV_EVENT_CLICKED, &item);

    lv_obj_t *member = lv_label_create(btn);
    lv_label_set_text(member, item.member);
    lv_obj_set_style_text_color(member, lv_color_hex(item.color), 0);
    lv_obj_set_style_text_font(member, &lv_font_montserrat_14, 0);
    lv_obj_align(member, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *title = lv_label_create(btn);
    lv_label_set_text(title, item.title);
    lv_obj_set_style_text_color(title, lv_color_hex(item.done ? 0x6e675d : 0x171512), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 12);

    lv_obj_t *done = lv_label_create(btn);
    lv_label_set_text(done, item.done ? "完成" : "待做");
    lv_obj_set_style_text_font(done, &lv_font_montserrat_18, 0);
    lv_obj_align(done, LV_ALIGN_RIGHT_MID, 0, 0);
  }
}

void build_task_grid() {
  task_grid = lv_obj_create(root);
  lv_obj_set_size(task_grid, 760, 250);
  lv_obj_align(task_grid, LV_ALIGN_TOP_MID, 0, 132);
  lv_obj_set_style_bg_opa(task_grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(task_grid, 0, 0);
  lv_obj_set_flex_flow(task_grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(task_grid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_all(task_grid, 0, 0);
  lv_obj_set_style_pad_row(task_grid, 12, 0);
  lv_obj_set_style_pad_column(task_grid, 16, 0);

  for (auto &item : tasks) {
    lv_obj_t *btn = lv_btn_create(task_grid);
    lv_obj_set_size(btn, 360, 86);
    style_panel(btn, item.color, item.done);
    lv_obj_add_event_cb(btn, task_click, LV_EVENT_CLICKED, &item);

    lv_obj_t *member = lv_label_create(btn);
    lv_label_set_text(member, item.member);
    lv_obj_set_style_text_color(member, lv_color_hex(item.color), 0);
    lv_obj_set_style_text_font(member, &lv_font_montserrat_14, 0);
    lv_obj_align(member, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *title = lv_label_create(btn);
    lv_label_set_text(title, item.title);
    lv_obj_set_style_text_color(title, lv_color_hex(item.done ? 0x6e675d : 0x171512), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 12);

    lv_obj_t *done = lv_label_create(btn);
    lv_label_set_text(done, item.done ? "完成" : "待做");
    lv_obj_set_style_text_font(done, &lv_font_montserrat_18, 0);
    lv_obj_align(done, LV_ALIGN_RIGHT_MID, 0, 0);
  }
}

void connect_wifi() {
  if (strlen(WIFI_SSID) == 0) {
    lv_label_set_text(status_label, "离线演示");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
    delay(250);
  }

  lv_label_set_text(status_label, WiFi.status() == WL_CONNECTED ? "Wi-Fi 已连接" : "Wi-Fi 未连接");
}

void build_ui() {
  root = lv_obj_create(nullptr);
  lv_scr_load(root);
  lv_obj_set_style_bg_color(root, lv_color_hex(0xfffaf0), 0);
  lv_obj_set_style_pad_all(root, 20, 0);

  lv_obj_t *title = lv_label_create(root);
  lv_label_set_text(title, "LifeTodo 今日");
  lv_obj_set_style_text_color(title, lv_color_hex(0x171512), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 2);

  status_label = lv_label_create(root);
  lv_label_set_text(status_label, "启动中");
  lv_obj_set_style_text_color(status_label, lv_color_hex(0x6e675d), 0);
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_18, 0);
  lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, -8, 10);

  summary_label = lv_label_create(root);
  lv_obj_set_style_text_color(summary_label, lv_color_hex(0x6e675d), 0);
  lv_obj_set_style_text_font(summary_label, &lv_font_montserrat_18, 0);
  lv_obj_align(summary_label, LV_ALIGN_TOP_LEFT, 10, 72);
  update_summary();

  build_task_grid();

  lv_obj_t *footer = lv_label_create(root);
  lv_label_set_text(footer, "玄关屏 · 本地 GUI 原型 · 下一步接入 lifetodo.xyz 设备 API");
  lv_obj_set_style_text_color(footer, lv_color_hex(0x6e675d), 0);
  lv_obj_set_style_text_font(footer, &lv_font_montserrat_14, 0);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_LEFT, 8, -4);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("LifeTodo ESP32 display starting");

  if (!display->begin()) {
    Serial.println("Display init failed");
    while (true) delay(1000);
  }
  display->fillScreen(BLACK);

  touch.begin();
  touch.setRotation(ROTATION_NORMAL);

  lv_init();
  buf1 = static_cast<lv_color_t *>(ps_malloc(SCREEN_W * 40 * sizeof(lv_color_t)));
  buf2 = static_cast<lv_color_t *>(ps_malloc(SCREEN_W * 40 * sizeof(lv_color_t)));
  if (!buf1 || !buf2) {
    Serial.println("LVGL draw buffer allocation failed");
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
}

void loop() {
  lv_timer_handler();
  delay(5);
}
