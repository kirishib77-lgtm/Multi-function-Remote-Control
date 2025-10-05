#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <RH_ASK.h>
#include <SPI.h>
#include <Stepper.h>

Adafruit_BMP085 bmp;

RH_ASK driver(2000, 27, 2, 255);

/* ---------- ШАГОВИК 28BYJ-48 + ULN2003 ---------- */
constexpr int IN1 = 5;    // ваша распиновка
constexpr int IN2 = 18;
constexpr int IN3 = 19;
constexpr int IN4 = 23;

constexpr int STEPS_PER_REV = 2048;
Stepper stepperMotor(STEPS_PER_REV, IN1, IN3, IN2, IN4); // порядок пинов важен!

/* ---------- ПАРАМЕТРЫ ---------- */
constexpr float T_THRESHOLD = 31.0;          // порог °C
constexpr unsigned long MOTOR_RUN_MS = 30000; // 30 сек
constexpr unsigned long TX_PERIOD_MS  = 1000; // раз в секунду
constexpr unsigned long REARM_DELAY_MS = 3000; // пауза перед новым запуском (защита от дребезга)

/* ---------- СОСТОЯНИЕ ---------- */
bool motorActive = false;            // крутится сейчас?
bool armed = true;                   // разрешён запуск (блокируется на REARM_DELAY_MS после остановки)
unsigned long tMotorUntil = 0;       // когда остановить мотор
unsigned long tNextTX = 0;           // когда отправлять следующий кадр

/* ---------- ПЕРЕДАЧА ---------- */
char txbuf[30];

void setup() {
  Serial.begin(115200);
  delay(100);

  Wire.begin(21, 22); // SDA, SCL

  if (!bmp.begin()) {
    Serial.println(F("BMP180 not found! Check wiring."));
    while (true) { delay(1000); }
  }
  Serial.println(F("BMP180 OK"));

  if (!driver.init()) {
    Serial.println(F("RH_ASK init failed"));
    while (true) { delay(1000); }
  }
  Serial.println(F("RH_ASK @2000 baud, TX=GPIO2 ready"));

  // скорость шаговика (об/мин). 10–15 RPM — тихо и надёжно.
  stepperMotor.setSpeed(12);

  // На всякий случай явно конфигурируем пины шаговика как выходы:
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
}

void loop() {
  const unsigned long now = millis();

  /* ---- ПЕРЕДАЧА T/P РАЗ В СЕКУНДУ ---- */
  if (now >= tNextTX) {
    tNextTX = now + TX_PERIOD_MS;

    const float tC = bmp.readTemperature();  // °C
    const long  pPa = bmp.readPressure();    // Па

    // Формат, который ждёт UNO
    snprintf(txbuf, sizeof(txbuf), "T:%.2f;P:%ld", tC, pPa);
    driver.send((uint8_t*)txbuf, (uint8_t)strlen(txbuf));
    driver.waitPacketSent();

    Serial.print(F("TX: ")); Serial.println(txbuf);

    // Условие старта мотора
    if (armed && !motorActive && tC >= T_THRESHOLD) {
      motorActive = true;
      armed = false;                       // запретить немедленный перезапуск
      tMotorUntil = now + MOTOR_RUN_MS;
      Serial.println(F("MOTOR: START 30s (T >= 31C)"));
    }
  }

  /* ---- ВРАЩЕНИЕ МОТОРА ----
  if (motorActive) {
    stepperMotor.step(1);                  // +1 шаг; смените на -1 для обратного направления

    if ((long)(now - tMotorUntil) >= 0) {  // корректно с переполнением millis()
      motorActive = false;
      Serial.println(F("MOTOR: STOP"));

      tMotorUntil = now + REARM_DELAY_MS;  // временно переиспользуем переменную
    }
  } else if (!armed) {
    // Ждём, когда закончится «перезарядка» триггера
    if ((long)(now - tMotorUntil) >= 0) armed = true;
  }
}

