#include <WiFi.h>
#include <esp_now.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
SPIClass mySpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
uint16_t touchScreenMinimumX = 200, touchScreenMaximumX = 3700, touchScreenMinimumY = 240, touchScreenMaximumY = 3800;

static const uint16_t screenWidth = 240;
static const uint16_t screenHeight = 320;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];
TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);

uint8_t receiverAddress[] = { 0xD8, 0x3B, 0xDA, 0xC8, 0x95, 0xEC };

void sendCommand(const char* message) {
  esp_now_send(receiverAddress, (uint8_t*)message, strlen(message) + 1);
  Serial.print("Sent: ");
  Serial.println(message);
}

void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp_drv);
}

void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    if (p.x < touchScreenMinimumX) touchScreenMinimumX = p.x;
    if (p.x > touchScreenMaximumX) touchScreenMaximumX = p.x;
    if (p.y < touchScreenMinimumY) touchScreenMinimumY = p.y;
    if (p.y > touchScreenMaximumY) touchScreenMaximumY = p.y;
    data->point.x = map(p.x, touchScreenMinimumX, touchScreenMaximumX, 1, screenWidth);
    data->point.y = map(p.y, touchScreenMinimumY, touchScreenMaximumY, 1, screenHeight);
    data->state = LV_INDEV_STATE_PR;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

void prop_start_cb(lv_event_t* e) {
  sendCommand("PROP_START");
}

void prop_stop_cb(lv_event_t* e) {
  sendCommand("PROP_STOP");
}

void weight_start_cb(lv_event_t* e) {
  sendCommand("WEIGHT_START");
}

void weight_stop_cb(lv_event_t* e) {
  sendCommand("WEIGHT_STOP");
}

void rpm_event_cb(lv_event_t* e) {
  int rpm = (int)lv_event_get_user_data(e);
  char msg[16];
  sprintf(msg, "RPM:%d", rpm);
  sendCommand(msg);
}

void setupUI() {
  // Set screen background to darker blue (changed from 0x10335B to 0x0A1F35)
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x0A1F35), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t* label;
  lv_obj_t* btn;
  int y = 20;
  int y_spacing = 60;

  auto make_button = [](const char* text, lv_event_cb_t cb, int y, lv_color_t bgColor) {
    lv_obj_t* btn = lv_btn_create(lv_scr_act());
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_size(btn, 160, 40);
    lv_obj_set_style_bg_color(btn, bgColor, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 6, LV_PART_MAIN);
    lv_obj_set_style_outline_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_outline_color(btn, lv_color_hex(0x1A2D20), LV_PART_MAIN); // Darker outline
    lv_obj_set_style_text_color(btn, lv_color_hex(0x1A2D20), LV_PART_MAIN); // Darker text (changed from 0x2D4634)

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    return btn;
  };

  // Buttons with wood color
  make_button("Prop Start",  prop_start_cb,  y,             lv_color_hex(0xC89354));
  make_button("Prop Stop",   prop_stop_cb,   y += y_spacing,lv_color_hex(0xC89354));
  make_button("Weight Start",weight_start_cb,y += y_spacing,lv_color_hex(0xC89354));
  make_button("Weight Stop", weight_stop_cb, y += y_spacing,lv_color_hex(0xC89354));

  // RPM buttons with green color
  int rpms[] = {10, 30, 60};
  for (int i = 0; i < 3; i++) {
    int rpm = rpms[i];
    lv_obj_t* btn = lv_btn_create(lv_scr_act());
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y + (i * y_spacing));
    lv_obj_set_size(btn, 160, 40);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x4CAF50), LV_PART_MAIN); // Green
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 6, LV_PART_MAIN);
    lv_obj_set_style_outline_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_outline_color(btn, lv_color_hex(0x1A2D20), LV_PART_MAIN); // Darker outline
    lv_obj_set_style_text_color(btn, lv_color_hex(0x1A2D20), LV_PART_MAIN); // Darker text (changed from 0x2D4634)

    label = lv_label_create(btn);
    lv_label_set_text_fmt(label, "%d RPM", rpm);
    lv_obj_center(label);
    lv_obj_add_event_cb(btn, rpm_event_cb, LV_EVENT_CLICKED, (void*)rpm);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  lv_init();

  tft.fillScreen(TFT_NAVY); // Optional: wipe any leftover framebuffer

  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(mySpi);
  ts.setRotation(0);
  tft.begin();
  tft.setRotation(0);

  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);
  lv_disp_t* disp = lv_disp_get_default();
  lv_theme_t* th = lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
  lv_disp_set_theme(disp, NULL);  // Disable theme

  setupUI();
  Serial.println("CYD Controller Ready");
}

void loop() {
  lv_timer_handler();
  delay(5);
}