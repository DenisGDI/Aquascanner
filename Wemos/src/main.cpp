#include <Arduino.h>
/*
 * ============================================================
 *   ПОЛНОЦЕННОЕ УПРАВЛЕНИЕ КВАДРОКОПТЕРОМ
 *   ПЛАТА: Wemos D1 Mini (ESP8266)
 *   ДРАЙВЕРЫ: TA6586 (4 шт.)
 *   УПРАВЛЕНИЕ: ГАЗ + КРЕН + ТАНГАЖ + РЫСКАНИЕ
 * ============================================================
 * 
 * ПОДКЛЮЧЕНИЕ МОТОРОВ:
 * Мотор 1 (CW):  FI=D2, BI=D1  (передний левый)
 * Мотор 2 (CCW): FI=D4, BI=D3  (передний правый)
 * Мотор 3 (CW):  FI=D6, BI=D7  (задний правый)
 * Мотор 4 (CCW): FI=D0, BI=D5  (задний левый)
 * 
 * УПРАВЛЕНИЕ:
 * - ГАЗ: ползунок (общая тяга)
 * - КРЕН: джойстик влево/вправо
 * - ТАНГАЖ: джойстик вверх/вниз
 * - РЫСКАНИЕ: (опционально, пока закомментировано)
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ============================================================
// 1. НАСТРОЙКИ Wi-Fi
// ============================================================

const char* ssid = "Drone_Control";
const char* password = "12345678";
ESP8266WebServer server(80);

// ============================================================
// 2. ПИНЫ МОТОРОВ
// ============================================================

#define FI1 D2
#define BI1 D1
#define FI2 D4
#define BI2 D3
#define FI3 D6
#define BI3 D7
#define FI4 D0
#define BI4 D5

// ============================================================
// 3. ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================

int throttle = 0;      // 0..100
int roll = 0;          // -100..100
int pitch = 0;         // -100..100
int yaw = 0;           // -100..100

int motor1Speed = 0;
int motor2Speed = 0;
int motor3Speed = 0;
int motor4Speed = 0;

unsigned long lastClientTime = 0;
bool emergencyStopFlag = false;

struct Motor {
  int pinFI;
  int pinBI;
  bool isCW;
  String name;
};

Motor motors[4] = {
  {FI1, BI1, true,  "Мотор 1 (CW)"},
  {FI2, BI2, false, "Мотор 2 (CCW)"},
  {FI3, BI3, true,  "Мотор 3 (CW)"},
  {FI4, BI4, false, "Мотор 4 (CCW)"}
};

// ============================================================
// 4. ПРОТОТИПЫ ФУНКЦИЙ (ОБЪЯВЛЕНИЯ)  <--- ОШИБКА ИСПРАВЛЕНА!
// ============================================================

void setMotor(int index, int speed);
void stopAllMotors();
void calculateMotorSpeeds();
void applyMotorSpeeds();
void handleRoot();
void handleControl();
void handleStatus();
String getHTMLPage();   // <--- ПРОТОТИП ДЛЯ HTML

// ============================================================
// 5. ФУНКЦИИ УПРАВЛЕНИЯ МОТОРАМИ
// ============================================================

void setMotor(int index, int speed) {
  if (index < 0 || index > 3) return;
  if (speed < 0) speed = 0;
  if (speed > 1023) speed = 1023;
  
  if (speed == 0) {
    digitalWrite(motors[index].pinFI, LOW);
    analogWrite(motors[index].pinBI, 0);
  } else if (motors[index].isCW) {
    digitalWrite(motors[index].pinFI, HIGH);
    analogWrite(motors[index].pinBI, speed);
  } else {
    digitalWrite(motors[index].pinFI, LOW);
    analogWrite(motors[index].pinBI, speed);
  }
}

void stopAllMotors() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(motors[i].pinFI, LOW);
    analogWrite(motors[i].pinBI, 0);
  }
  motor1Speed = motor2Speed = motor3Speed = motor4Speed = 0;
  emergencyStopFlag = false;
}

void calculateMotorSpeeds() {
  int baseSpeed = map(throttle, 0, 100, 0, 800);
  
  int rollEffect = map(abs(roll), 0, 100, 0, 200);
  if (roll < 0) rollEffect = -rollEffect;
  
  int pitchEffect = map(abs(pitch), 0, 100, 0, 200);
  if (pitch < 0) pitchEffect = -pitchEffect;
  
  int yawEffect = map(abs(yaw), 0, 100, 0, 150);
  if (yaw < 0) yawEffect = -yawEffect;
  
  motor1Speed = baseSpeed - rollEffect + pitchEffect - yawEffect;
  motor2Speed = baseSpeed + rollEffect + pitchEffect + yawEffect;
  motor3Speed = baseSpeed + rollEffect - pitchEffect - yawEffect;
  motor4Speed = baseSpeed - rollEffect - pitchEffect + yawEffect;
  
  motor1Speed = constrain(motor1Speed, 0, 1023);
  motor2Speed = constrain(motor2Speed, 0, 1023);
  motor3Speed = constrain(motor3Speed, 0, 1023);
  motor4Speed = constrain(motor4Speed, 0, 1023);
  
  int minSpeed = 30;
  if (throttle > 0) {
    if (motor1Speed > 0 && motor1Speed < minSpeed) motor1Speed = minSpeed;
    if (motor2Speed > 0 && motor2Speed < minSpeed) motor2Speed = minSpeed;
    if (motor3Speed > 0 && motor3Speed < minSpeed) motor3Speed = minSpeed;
    if (motor4Speed > 0 && motor4Speed < minSpeed) motor4Speed = minSpeed;
  }
}

void applyMotorSpeeds() {
  if (emergencyStopFlag || throttle == 0) {
    stopAllMotors();
    return;
  }
  
  calculateMotorSpeeds();
  
  setMotor(0, motor1Speed);
  setMotor(1, motor2Speed);
  setMotor(2, motor3Speed);
  setMotor(3, motor4Speed);
}

// ============================================================
// 6. ВЕБ-ОБРАБОТЧИКИ
// ============================================================

void handleRoot() {
  server.send(200, "text/html", getHTMLPage());  // <--- getHTMLPage() известна!
}

void handleControl() {
  lastClientTime = millis();
  
  String action = server.arg("action");
  
  if (action == "stop") {
    emergencyStopFlag = true;
    stopAllMotors();
    server.send(200, "text/plain", "STOPPED");
    Serial.println("АВАРИЙНАЯ ОСТАНОВКА!");
    return;
  }
  
  if (server.hasArg("throttle")) {
    throttle = server.arg("throttle").toInt();
    if (throttle < 0) throttle = 0;
    if (throttle > 100) throttle = 100;
  }
  
  if (server.hasArg("roll")) {
    roll = server.arg("roll").toInt();
    if (roll < -100) roll = -100;
    if (roll > 100) roll = 100;
  }
  
  if (server.hasArg("pitch")) {
    pitch = server.arg("pitch").toInt();
    if (pitch < -100) pitch = -100;
    if (pitch > 100) pitch = 100;
  }
  
  if (server.hasArg("yaw")) {
    yaw = server.arg("yaw").toInt();
    if (yaw < -100) yaw = -100;
    if (yaw > 100) yaw = 100;
  }
  
  if (emergencyStopFlag) {
    emergencyStopFlag = false;
  }
  
  applyMotorSpeeds();
  
  server.send(200, "text/plain", "OK");
  
  if (throttle > 0) {
    Serial.print("T:");
    Serial.print(throttle);
    Serial.print(" R:");
    Serial.print(roll);
    Serial.print(" P:");
    Serial.print(pitch);
    Serial.print(" Y:");
    Serial.print(yaw);
    Serial.print(" | M1:");
    Serial.print(motor1Speed);
    Serial.print(" M2:");
    Serial.print(motor2Speed);
    Serial.print(" M3:");
    Serial.print(motor3Speed);
    Serial.print(" M4:");
    Serial.println(motor4Speed);
  }
}

void handleStatus() {
  lastClientTime = millis();
  
  String json = "{";
  json += "\"throttle\":" + String(throttle) + ",";
  json += "\"roll\":" + String(roll) + ",";
  json += "\"pitch\":" + String(pitch) + ",";
  json += "\"yaw\":" + String(yaw) + ",";
  json += "\"m1\":" + String(motor1Speed) + ",";
  json += "\"m2\":" + String(motor2Speed) + ",";
  json += "\"m3\":" + String(motor3Speed) + ",";
  json += "\"m4\":" + String(motor4Speed) + ",";
  json += "\"emergency\":" + String(emergencyStopFlag ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

// ============================================================
// 7. ГЕНЕРАЦИЯ HTML-СТРАНИЦЫ (ОПРЕДЕЛЕНИЕ)
// ============================================================

String getHTMLPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>Дрон - Полноценное управление</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; user-select: none; }
    body {
      font-family: 'Segoe UI', Arial, sans-serif;
      background: #0a0a1a;
      color: #eee;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      padding: 10px;
      touch-action: none;
    }
    .container {
      background: #12122a;
      border-radius: 20px;
      padding: 20px 25px;
      max-width: 500px;
      width: 100%;
      box-shadow: 0 10px 40px rgba(0,0,0,0.8);
    }
    h1 {
      font-size: 22px;
      text-align: center;
      color: #00d2ff;
      margin-bottom: 5px;
    }
    .subtitle {
      text-align: center;
      font-size: 12px;
      color: #666;
      margin-bottom: 20px;
    }
    .joystick-container {
      display: flex;
      justify-content: center;
      gap: 20px;
      margin-bottom: 20px;
      flex-wrap: wrap;
    }
    .joystick-wrapper {
      text-align: center;
    }
    .joystick-label {
      font-size: 12px;
      color: #888;
      margin-bottom: 5px;
    }
    .joystick {
      width: 140px;
      height: 140px;
      border-radius: 50%;
      background: radial-gradient(circle, #1a1a3a, #0d0d1a);
      border: 2px solid #333;
      position: relative;
      touch-action: none;
      box-shadow: inset 0 0 30px rgba(0,0,0,0.5);
    }
    .joystick .knob {
      width: 50px;
      height: 50px;
      border-radius: 50%;
      background: radial-gradient(circle, #00d2ff, #0066aa);
      position: absolute;
      top: 50%;
      left: 50%;
      transform: translate(-50%, -50%);
      pointer-events: none;
      box-shadow: 0 0 30px rgba(0,210,255,0.3);
      border: 2px solid rgba(255,255,255,0.2);
    }
    .joystick .crosshair {
      position: absolute;
      top: 50%;
      left: 50%;
      width: 80%;
      height: 80%;
      transform: translate(-50%, -50%);
      pointer-events: none;
    }
    .joystick .crosshair::before,
    .joystick .crosshair::after {
      content: '';
      position: absolute;
      background: rgba(255,255,255,0.05);
    }
    .joystick .crosshair::before {
      width: 2px;
      height: 100%;
      left: 50%;
      transform: translateX(-50%);
    }
    .joystick .crosshair::after {
      width: 100%;
      height: 2px;
      top: 50%;
      transform: translateY(-50%);
    }
    .throttle-container {
      margin: 15px 0 20px 0;
    }
    .throttle-row {
      display: flex;
      align-items: center;
      gap: 15px;
    }
    .throttle-row label {
      font-size: 14px;
      color: #aaa;
      min-width: 50px;
    }
    input[type="range"] {
      flex: 1;
      -webkit-appearance: none;
      appearance: none;
      height: 8px;
      border-radius: 4px;
      background: linear-gradient(to right, #2d2d44, #ff6b6b);
      outline: none;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 24px;
      height: 24px;
      border-radius: 50%;
      background: #ff6b6b;
      cursor: pointer;
      border: 2px solid #fff;
      box-shadow: 0 0 20px rgba(255,107,107,0.3);
    }
    .throttle-value {
      font-size: 18px;
      font-weight: bold;
      min-width: 40px;
      text-align: center;
      color: #ff6b6b;
    }
    .button-row {
      display: flex;
      gap: 10px;
      justify-content: center;
      margin: 15px 0 10px 0;
      flex-wrap: wrap;
    }
    .btn {
      padding: 12px 25px;
      border: none;
      border-radius: 10px;
      font-size: 16px;
      font-weight: bold;
      cursor: pointer;
      transition: 0.3s;
    }
    .btn-stop {
      background: #ff6b6b;
      color: #fff;
      box-shadow: 0 4px 15px rgba(255,107,107,0.3);
      flex: 1;
      min-width: 100px;
    }
    .btn-stop:hover {
      transform: scale(1.02);
      background: #ff5252;
    }
    .btn-reset {
      background: #2d2d44;
      color: #aaa;
      flex: 1;
      min-width: 100px;
    }
    .btn-reset:hover {
      background: #3d3d5a;
      color: #fff;
    }
    .status {
      font-size: 12px;
      color: #666;
      padding: 8px;
      background: #0d0d1a;
      border-radius: 8px;
      margin-top: 10px;
      text-align: center;
      font-family: monospace;
    }
    .status .green { color: #4caf50; }
    .status .red { color: #ff6b6b; }
    .status .blue { color: #00d2ff; }
    .status .orange { color: #ff9800; }
    .warning {
      font-size: 11px;
      color: #ff6b6b;
      padding: 8px;
      background: rgba(255,107,107,0.08);
      border-radius: 6px;
      margin-top: 8px;
      text-align: center;
    }
    .footer {
      text-align: center;
      font-size: 10px;
      color: #333;
      margin-top: 10px;
    }
  </style>
</head>
<body>
<div class="container">
  <h1>🚁 Квадрокоптер</h1>
  <div class="subtitle">Джойстик + ГАЗ</div>
  
  <div class="joystick-container">
    <div class="joystick-wrapper">
      <div class="joystick-label">🔄 КРЕН / ТАНГАЖ</div>
      <div class="joystick" id="joystick">
        <div class="crosshair"></div>
        <div class="knob" id="knob"></div>
      </div>
    </div>
  </div>
  
  <div class="throttle-container">
    <div class="throttle-row">
      <label>⬆ ГАЗ</label>
      <input type="range" id="throttle" min="0" max="100" value="0" step="1">
      <span class="throttle-value" id="throttleDisplay">0</span>
    </div>
  </div>
  
  <div class="button-row">
    <button class="btn btn-stop" id="stopBtn">🛑 СТОП</button>
    <button class="btn btn-reset" id="resetBtn">↺ Сброс</button>
  </div>
  
  <div class="status" id="status">
    ⚡ <span class="orange">Ожидание</span> | 
    ГАЗ: <span class="blue" id="statusThrottle">0</span> |
    КРЕН: <span class="blue" id="statusRoll">0</span> |
    ТАНГАЖ: <span class="blue" id="statusPitch">0</span>
  </div>
  <div class="warning" id="warning"></div>
  <div class="footer">Wemos D1 Mini | TA6586 | v2.0</div>
</div>

<script>
  let throttle = 0;
  let roll = 0;
  let pitch = 0;
  let yaw = 0;
  
  const joystick = document.getElementById('joystick');
  const knob = document.getElementById('knob');
  const throttleSlider = document.getElementById('throttle');
  const throttleDisplay = document.getElementById('throttleDisplay');
  const stopBtn = document.getElementById('stopBtn');
  const resetBtn = document.getElementById('resetBtn');
  const statusEl = document.getElementById('status');
  const warningEl = document.getElementById('warning');
  
  let isDragging = false;
  let lastSendTime = 0;
  
  function getJoystickCoords(e) {
    const touch = e.touches ? e.touches[0] : e;
    const rect = joystick.getBoundingClientRect();
    const cx = rect.left + rect.width / 2;
    const cy = rect.top + rect.height / 2;
    const maxR = rect.width / 2 - 25;
    
    let dx = touch.clientX - cx;
    let dy = touch.clientY - cy;
    const dist = Math.sqrt(dx*dx + dy*dy);
    
    if (dist > maxR) {
      dx = dx / dist * maxR;
      dy = dy / dist * maxR;
    }
    
    return { x: dx / maxR, y: -dy / maxR, dx: dx, dy: -dy };
  }
  
  function updateJoystick(x, y) {
    const maxR = joystick.offsetWidth / 2 - 25;
    knob.style.transform = `translate(calc(-50% + ${x * maxR}px), calc(-50% + ${-y * maxR}px))`;
  }
  
  function handleJoystickStart(e) {
    e.preventDefault();
    isDragging = true;
    handleJoystickMove(e);
  }
  
  function handleJoystickMove(e) {
    e.preventDefault();
    if (!isDragging) return;
    
    const coords = getJoystickCoords(e);
    const x = coords.x;
    const y = coords.y;
    
    const deadZone = 0.05;
    let finalX = Math.abs(x) < deadZone ? 0 : x;
    let finalY = Math.abs(y) < deadZone ? 0 : y;
    
    roll = Math.round(finalX * 100);
    pitch = Math.round(finalY * 100);
    
    updateJoystick(finalX, finalY);
    sendControl();
  }
  
  function handleJoystickEnd(e) {
    e.preventDefault();
    isDragging = false;
    roll = 0;
    pitch = 0;
    updateJoystick(0, 0);
    sendControl();
  }
  
  function sendControl() {
    const now = Date.now();
    if (now - lastSendTime < 50) return;
    lastSendTime = now;
    
    const url = `/control?throttle=${throttle}&roll=${roll}&pitch=${pitch}&yaw=${yaw}`;
    
    fetch(url)
      .then(() => { warningEl.textContent = ''; updateStatus(); })
      .catch(() => { warningEl.textContent = '⚠️ Ошибка связи!'; });
  }
  
  function emergencyStop() {
    fetch('/control?action=stop')
      .then(() => {
        throttle = 0; roll = 0; pitch = 0; yaw = 0;
        throttleSlider.value = 0;
        throttleDisplay.textContent = '0';
        updateJoystick(0, 0);
        warningEl.textContent = '🛑 АВАРИЙНАЯ ОСТАНОВКА!';
        updateStatus();
      })
      .catch(() => { warningEl.textContent = '⚠️ Ошибка связи!'; });
  }
  
  function resetAll() {
    throttle = 0; roll = 0; pitch = 0; yaw = 0;
    throttleSlider.value = 0;
    throttleDisplay.textContent = '0';
    updateJoystick(0, 0);
    warningEl.textContent = '↺ Сброшено';
    sendControl();
  }
  
  function updateStatus() {
    document.getElementById('statusThrottle').textContent = throttle;
    document.getElementById('statusRoll').textContent = roll;
    document.getElementById('statusPitch').textContent = pitch;
  }
  
  function fetchStatus() {
    fetch('/status')
      .then(response => response.json())
      .then(data => {
        if (data.emergency) warningEl.textContent = '🛑 АВАРИЙНАЯ ОСТАНОВКА!';
      })
      .catch(() => {});
  }
  
  joystick.addEventListener('mousedown', handleJoystickStart);
  document.addEventListener('mousemove', handleJoystickMove);
  document.addEventListener('mouseup', handleJoystickEnd);
  
  joystick.addEventListener('touchstart', handleJoystickStart, { passive: false });
  document.addEventListener('touchmove', handleJoystickMove, { passive: false });
  document.addEventListener('touchend', handleJoystickEnd, { passive: false });
  
  throttleSlider.addEventListener('input', function() {
    throttle = parseInt(this.value);
    throttleDisplay.textContent = throttle;
    sendControl();
  });
  
  stopBtn.addEventListener('click', emergencyStop);
  resetBtn.addEventListener('click', resetAll);
  
  updateStatus();
  setInterval(fetchStatus, 3000);
  console.log('🚁 Дрон готов к управлению!');
</script>
</body>
</html>
)rawliteral";
  return html;
}

// ============================================================
// 8. SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("============================================");
  Serial.println("   ПОЛНОЦЕННОЕ УПРАВЛЕНИЕ КВАДРОКОПТЕРОМ");
  Serial.println("============================================");
  
  for (int i = 0; i < 4; i++) {
    pinMode(motors[i].pinFI, OUTPUT);
    pinMode(motors[i].pinBI, OUTPUT);
  }
  
  stopAllMotors();
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Wi-Fi: ");
  Serial.println(ssid);
  Serial.print("IP: ");
  Serial.println(IP);
  
  server.on("/", handleRoot);
  server.on("/control", handleControl);
  server.on("/status", handleStatus);
  
  server.begin();
  Serial.println("Сервер запущен!");
  Serial.println("http://192.168.4.1");
  Serial.println();
  Serial.println("УПРАВЛЕНИЕ:");
  Serial.println("  Джойстик → КРЕН (влево/вправо) и ТАНГАЖ (вверх/вниз)");
  Serial.println("  Ползунок → ГАЗ (общая тяга)");
  Serial.println("  Кнопка СТОП → аварийная остановка");
  Serial.println("============================================");
}

// ============================================================
// 9. LOOP
// ============================================================

void loop() {
  server.handleClient();
  
  if (millis() - lastClientTime > 5000 && throttle > 0) {
    stopAllMotors();
    Serial.println("⚠️ Потеря связи! Моторы выключены.");
    throttle = 0;
    lastClientTime = millis();
  }
  
  delay(10);
}