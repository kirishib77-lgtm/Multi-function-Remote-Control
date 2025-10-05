#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <RH_ASK.h>
#include <SPI.h>

Adafruit_BMP085 bmp;

RH_ASK driver(2000, 27, 2, 255);   // (бит/с, rxPin, txPin, pttPin)

// Буфер для передачи (RadioHead ограничение +-27 байт по умолчанию)
char txbuf[30];

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(21, 22);

  // Инициализация BMP180
  if (!bmp.begin()) {
    Serial.println(F("BMP180 not found! Check wiring."));
    while (1) { delay(1000); }
  }
  Serial.println(F("BMP180 OK"));

  // Инициализация радио (FS1000A на GPIO2)
  if (!driver.init()) {
    Serial.println(F("RH_ASK init failed"));
    while (1) { delay(1000); }
  }
  Serial.println(F("RH_ASK @2000 baud, TX=GPIO2 ready"));
}

void loop() {
  // Считываем данные с BMP180
  float tC = bmp.readTemperature();         // Градусы Цельсия
  long pPa  = bmp.readPressure();           // Давление в Паскалях

  snprintf(txbuf, sizeof(txbuf), "T:%.2f;P:%ld", tC, pPa);

  // Передаём
  driver.send((uint8_t*)txbuf, (uint8_t)strlen(txbuf));
  driver.waitPacketSent();

  // Отладка в Serial
  Serial.print(F("TX: ")); Serial.println(txbuf);

  delay(1000);
}