#include <Arduino.h>
#include <Servo.h>

Servo myservo;
int currentAngle = 0;
int targetAngle = 0;

void setup() {
  myservo.attach(9);
  myservo.write(0);
  delay(300); // Даем серве время инициализироваться
}

void loop() {
  // Медленное движение с небольшими шагами
  for (int angle = 0; angle <= 180; angle += 2) { // Шаг 2 градуса
    myservo.write(angle);
    delay(30); // Большая задержка = плавнее движение
  }
  
  delay(100); // Пауза в крайних точках
  
  for (int angle = 180; angle >= 0; angle -= 2) {
    myservo.write(angle);
    delay(30);
  }
  
  delay(100);
}
