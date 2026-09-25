/*
  Reproductor FLAC/MP3 portátil
  ESP32-S3 (N16R8) + DAC PCM5102A + OLED 1.3" SH1106 + microSD

  Placa (Arduino IDE, core esp32 de Espressif 3.x):
    - Placa:            ESP32S3 Dev Module
    - Flash Size:       16MB
    - PSRAM:            OPI PSRAM        <- imprescindible para FLAC
    - USB CDC On Boot:  Enabled
  Librerías:
    - ESP32-audioI2S (schreibfaul1) -> GitHub, "Añadir biblioteca .ZIP"
    - U8g2 (olikraus)               -> Gestor de bibliotecas

  Controles
    Pantalla de reproducción:
      PLAY corto   -> pausa / reanudar
      PLAY largo   -> abrir el navegador de carpetas
      NEXT         -> siguiente pista
      PREV         -> reinicia la pista (o la anterior si van < 3 s)
      VOL+ / VOL-  -> volumen (mantener para repetir)
    Navegador:
      PREV / NEXT  -> subir / bajar (mantener para desplazarse rápido)
      PLAY corto   -> entrar en carpeta / reproducir canción
      PLAY largo   -> volver a la pantalla de reproducción
      VOL- corto   -> carpeta anterior (atrás)
      VOL+         -> volumen
*/

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <U8g2lib.h>
#include <vector>
#include <algorithm>
#include <strings.h>
#include "Audio.h"

// ---------------- Pines ----------------
// I2S -> PCM5102A
#define I2S_BCLK 4
#define I2S_LRC  5
#define I2S_DOUT 6
// microSD (SPI)
#define SD_CS    10
#define SD_MOSI  11
#define SD_SCK   12
#define SD_MISO  13
// OLED (I2C)
#define OLED_SDA 8
#define OLED_SCL 9
// Botones (a GND, con pull-up interno)
#define BTN_VOLUP 15
#define BTN_VOLDN 16
#define BTN_PREV  17
#define BTN_PLAY  18
#define BTN_NEXT  21
// Batería (divisor 100k/100k)
#define BAT_ADC   1

// ---------------- Ajustes ----------------
const uint8_t  VOL_MAX     = 21;   // pasos de volumen de la librería
const uint8_t  VOL_INICIAL = 8;
const uint32_t DEBOUNCE_MS = 25;
const uint32_t LONG_MS     = 600;
const uint32_t REPEAT_MS   = 120;
const float    BAT_DIVISOR = 2.0f;
const uint32_t SD_SPI_HZ   = 20000000;  // bájalo a 10 MHz si hay cortes o errores

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
Audio audio;

volatile uint8_t volume = VOL_INICIAL;

// ============ Tarea de audio (núcleo 0) ============
// Toda llamada a la librería de audio ocurre en esta tarea; la interfaz
// solo le manda órdenes por una cola. Así redibujar la pantalla nunca corta el sonido.
enum CmdType : uint8_t { CMD_PLAY, CMD_TOGGLE, CMD_VOL, CMD_STOP };
struct Cmd {
  CmdType type;
  int value;
  char path[256];
};
QueueHandle_t cmdQ;

volatile bool     aRunning = false;
volatile uint32_t aPos = 0, aDur = 0;

void sendCmd(CmdType t, int v = 0, const char* p = nullptr) {
  Cmd c;
  c.type = t;
  c.value = v;
  c.path[0] = 0;
  if (p) strlcpy(c.path, p, sizeof(c.path));
  xQueueSend(cmdQ, &c, pdMS_TO_TICKS(50));
}

void audioTask(void*) {
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volume);
  Cmd c;
  for (;;) {
    while (xQueueReceive(cmdQ, &c, 0) == pdTRUE) {
      switch (c.type) {
        case CMD_PLAY:   audio.connecttoFS(SD, c.path); break;
        case CMD_TOGGLE: audio.pauseResume();           break;
        case CMD_VOL:    audio.setVolume(c.value);      break;
        case CMD_STOP:   audio.stopSong();              break;
      }
    }
    audio.loop();
    aRunning = audio.isRunning();
    aPos = audio.getAudioCurrentTime();
    aDur = audio.getAudioFileDuration();
    vTaskDelay(1);
  }
}

// ============ Botones ============
enum BtnEvt { EV_NONE, EV_DOWN, EV_SHORT, EV_LONG, EV_REPEAT };

struct Button {
  uint8_t pin;
  bool raw = false, stable = false, longDone = false;
  uint32_t tRaw = 0, tDown = 0, tRep = 0;

  BtnEvt update(uint32_t now) {
    bool r = digitalRead(pin) == LOW;
    if (r != raw) { raw = r; tRaw = now; }
    if (raw != stable && now - tRaw >= DEBOUNCE_MS) {
      stable = raw;
      if (stable) { tDown = now; longDone = false; return EV_DOWN; }
      return longDone ? EV_NONE : EV_SHORT;
    }
    if (stable) {
      if (!longDone && now - tDown >= LONG_MS) { longDone = true; tRep = now; return EV_LONG; }
      if (longDone && now - tRep >= REPEAT_MS) { tRep = now; return EV_REPEAT; }
    }
    return EV_NONE;
  }
};

Button bVolUp{BTN_VOLUP}, bVolDn{BTN_VOLDN}, bPrev{BTN_PREV}, bPlay{BTN_PLAY}, bNext{BTN_NEXT};

// ============ Utilidades de rutas ============
bool isAudio(const String& n) {
  String l = n;
  l.toLowerCase();
  return l.endsWith(".flac") || l.endsWith(".mp3") || l.endsWith(".wav") ||
         l.endsWith(".m4a") || l.endsWith(".aac");
}
String joinPath(const String& dir, const String& name) {
  return dir == "/" ? "/" + name : dir + "/" + name;
}
String parentDir(const String& p) {
  int i = p.lastIndexOf('/');
  return i <= 0 ? String("/") : p.substring(0, i);
}
String baseName(const String& p) {
  if (p == "/") return "Tarjeta SD";
  return p.substring(p.lastIndexOf('/') + 1);
}
// "03 - Mi cancion.flac" -> "Mi cancion"
String cleanTitle(String n) {
  int dot = n.lastIndexOf('.');
  if (dot > 0) n = n.substring(0, dot);
  int i = 0;
  while (i < (int)n.length() && isDigit(n[i])) i++;
  if (i > 0 && i <= 3) {
    while (i < (int)n.length() && (n[i] == ' ' || n[i] == '.' || n[i] == '-' || n[i] == '_')) i++;
    if (i < (int)n.length()) n = n.substring(i);
  }
  return n;
}

struct Entry {
  String name;
  bool isDir;
};

std::vector<Entry> readDir(const String& path) {
  std::vector<Entry> v;
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) return v;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    String n = f.name();
    int s = n.lastIndexOf('/');
    if (s >= 0) n = n.substring(s + 1);
    bool d = f.isDirectory();
    f.close();
    if (n.startsWith(".") || n == "System Volume Information") continue;
    if (d || isAudio(n)) v.push_back({n, d});
  }
  dir.close();
  std::sort(v.begin(), v.end(), [](const Entry& a, const Entry& b) {
    if (a.isDir != b.isDir) return a.isDir;           // carpetas primero
    return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
  });
  return v;
}

// ============ Estado ============
enum Mode { MODE_BROWSER, MODE_PLAYER };
Mode mode = MODE_BROWSER;

String brDir = "/";
std::vector<Entry> brList;
int brSel = 0, brTop = 0;

String plDir;
std::vector<String> playlist;
int plIndex = -1;
bool playing = false, paused = false;
uint32_t tGrace = 0;          // margen para que la tarea de audio arranque
String curTitle, curAlbum;

float batV = 0;
int batPct = 0;

// ============ Reproducción ============
void playIndex(int i) {
  if (i < 0 || i >= (int)playlist.size()) {   // fin de la carpeta
    playing = false;
    paused = false;
    sendCmd(CMD_STOP);
    return;
  }
  plIndex = i;
  String p = joinPath(plDir, playlist[i]);
  sendCmd(CMD_PLAY, 0, p.c_str());
  playing = true;
  paused = false;
  tGrace = millis();
  curTitle = cleanTitle(playlist[i]);
  curAlbum = baseName(plDir);
}

void togglePlay() {
  if (plIndex < 0) { mode = MODE_BROWSER; return; }
  if (!playing) { playIndex(plIndex); return; }
  sendCmd(CMD_TOGGLE);
  paused = !paused;
  tGrace = millis();
}

void changeVolume(int d) {
  int v = constrain((int)volume + d, 0, (int)VOL_MAX);
  if (v != volume) {
    volume = v;
    sendCmd(CMD_VOL, v);
  }
}

// ============ Navegador ============
void openDir(const String& path, const String& selectName = "") {
  brDir = path;
  brList = readDir(path);
  brSel = 0;
  brTop = 0;
  if (selectName.length()) {
    for (size_t i = 0; i < brList.size(); i++)
      if (brList[i].name == selectName) { brSel = i; break; }
  }
}

void startFromBrowser() {
  plDir = brDir;
  playlist.clear();
  int start = 0;
  for (auto& e : brList) {
    if (e.isDir) continue;
    if (e.name == brList[brSel].name) start = playlist.size();
    playlist.push_back(e.name);
  }
  playIndex(start);
  mode = MODE_PLAYER;
}

void browserSelect() {
  if (brList.empty()) return;
  const Entry& e = brList[brSel];
  if (e.isDir) openDir(joinPath(brDir, e.name));
  else startFromBrowser();
}

void browserBack() {
  if (brDir == "/") {
    if (plIndex >= 0) mode = MODE_PLAYER;
    return;
  }
  String child = baseName(brDir);
  openDir(parentDir(brDir), child);
}

void browserMove(int d) {
  int n = brList.size();
  if (n == 0) return;
  brSel = (brSel + d + n) % n;
}

// ============ Entrada ============
void handleButtons() {
  uint32_t now = millis();
  BtnEvt eu = bVolUp.update(now);
  BtnEvt ed = bVolDn.update(now);
  BtnEvt ep = bPrev.update(now);
  BtnEvt ey = bPlay.update(now);
  BtnEvt en = bNext.update(now);

  if (eu == EV_DOWN || eu == EV_LONG || eu == EV_REPEAT) changeVolume(+1);

  if (mode == MODE_PLAYER) {
    if (ed == EV_DOWN || ed == EV_LONG || ed == EV_REPEAT) changeVolume(-1);
    if (ey == EV_SHORT) togglePlay();
    if (ey == EV_LONG) mode = MODE_BROWSER;
    if (en == EV_SHORT && plIndex >= 0) playIndex(plIndex + 1);
    if (ep == EV_SHORT && plIndex >= 0) {
      if (aPos > 3 || plIndex == 0) playIndex(plIndex);
      else playIndex(plIndex - 1);
    }
  } else {
    if (ep == EV_SHORT || ep == EV_LONG || ep == EV_REPEAT) browserMove(-1);
    if (en == EV_SHORT || en == EV_LONG || en == EV_REPEAT) browserMove(+1);
    if (ey == EV_SHORT) browserSelect();
    if (ey == EV_LONG && plIndex >= 0) mode = MODE_PLAYER;
    if (ed == EV_SHORT) browserBack();
  }
}

// ============ Batería ============
void readBattery() {
  float v = analogReadMilliVolts(BAT_ADC) * BAT_DIVISOR / 1000.0f;
  batV = (batV == 0) ? v : batV * 0.8f + v * 0.2f;
  batPct = constrain((int)((batV - 3.30f) / (4.15f - 3.30f) * 100.0f), 0, 100);
}

// ============ Pantalla ============
String fmtTime(uint32_t s) {
  char b[16];
  snprintf(b, sizeof(b), "%u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
  return String(b);
}

void drawBattery(int x, int y) {
  u8g2.drawFrame(x, y, 12, 7);
  u8g2.drawBox(x + 12, y + 2, 2, 3);
  int w = batPct * 10 / 100;
  if (w > 0) u8g2.drawBox(x + 1, y + 1, w, 5);
}

// Texto centrado; si no cabe, se desplaza con pausas al principio y al final
void drawScrolling(const String& s, int y, const uint8_t* font) {
  u8g2.setFont(font);
  int w = u8g2.getUTF8Width(s.c_str());
  if (w <= 128) {
    u8g2.drawUTF8((128 - w) / 2, y, s.c_str());
    return;
  }
  int span = w - 128 + 4;
  int t = (millis() / 40) % (span + 60);
  int off = constrain(t - 30, 0, span);
  u8g2.drawUTF8(2 - off, y, s.c_str());
}

void drawPlayer() {
  u8g2.setFont(u8g2_font_5x7_tf);
  char vb[10];
  snprintf(vb, sizeof(vb), "VOL %u", (unsigned)volume);
  u8g2.drawStr(0, 7, vb);
  const char* st = !playing ? "STOP" : (paused ? "PAUSA" : "PLAY");
  u8g2.drawStr((128 - u8g2.getStrWidth(st)) / 2, 7, st);
  drawBattery(113, 0);
  u8g2.drawHLine(0, 9, 128);

  drawScrolling(curTitle, 27, u8g2_font_7x13B_tf);
  drawScrolling(curAlbum, 40, u8g2_font_6x10_tf);

  uint32_t pos = aPos, dur = aDur;
  u8g2.drawFrame(0, 46, 128, 5);
  if (dur > 0) {
    uint32_t w = std::min<uint32_t>(126, pos * 126 / dur);
    if (w) u8g2.drawBox(1, 47, w, 3);
  }
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(0, 63, fmtTime(pos).c_str());
  if (dur > 0) {
    String d = fmtTime(dur);
    u8g2.drawStr(128 - u8g2.getStrWidth(d.c_str()), 63, d.c_str());
  }
  char ib[16];
  snprintf(ib, sizeof(ib), "%d/%d", plIndex + 1, (int)playlist.size());
  u8g2.drawStr((128 - u8g2.getStrWidth(ib)) / 2, 63, ib);
}

void drawBrowser() {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawBox(0, 0, 128, 11);
  u8g2.setDrawColor(0);
  u8g2.drawUTF8(2, 9, baseName(brDir).c_str());
  u8g2.setDrawColor(1);

  if (brList.empty()) {
    u8g2.drawStr(2, 32, "(carpeta vacia)");
    return;
  }
  const int rows = 5;
  if (brSel < brTop) brTop = brSel;
  if (brSel >= brTop + rows) brTop = brSel - rows + 1;

  u8g2.setClipWindow(0, 12, 122, 63);
  for (int r = 0; r < rows && brTop + r < (int)brList.size(); r++) {
    int i = brTop + r;
    int y = 13 + r * 10;
    const Entry& e = brList[i];
    String label = String(e.isDir ? "> " : "") + (e.isDir ? e.name : cleanTitle(e.name));
    if (i == brSel) {
      u8g2.drawBox(0, y, 123, 10);
      u8g2.setDrawColor(0);
    }
    u8g2.drawUTF8(2, y + 8, label.c_str());
    u8g2.setDrawColor(1);
  }
  u8g2.setMaxClipWindow();

  int n = brList.size();
  int barH = std::max(4, 50 * rows / std::max(n, rows));
  int barY = 13 + (50 - barH) * brSel / std::max(1, n - 1);
  u8g2.drawVLine(126, 13, 50);
  u8g2.drawBox(125, barY, 3, barH);
}

void message(const char* l1, const char* l2 = "") {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr((128 - u8g2.getStrWidth(l1)) / 2, 28, l1);
  u8g2.drawStr((128 - u8g2.getStrWidth(l2)) / 2, 42, l2);
  u8g2.sendBuffer();
}

// ============ Arranque ============
void setup() {
  Serial.begin(115200);
  for (uint8_t p : {BTN_VOLUP, BTN_VOLDN, BTN_PREV, BTN_PLAY, BTN_NEXT}) pinMode(p, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetPinAttenuation(BAT_ADC, ADC_11db);

  u8g2.setBusClock(400000);
  u8g2.begin();
  message("Iniciando...");

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  while (!SD.begin(SD_CS, SPI, SD_SPI_HZ)) {
    message("Inserta la microSD", "(FAT32)");
    SD.end();
    delay(1000);
  }

  cmdQ = xQueueCreate(6, sizeof(Cmd));
  xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr, 3, nullptr, 0);

  readBattery();
  openDir("/");
}

void loop() {
  handleButtons();

  // Fin de pista -> siguiente
  if (playing && !paused && !aRunning && millis() - tGrace > 1500) playIndex(plIndex + 1);

  static uint32_t tBat = 0, tDraw = 0;
  uint32_t now = millis();
  if (now - tBat > 2000) { tBat = now; readBattery(); }
  if (now - tDraw > 80) {
    tDraw = now;
    u8g2.clearBuffer();
    if (mode == MODE_PLAYER) drawPlayer();
    else drawBrowser();
    u8g2.sendBuffer();
  }
  delay(2);
}
