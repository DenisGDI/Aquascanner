#include <Arduino.h>
void setup() {
  // Инициализируем пин встроенного светодиода как выход
  // LED_BUILTIN на Wemos D1 mini соответствует D4 (GPIO2)
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, LOW);   // LOW = включить (активный низкий уровень) [citation:10]
  delay(500);                       // Ждём 500 миллисекунд
  digitalWrite(LED_BUILTIN, HIGH);  // HIGH = выключить
  delay(500);                       // Ждём 500 миллисекунд
}