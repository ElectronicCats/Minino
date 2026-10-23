#include "menu_icons.h"
#include <math.h>
#include <string.h>
#include "oled_screen.h"

/* 12x12 monochrome icon library for menu items, drawn with oled_screen
   primitives (OLED_DISPLAY_NORMAL = lit).
 */
static void p(int x, int y) {
  oled_screen_draw_pixel(x, y, OLED_DISPLAY_NORMAL);
}
static void ln(int a, int b, int c, int d) {
  oled_screen_draw_line(a, b, c, d, OLED_DISPLAY_NORMAL);
}
static void rc(int x, int y, int w, int h) {
  oled_screen_draw_rect(x, y, w, h, OLED_DISPLAY_NORMAL);
}
static void circ(int cx, float cy, float r) {
  for (int a = 0; a < 360; a += 15)
    p(cx + (int) roundf(r * cosf(a * M_PI / 180)),
      (int) roundf(cy + r * sinf(a * M_PI / 180)));
}
static void arc(int cx, float cy, float r, int a0, int a1) {
  for (int a = a0; a <= a1; a += 6)
    p(cx + (int) roundf(r * sinf(a * M_PI / 180)),
      (int) roundf(cy - r * cosf(a * M_PI / 180)));
}

/* ---- category / base glyphs ---- */
void ic_wifi(int x, int y) {
  for (int r = 2; r <= 6; r += 2)
    arc(x + 6, y + 8, r, -45, 45);
  arc(x + 6, y + 8, 7, -45, 45);
  p(x + 6, y + 8);
}
void ic_bt(int x, int y) {
  int c = x + 6;
  ln(c, y + 1, c, y + 8);
  ln(c, y + 1, c + 3, y + 3);
  ln(c + 3, y + 3, c - 3, y + 6);
  ln(c, y + 8, c + 3, y + 6);
  ln(c + 3, y + 6, c - 3, y + 3);
}
void ic_zig(int x, int y) {
  int cx = x + 6, hx[6], hy[6];
  float cy = y + 4.5f;
  for (int i = 0; i < 6; i++) {
    float a = (i * 60 + 30) * M_PI / 180;
    hx[i] = cx + (int) roundf(3.8f * cosf(a));
    hy[i] = (int) roundf(cy + 3.8f * sinf(a));
  }
  for (int i = 0; i < 6; i++)
    ln(hx[i], hy[i], hx[(i + 1) % 6], hy[(i + 1) % 6]);
}
void ic_thread(int x, int y) {
  int nx[4] = {x + 2, x + 10, x + 2, x + 10},
      ny[4] = {y + 1, y + 1, y + 8, y + 8};
  for (int i = 0; i < 4; i++)
    for (int j = i + 1; j < 4; j++)
      ln(nx[i], ny[i], nx[j], ny[j]);
}
void ic_gps(int x, int y) {
  ln(x + 4, y + 1, x + 8, y + 1);
  ln(x + 3, y + 2, x + 5, y + 2);
  ln(x + 7, y + 2, x + 9, y + 2);
  ln(x + 3, y + 3, x + 4, y + 3);
  ln(x + 8, y + 3, x + 9, y + 3);
  ln(x + 3, y + 4, x + 5, y + 4);
  ln(x + 7, y + 4, x + 9, y + 4);
  ln(x + 4, y + 5, x + 8, y + 5);
  ln(x + 5, y + 6, x + 7, y + 6);
  ln(x + 5, y + 7, x + 7, y + 7);
  p(x + 6, y + 8);
}
void ic_chip(int x, int y) {
  rc(x + 2, y + 1, 8, 8);
  for (int i = 0; i < 3; i++) {
    ln(x, y + 2 + i * 2, x + 1, y + 2 + i * 2);
    ln(x + 10, y + 2 + i * 2, x + 11, y + 2 + i * 2);
  }
}
void ic_folder(int x, int y) {
  ln(x + 1, y + 1, x + 4, y + 1);
  ln(x + 4, y + 1, x + 5, y + 2);
  rc(x + 1, y + 2, 10, 7);
}
void ic_apps(int x, int y) {
  for (int r = 0; r < 2; r++)
    for (int c = 0; c < 2; c++)
      rc(x + 2 + c * 5, y + 1 + r * 5, 3, 3);
}
void ic_gear(int x, int y) {
  circ(x + 6, y + 4.5f, 1.8f);
  for (int a = 0; a < 360; a += 45)
    p(x + 6 + (int) roundf(3.6f * cosf(a * M_PI / 180)),
      (int) roundf(y + 4.5f + 3.6f * sinf(a * M_PI / 180)));
}
void ic_info(int x, int y) {
  circ(x + 6, y + 4.5f, 3.6f);
  p(x + 6, y + 3);
  ln(x + 6, y + 4, x + 6, y + 6);
}
void ic_dot(int x, int y) {
  rc(x + 4, y + 3, 4, 4);
}

/* ---- reusable action metaphors ---- */
void ic_scan(int x, int y) {
  circ(x + 5, y + 3.5f, 2.5f);
  ln(x + 7, y + 5, x + 10, y + 8);
} /* magnifier */
void ic_waves(int x, int y) {
  ln(x + 2, y + 3, x + 6, y + 1);
  ln(x + 2, y + 6, x + 6, y + 8);
  ln(x + 2, y + 3, x + 2, y + 6);
  ln(x + 6, y + 1, x + 6, y + 8);
  arc(x + 8, y + 4.5f, 2, 10, 80);
  arc(x + 8, y + 4.5f, 4, 10, 80);
}
void ic_bolt(int x, int y) {
  ln(x + 7, y + 1, x + 3, y + 4);
  ln(x + 3, y + 4, x + 6, y + 4);
  ln(x + 6, y + 4, x + 4, y + 8);
  ln(x + 4, y + 8, x + 8, y + 4);
  ln(x + 8, y + 4, x + 5, y + 4);
} /* lightning */
void ic_play(int x, int y) {
  for (int i = 0; i < 4; i++)
    ln(x + 4, y + 1 + i, x + 4, y + 8 - i);
  ln(x + 4, y + 1, x + 10, y + 4);
  ln(x + 4, y + 8, x + 10, y + 5);
}
void ic_doc(int x, int y) {
  rc(x + 2, y + 1, 8, 8);
  ln(x + 4, y + 3, x + 7, y + 3);
  ln(x + 4, y + 5, x + 7, y + 5);
  ln(x + 4, y + 7, x + 7, y + 7);
}
void ic_download(int x, int y) {
  ln(x + 6, y + 1, x + 6, y + 5);
  ln(x + 3, y + 4, x + 6, y + 6);
  ln(x + 9, y + 4, x + 6, y + 6);
  ln(x + 2, y + 8, x + 10, y + 8);
}
void ic_card(int x, int y) {
  rc(x + 2, y + 1, 8, 8);
  p(x + 8, y + 1);
  p(x + 9, y + 2);
  for (int i = 0; i < 3; i++)
    ln(x + 3, y + 2 + i, x + 3, y + 2 + i);
}
void ic_clock(int x, int y) {
  circ(x + 6, y + 4.5f, 3.6f);
  ln(x + 6, y + 4, x + 6, y + 2);
  ln(x + 6, y + 5, x + 8, y + 5);
}
void ic_speed(int x, int y) {
  arc(x + 6, y + 7.0f, 5.0f, -70, 70);
  ln(x + 6, y + 7, x + 8, y + 3);
  p(x + 6, y + 7);
}
void ic_route(int x, int y) {
  ln(x + 2, y + 8, x + 5, y + 8);
  ln(x + 5, y + 8, x + 5, y + 4);
  ln(x + 5, y + 4, x + 9, y + 4);
  ln(x + 9, y + 4, x + 9, y + 1);
  p(x + 2, y + 8);
  p(x + 9, y + 1);
}
void ic_sat(int x, int y) {
  arc(x + 6, y + 6, 4, -80, 10);
  ln(x + 6, y + 6, x + 9, y + 2);
  circ(x + 6, y + 6, 1);
}
void ic_bulb(int x, int y) {
  circ(x + 6, y + 3, 2);
  ln(x + 4, y + 5, x + 4, y + 6);
  ln(x + 8, y + 5, x + 8, y + 6);
  ln(x + 4, y + 7, x + 8, y + 7);
  ln(x + 5, y + 8, x + 7, y + 8);
}
void ic_toggle(int x, int y) {
  rc(x + 1, y + 2, 10, 6);
  rc(x + 2, y + 3, 3, 4);
}
void ic_moon(int x, int y) {
  arc(x + 5, y + 4.5f, 4.0f, -145, 145);
  arc(x + 7, y + 4.5f, 4.0f, -145, 145);
}
void ic_stealth(int x, int y) {
  ln(x + 2, y + 1, x + 9, y + 1);
  ln(x + 2, y + 8, x + 9, y + 8);
  ln(x + 1, y + 2, x + 1, y + 7);
  ln(x + 10, y + 2, x + 10, y + 7);
  rc(x + 2, y + 3, 8, 3);
  p(x + 4, y + 4);
  p(x + 7, y + 4);
}
void ic_zzz(int x, int y) {
  ln(x + 8, y + 1, x + 10, y + 1);
  p(x + 9, y + 2);
  ln(x + 8, y + 3, x + 10, y + 3);
  ln(x + 5, y + 3, x + 7, y + 3);
  p(x + 6, y + 4);
  ln(x + 5, y + 5, x + 7, y + 5);
  ln(x + 1, y + 5, x + 4, y + 5);
  ln(x + 4, y + 5, x + 1, y + 8);
  ln(x + 1, y + 8, x + 4, y + 8);
}
void ic_drone(int x, int y) {
  circ(x + 3, y + 2, 1);
  circ(x + 9, y + 2, 1);
  circ(x + 3, y + 7, 1);
  circ(x + 9, y + 7, 1);
  rc(x + 5, y + 3, 3, 3);
}
void ic_cam(int x, int y) {
  rc(x + 1, y + 2, 10, 7);
  rc(x + 3, y + 1, 3, 2);
  circ(x + 7, y + 5, 2);
  p(x + 10, y + 4);
}
void ic_key(int x, int y) {
  rc(x + 1, y + 2, 10, 6);
  for (int i = 0; i < 4; i++)
    p(x + 2 + i * 2, y + 5);
} /* keyboard */
void ic_tag(int x, int y) {
  rc(x + 2, y + 1, 6, 6);
  circ(x + 5, y + 4, 2);
  ln(x + 8, y + 7, x + 10, y + 8);
}
void ic_lock(int x, int y) {
  rc(x + 2, y + 3, 8, 6);
  arc(x + 6, y + 3, 2, -90, 90);
  p(x + 6, y + 5);
  p(x + 6, y + 6);
}
void ic_term(int x, int y) {
  rc(x + 1, y + 1, 10, 8);
  ln(x + 3, y + 4, x + 5, y + 5);
  ln(x + 5, y + 5, x + 3, y + 6);
  ln(x + 6, y + 7, x + 9, y + 7);
}
void ic_monitor(int x, int y) {
  rc(x + 1, y + 1, 10, 5);
  ln(x + 5, y + 6, x + 7, y + 6);
  ln(x + 5, y + 7, x + 7, y + 7);
  ln(x + 3, y + 8, x + 9, y + 8);
}
void ic_list(int x, int y) {
  for (int i = 0; i < 3; i++) {
    p(x + 2, y + 2 + i * 3);
    ln(x + 4, y + 2 + i * 3, x + 10, y + 2 + i * 3);
  }
}
void ic_disk(int x, int y) {
  rc(x + 2, y + 1, 8, 8);
  circ(x + 6, y + 5, 2);
}
void ic_apple(int x, int y) {
  arc(x + 5, y + 5.5f, 2.7f, -170, 10);
  arc(x + 8, y + 5.5f, 2.7f, -10, 170);
  ln(x + 6, y + 1, x + 8, y + 3);
  p(x + 7, y + 2);
  p(x + 10, y + 4);
}
void ic_android(int x, int y) {
  arc(x + 6, y + 5, 4, -90, 90);
  ln(x + 2, y + 5, x + 10, y + 5);
  ln(x + 2, y + 5, x + 2, y + 8);
  ln(x + 10, y + 5, x + 10, y + 8);
  p(x + 4, y + 3);
  p(x + 8, y + 3);
  ln(x + 4, y + 1, x + 5, y + 2);
  ln(x + 8, y + 1, x + 7, y + 2);
}
void ic_windows(int x, int y) {
  rc(x + 2, y + 1, 4, 3);
  rc(x + 7, y + 1, 4, 3);
  rc(x + 2, y + 5, 4, 4);
  rc(x + 7, y + 5, 4, 4);
}
void ic_samsung(int x, int y) {
  rc(x + 3, y + 1, 6, 8);
  ln(x + 5, y + 7, x + 7, y + 7);
}
void ic_all(int x, int y) {
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++)
      p(x + 3 + c * 3, y + 2 + r * 3);
}
void ic_check(int x, int y) {
  ln(x + 2, y + 5, x + 5, y + 8);
  ln(x + 5, y + 8, x + 10, y + 1);
}
void ic_back(int x, int y) {
  ln(x + 2, y + 4, x + 6, y + 1);
  ln(x + 2, y + 5, x + 6, y + 8);
  ln(x + 2, y + 4, x + 10, y + 4);
  ln(x + 2, y + 5, x + 10, y + 5);
}

typedef void (*gfn)(int, int);
typedef struct {
  const char* name;
  gfn fn;
} map_t;
static map_t MAP[] = {
    {"Applications", ic_apps},
    {"Settings", ic_gear},
    {"About", ic_info},
    {"Version", ic_info},
    {"License", ic_doc},
    {"Credits", ic_doc},
    {"Legal", ic_doc},
    {"Update", ic_download},
    {"Hello APP", ic_dot},
    {"WiFi", ic_wifi},
    {"Bluetooth", ic_bt},
    {"Zigbee", ic_zig},
    {"Thread", ic_thread},
    {"GPS", ic_gps},
    {"GPIO", ic_chip},
    {"File Manager", ic_folder},
    {"Analyzer", ic_scan},
    {"Captive", ic_lock},
    {"Deauth", ic_bolt},
    {"Deauth Scan", ic_scan},
    {"DoS", ic_bolt},
    {"SSID Spammer", ic_waves},
    {"Drone ID", ic_drone},
    {"DroneID Scan", ic_scan},
    {"Modbus TCP", ic_chip},
    {"Flock / ALPR", ic_cam},
    {"Flock Sim", ic_waves},
    {"Spam", ic_waves},
    {"HID", ic_key},
    {"ADV Scanner", ic_scan},
    {"Trackers", ic_tag},
    {"Apple", ic_apple},
    {"Android", ic_android},
    {"Windows", ic_windows},
    {"Samsung", ic_samsung},
    {"All", ic_all},
    {"Spoofing", ic_waves},
    {"Sniffer", ic_scan},
    {"Switch", ic_toggle},
    {"Light", ic_bulb},
    {"Broadcast", ic_waves},
    {"Run", ic_play},
    {"Wardriving", ic_gps},
    {"Date & Time", ic_clock},
    {"Location", ic_gps},
    {"Speed", ic_speed},
    {"Route", ic_route},
    {"Num Sats", ic_sat},
    {"Config", ic_gear},
    {"Help", ic_info},
    {"AP Start", ic_play},
    {"Zigbee Start", ic_play},
    {"Thread Start", ic_play},
    {"I2C Scanner", ic_scan},
    {"UART Bridge", ic_term},
    {"Local", ic_folder},
    {"Web", ic_download},
    {"Display", ic_monitor},
    {"System", ic_gear},
    {"Stealth Mode", ic_stealth},
    {"Menu Style", ic_list},
    {"Sleep Mode", ic_zzz},
    {"ZB CLI", ic_term},
    {"Logs Output", ic_list},
    {"SD card", ic_card},
    {"Info", ic_info},
    {"Check Format", ic_check},
    {"Format", ic_disk},
    {"SDR", ic_waves},
    {"Spectrum", ic_waves},
};
menu_icon_fn menu_icon_for(const char* n) {
  for (unsigned i = 0; i < sizeof(MAP) / sizeof(MAP[0]); i++)
    if (!strcmp(MAP[i].name, n))
      return MAP[i].fn;
  return ic_dot;
}
