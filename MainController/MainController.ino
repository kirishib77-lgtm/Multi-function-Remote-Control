
#include <MCUFRIEND_kbv.h>
#include <Adafruit_GFX.h>
#include <TouchScreen.h>
#include <U8g2_for_Adafruit_GFX.h>  // кириллица
#include <RH_ASK.h>                 // Радио передатчик 
#include <SPI.h>                    

/***** ДИСПЛЕЙ + русскийяз *****/
MCUFRIEND_kbv tft;
U8G2_FOR_ADAFRUIT_GFX u8g2;

/***** ЦВЕТА  *****/
#define WHITE 0xFFFF
#define BLACK 0x0000
#define BLUE  0x001F

/***** ТАЧПАд (пины + калибровка под setRotation(1)) *****/
#define XP 7
#define XM A1
#define YP A2
#define YM 6
TouchScreen ts(XP, YP, XM, YM, 300);
#define TS_LEFT  950
#define TS_RT    195
#define TS_TOP   181
#define TS_BOT   919
#define MINPRESSURE 120
#define MAXPRESSURE 1000

/***** ЯЗЫКИ *****/
enum Lang { RU, EN };
Lang lang = RU;

/***** ТЕКСТЫ ДЛЯ МЕНЮ И НЕ ТОЛЬКО *****/
struct Strings {
  const char* title_main;
  const char* b_radio;
  const char* b_ir;
  const char* b_lang;
  const char* b_about;

  const char* title_radio;
  const char* r_temp;
  const char* r_press;
  const char* b_back;

  const char* ir_msg;

  const char* title_lang;
  const char* b_ru;
  const char* b_en;

  const char* title_about;
  const char* about_text;
};

const Strings STR_RU = {
  "Главное меню",
  "Радиосигнал",
  "Инфракрасный порт",
  "Смена языка",
  "О продукте",

  "Радиосигнал",
  "Узнать температуру",
  "Узнать давление",
  "Назад",

  "В настоящий момент данная опция недоступна.",

  "Смена языка",
  "Русский",
  "Английский",

  "О продукте",
  "Данную программу разработала команда\nМедная капля при поддержке RealLab\nна хакатоне Cyber Garden Hardware 2025."
};

const Strings STR_EN = {
  "Main menu",
  "Radio signal",
  "Infrared port",
  "Language",
  "About",

  "Radio signal",
  "Read temperature",
  "Read pressure",
  "Back",

  "This option is not available at the moment.",

  "Language",
  "Russian",
  "English",

  "About",
  "This program was created by the Copper Drop team\nwith support from RealLab at Cyber Garden\nHardware 2025."
};

inline const Strings& T() { return (lang == RU) ? STR_RU : STR_EN; }

/***** ЭКРАНЫ *****/
enum Screen { SCR_MAIN, SCR_RADIO, SCR_IR, SCR_LANG, SCR_ABOUT };
Screen scr = SCR_MAIN;

/***** ГЕОМЕТРИЯ КНОПОК *****/
const int16_t BTN_X = 10, BTN_W = 300, BTN_H = 35, BTN_GAP = 10;
const int16_t FIRST_Y = 50;
const int16_t BACK_X=10, BACK_Y=200, BACK_W=65, BACK_H=34;

/***** РАДИО ПРИЁМ (433 МГц) *****/
// Используем D13 как вход  приёмника
RH_ASK rf(2000, 13, 12, 11);   // (скорость, rxPin=D13, txPin, pttPin)

/***** ДАННЫЕ *****/
char g_temp[10]  = "--";     // последнее полученное значение T (строка)
char g_press[12] = "--";     // последнее полученное значение P (строка)

enum WaitWhat { NONE, WAIT_TEMP, WAIT_PRESS };
WaitWhat waiting = NONE;
unsigned long waitUntil = 0;     // дедлайн ожидания (millis()+10000)

/***** УТИЛИТЫ ГРАФИКИ *****/
void drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const char* label) {
  tft.drawRect(x, y, w, h, BLUE);
  u8g2.setForegroundColor(BLACK);
  u8g2.setBackgroundColor(WHITE);
  u8g2.setCursor(x + 10, y + h/2 + 5);
  u8g2.print(label);
}

void drawTitle(const char* title) {
  tft.fillScreen(WHITE);
  u8g2.setForegroundColor(BLACK);
  u8g2.setBackgroundColor(WHITE);
  tft.drawRect(6, 8, 308, 28, BLUE);
  u8g2.setCursor(20, 28);
  u8g2.print(title);
}

// БАЗОВАЯ печать значения справа в строке
void drawRightValue(int16_t lineIndex, const char* val) {
  int16_t y = FIRST_Y + lineIndex*(BTN_H+BTN_GAP);
  tft.fillRect(BTN_X+150, y+1, BTN_W-151, BTN_H-2, WHITE);
  u8g2.setForegroundColor(BLACK);
  u8g2.setBackgroundColor(WHITE);
  u8g2.setCursor(BTN_X+160, y + BTN_H/2 + 5);
  u8g2.print(val);
}

// Печать температуры с единицей (строка 1)
void drawRightTemp(const char* val) {
  int16_t y = FIRST_Y + 0*(BTN_H+BTN_GAP);
  tft.fillRect(BTN_X+150, y+1, BTN_W-151, BTN_H-2, WHITE);
  u8g2.setForegroundColor(BLACK);
  u8g2.setBackgroundColor(WHITE);
  u8g2.setCursor(BTN_X+160, y + BTN_H/2 + 5);
  u8g2.print(val);
  u8g2.print(" ");
  u8g2.print("\xC2\xB0");
  u8g2.print("C");
}

// Печать давления с единицей ( строкф 2)
void drawRightPress(const char* val) {
  int16_t y = FIRST_Y + 1*(BTN_H+BTN_GAP);
  tft.fillRect(BTN_X+150, y+1, BTN_W-151, BTN_H-2, WHITE);
  u8g2.setForegroundColor(BLACK);
  u8g2.setBackgroundColor(WHITE);
  u8g2.setCursor(BTN_X+160, y + BTN_H/2 + 5);
  u8g2.print(val);
  u8g2.print(" ");
  if (lang == RU) u8g2.print("гПа");
  else            u8g2.print("hPa");
}

/***** ЭКРАНЫ: ОТРИСОВКА *****/
void showMain() {
  drawTitle(T().title_main);
  drawButton(BTN_X, FIRST_Y + 0*(BTN_H+BTN_GAP), BTN_W, BTN_H, T().b_radio);
  drawButton(BTN_X, FIRST_Y + 1*(BTN_H+BTN_GAP), BTN_W, BTN_H, T().b_ir);
  drawButton(BTN_X, FIRST_Y + 2*(BTN_H+BTN_GAP), BTN_W, BTN_H, T().b_lang);
  drawButton(BTN_X, FIRST_Y + 3*(BTN_H+BTN_GAP), BTN_W, BTN_H, T().b_about);
}

void showRadioMenu() {
  drawTitle(T().title_radio);
  drawButton(BTN_X, FIRST_Y + 0*(BTN_H+BTN_GAP), BTN_W, BTN_H, T().r_temp);
  drawButton(BTN_X, FIRST_Y + 1*(BTN_H+BTN_GAP), BTN_W, BTN_H, T().r_press);
  drawButton(BACK_X, BACK_Y, BACK_W, BACK_H, T().b_back);
  // Подпишем текущие значения (если есть) — С ЕДИНИЦАМИ
  drawRightTemp(g_temp);
  drawRightPress(g_press);
}

void showIRUnavailable() {
  drawTitle(T().b_ir);
  u8g2.setCursor(14, 70);
  u8g2.print(T().ir_msg);
  drawButton(BACK_X, BACK_Y, BACK_W, BACK_H, T().b_back);
}

void showLangMenu() {
  drawTitle(T().title_lang);
  drawButton(BTN_X, FIRST_Y + 0*(BTN_H+BTN_GAP), BTN_W, BTN_H, T().b_ru);
  drawButton(BTN_X, FIRST_Y + 1*(BTN_H+BTN_GAP), BTN_W, BTN_H, T().b_en);
  drawButton(BACK_X, BACK_Y, BACK_W, BACK_H, T().b_back);
}

void showAbout() {
  drawTitle(T().title_about);
  u8g2.setCursor(14, 70);
  u8g2.print(T().about_text);
  drawButton(BACK_X, BACK_Y, BACK_W, BACK_H, T().b_back);
}

/***** ТАЧ: ПИКСЕЛИ *****/
bool readTouch(int16_t &sx, int16_t &sy) {
  TSPoint p = ts.getPoint();
  pinMode(XM, OUTPUT); pinMode(YP, OUTPUT);
  if (p.z < MINPRESSURE || p.z > MAXPRESSURE) return false;
  int16_t x = map(p.y, TS_LEFT, TS_RT, 0, tft.width());
  int16_t y = map(p.x, TS_TOP,  TS_BOT, 0, tft.height());
  sx = constrain(x, 0, tft.width()-1);
  sy = constrain(y, 0, tft.height()-1);
  return true;
}

/***** ХИТ-ТЕСТЫ *****/
bool inRect(int16_t x,int16_t y,int16_t w,int16_t h,int16_t px,int16_t py){
  return (px>=x && px<=x+w && py>=y && py<=y+h);
}
int8_t hitMain(int16_t px,int16_t py){
  for (uint8_t i=0;i<4;i++){
    int16_t y = FIRST_Y + i*(BTN_H+BTN_GAP);
    if (inRect(BTN_X, y, BTN_W, BTN_H, px, py)) return i;
  }
  return -1;
}
int8_t hitRadio(int16_t px,int16_t py){
  if (inRect(BTN_X, FIRST_Y, BTN_W, BTN_H, px, py)) return 0;                          // temp
  if (inRect(BTN_X, FIRST_Y+BTN_H+BTN_GAP, BTN_W, BTN_H, px, py)) return 1;            // press
  if (inRect(BACK_X,BACK_Y,BACK_W,BACK_H,px,py)) return 2;                             // back
  return -1;
}
int8_t hitIR(int16_t px,int16_t py){ if (inRect(BACK_X,BACK_Y,BACK_W,BACK_H,px,py)) return 0; return -1; }
int8_t hitLang(int16_t px,int16_t py){
  if (inRect(BTN_X, FIRST_Y, BTN_W, BTN_H, px, py)) return 0;
  if (inRect(BTN_X, FIRST_Y+BTN_H+BTN_GAP, BTN_W, BTN_H, px, py)) return 1;
  if (inRect(BACK_X,BACK_Y,BACK_W,BACK_H,px,py)) return 2;
  return -1;
}
int8_t hitAbout(int16_t px,int16_t py){ if (inRect(BACK_X,BACK_Y,BACK_W,BACK_H,px,py)) return 0; return -1; }

/***** РАДИО: ПАРСЕР КАДРА "T:xx.xx;P:yyyyyy" *****/
void radioPollAndParse() {
  uint8_t buf[32];       // буфер приёма
  uint8_t buflen = sizeof(buf);
  if (rf.available()) {
    if (rf.recv(buf, &buflen)) {
      if (buflen >= sizeof(buf)) buflen = sizeof(buf)-1;
      buf[buflen] = 0;

      // Ищем подстроки "T:" и "P:"
      char *tc = strstr((char*)buf, "T:");
      char *pc = strstr((char*)buf, "P:");
      if (tc) {
        tc += 2; // после "T:"
        char *sc = strchr(tc, ';'); if (!sc) sc = (char*)buf + buflen;
        size_t n = min((size_t)9, (size_t)(sc - tc));
        strncpy(g_temp, tc, n); g_temp[n] = 0;
      }
      if (pc) {
        pc += 2; // после "P:"
        size_t n = min((size_t)11, strlen(pc));
        strncpy(g_press, pc, n); g_press[n] = 0;
      }

      // Обновим экран, если мы на Radio-меню
      if (scr == SCR_RADIO) {
        if (waiting == WAIT_TEMP)  { drawRightTemp(g_temp);   waiting = NONE; }
        if (waiting == WAIT_PRESS) { drawRightPress(g_press); waiting = NONE; }
      }
    }
  }
}

/***** SETUP *****/
void setup() {
  // MCUFRIEND дисплей
  tft.reset();
  uint16_t id = tft.readID(); if (id == 0xD3D3) id = 0x9486;
  tft.begin(id);
  tft.setRotation(1);

  // Отключаем SD (если есть на шилде), чтобы не мешал линии D13
  pinMode(10, OUTPUT); digitalWrite(10, HIGH);

  // U8g2 поверх GFX
  u8g2.begin(tft);
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);

  // Радио приём (DATA на D13)
  rf.init(); // если false — проверь проводку и питание приёмника

  scr = SCR_MAIN;
  showMain();
}

/***** LOOP *****/
void loop() {
  // Поллим радио 
  radioPollAndParse();

  // Если есть активное ожидание и вышел таймаут — показать error
  if (waiting != NONE && millis() > waitUntil) {
    if (scr == SCR_RADIO) {
      if (waiting == WAIT_TEMP)  drawRightTemp("...");
      if (waiting == WAIT_PRESS) drawRightPress("...");
      // Покажем, что не дождались (можно заменить на "error")
      drawRightValue((waiting == WAIT_TEMP) ? 0 : 1, "error");
    }
    waiting = NONE;
  }

  // Тач-управление
  int16_t x, y;
  if (!readTouch(x, y)) return;

  switch (scr) {
    case SCR_MAIN: {
      int8_t h = hitMain(x, y);
      if (h == 0)      { scr = SCR_RADIO; showRadioMenu(); }
      else if (h == 1) { scr = SCR_IR;    showIRUnavailable(); }
      else if (h == 2) { scr = SCR_LANG;  showLangMenu(); }
      else if (h == 3) { scr = SCR_ABOUT; showAbout(); }
    } break;

    case SCR_RADIO: {
      int8_t h = hitRadio(x, y);
      if (h == 0) {
        // Ждём кадр с температурой до 10 секунд, показываем плейсхолдер с единицей
        drawRightTemp("...");
        waiting = WAIT_TEMP;  waitUntil = millis()+10000UL;
      } else if (h == 1) {
        drawRightPress("...");
        waiting = WAIT_PRESS; waitUntil = millis()+10000UL;
      } else if (h == 2) {
        waiting = NONE;  scr = SCR_MAIN; showMain();
      }
    } break;

    case SCR_IR: {
      if (hitIR(x, y) == 0) { scr = SCR_MAIN; showMain(); }
    } break;

    case SCR_LANG: {
      int8_t h = hitLang(x, y);
      if (h == 0) { lang = RU;  scr = SCR_MAIN; showMain(); }
      else if (h == 1) { lang = EN;  scr = SCR_MAIN; showMain(); }
      else if (h == 2) { scr = SCR_MAIN; showMain(); }
    } break;

    case SCR_ABOUT: {
      if (hitAbout(x, y) == 0) { scr = SCR_MAIN; showMain(); }
    } break;
  }

  delay(160); // антидребезг
}
