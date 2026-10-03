#include <Arduino.h>
// Arduino Uno — сбор данных с 4 датчиков
// Отправляет только подключённые датчики, остальные помечает как "--"
// Формат: DATA,мутность,pH,TDS,температура
// Например: DATA,15.3,7.02,--,24.5  (TDS не подключён)

#define TURBIDITY_PIN  A1
#define PH_PIN         A2
#define TDS_PIN        A3
#define THERMISTOR_PIN A0

#define THERMISTOR_NOMINAL 10000
#define TEMPERATURE_NOMINAL 25
#define B_COEFFICIENT 3950
#define SERIES_RESISTOR 10000

// --- Проверка: датчик подключён? ---
// Делаем 5 замеров, смотрим разброс.
// Если разброс большой — вход "плавает", датчик не подключён.
bool isSensorConnected(int pin) {
  int minVal = 1023, maxVal = 0;
  for (int i = 0; i < 5; i++) {
    int v = analogRead(pin);
    if (v < minVal) minVal = v;
    if (v > maxVal) maxVal = v;
    delay(2);
  }
  // Если разброс больше 50 — это шум, датчик не подключён
  return (maxVal - minVal) < 50;
}

// --- Термистор ---
float readTemperature() {
  int adc = analogRead(THERMISTOR_PIN);
  if (adc == 0 || adc >= 1023) return -999;
  float resistance = SERIES_RESISTOR * (1023.0 / adc - 1.0);
  float steinhart = resistance / THERMISTOR_NOMINAL;
  steinhart = log(steinhart);
  steinhart /= B_COEFFICIENT;
  steinhart += 1.0 / (TEMPERATURE_NOMINAL + 273.15);
  steinhart = 1.0 / steinhart;
  steinhart -= 273.15;
  return steinhart;
}

// --- Мутность ---
float readTurbidity() {
  int adc = analogRead(TURBIDITY_PIN);
  float voltage = adc * (5.0 / 1023.0);
  float ntu = -1120.4 * voltage * voltage + 5742.3 * voltage - 4352.9;
  if (ntu < 0) ntu = 0;
  return ntu;
}

// --- pH ---
float readPH() {
  int adc = analogRead(PH_PIN);
  float voltage = adc * (5.0 / 1023.0);
  float ph = 7.0 + ((2.5 - voltage) / 0.18);
  return ph;
}

// --- TDS ---
float readTDS(float temperature) {
  int adc = analogRead(TDS_PIN);
  float voltage = adc * (5.0 / 1023.0);
  float compensation = 1.0 + 0.02 * (temperature - 25.0);
  float vComp = voltage / compensation;
  float tds = (133.42 * vComp * vComp * vComp 
             - 255.86 * vComp * vComp 
             + 857.39 * vComp) * 0.5;
  if (tds < 0) tds = 0;
  return tds;
}

void setup() {
  Serial.begin(9600);
  delay(500);
}

void loop() {
  // Проверяем подключение датчиков
  bool turbOK = isSensorConnected(TURBIDITY_PIN);
  bool phOK   = isSensorConnected(PH_PIN);
  bool tdsOK  = isSensorConnected(TDS_PIN);
  bool tempOK = isSensorConnected(THERMISTOR_PIN);
  
  // Читаем значения
  float temp = tempOK ? readTemperature() : -999;
  float turb = turbOK ? readTurbidity()   : -1;
  float ph   = phOK   ? readPH()          : -1;
  float tds  = tdsOK  ? readTDS(temp)     : -1;
  
  // Формируем строку
  Serial.print("DATA,");
  
  if (turbOK) { Serial.print(turb, 1); } else { Serial.print("--"); }
  Serial.print(",");
  
  if (phOK) { Serial.print(ph, 2); } else { Serial.print("--"); }
  Serial.print(",");
  
  if (tdsOK) { Serial.print(tds, 0); } else { Serial.print("--"); }
  Serial.print(",");
  
  if (tempOK && temp > -100) { Serial.println(temp, 1); } else { Serial.println("--"); }
  
  delay(1000);
}