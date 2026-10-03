#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
// SoftwareSerial НЕ нужен

const char* AP_SSID = "AquaScaner";
const char* AP_PASS = "12345678";

ESP8266WebServer server(80);

#define IN1 D1
#define IN2 D2
#define IN3 D5
#define IN4 D6

String dataTurbidity = "--";
String dataPH        = "--";
String dataTDS       = "--";
String dataTemp      = "--";

unsigned long lastReceiveTime = 0;
const unsigned long TIMEOUT = 3000;

void motors(int a1, int a2, int b1, int b2) {
  digitalWrite(IN1, a1); digitalWrite(IN2, a2);
  digitalWrite(IN3, b1); digitalWrite(IN4, b2);
}
void stopAll()   { motors(0,0,0,0); }
void forward()   { motors(1,0,1,0); }
void backward()  { motors(0,1,0,1); }
void turnLeft()  { motors(0,1,1,0); }
void turnRight() { motors(1,0,0,1); }

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="ru"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>АкваСканер</title>
<style>
  * { box-sizing: border-box; }
  body { font-family: -apple-system, Arial, sans-serif;
    background: linear-gradient(160deg, #0a1929 0%, #0d2137 100%);
    color: #e6f1ff; text-align: center; margin: 0; padding: 20px; min-height: 100vh; }
  h1 { font-size: 24px; margin: 0 0 6px;
    background: linear-gradient(90deg, #00d4ff, #00ffa3);
    -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
  .subtitle { font-size: 13px; color: #6b8ba8; margin-bottom: 20px; }
  .status-bar { display: inline-block; padding: 8px 18px; border-radius: 20px;
    font-size: 13px; margin-bottom: 20px; background: #1a2f45; }
  .status-bar.online { background: #0d3d2a; color: #00ffa3; }
  .status-bar.offline { background: #3d0d1a; color: #ff5c7a; }
  h2.section { font-size: 14px; color: #6b8ba8; text-transform: uppercase;
    letter-spacing: 1.5px; margin: 30px 0 14px; font-weight: 600; }
  .pad { display: grid; grid-template-columns: repeat(3, 90px);
    grid-gap: 10px; justify-content: center; margin: 0 auto 10px; }
  .pad button { width: 90px; height: 90px; font-size: 28px; border-radius: 16px;
    border: 1px solid rgba(255,255,255,0.08);
    background: rgba(255,255,255,0.04); color: #e6f1ff; cursor: pointer;
    user-select: none; transition: all .1s; -webkit-tap-highlight-color: transparent; }
  .pad button:active, .pad button.active {
    background: linear-gradient(135deg, rgba(0,212,255,0.3), rgba(0,255,163,0.3));
    border-color: rgba(0,255,163,0.5); transform: scale(0.95); }
  .pad .stop { background: rgba(255,92,122,0.15);
    border-color: rgba(255,92,122,0.3); color: #ff5c7a; }
  .pad .stop:active, .pad .stop.active { background: rgba(255,92,122,0.4);
    border-color: #ff5c7a; }
  .pad .empty { visibility: hidden; }
  .status-cmd { font-size: 13px; color: #6b8ba8; margin-bottom: 10px; height: 18px; }
  .sensors { display: grid; grid-template-columns: repeat(2, 1fr);
    gap: 14px; max-width: 520px; margin: 0 auto; }
  .card { background: rgba(255,255,255,0.04); border-radius: 18px;
    padding: 18px 14px; border: 1px solid rgba(255,255,255,0.06);
    position: relative; overflow: hidden; transition: all .3s; }
  .card::before { content: ''; position: absolute; top: 0; left: 0;
    width: 4px; height: 100%;
    background: linear-gradient(180deg, #00d4ff, #00ffa3); transition: background .3s; }
  .card.off::before { background: #3a4a5a; }
  .card.off { opacity: 0.55; }
  .card .icon { font-size: 24px; margin-bottom: 4px; }
  .card h3 { margin: 0 0 8px; font-size: 11px; color: #6b8ba8;
    font-weight: 600; text-transform: uppercase; letter-spacing: 1px; }
  .card .val { font-size: 28px; font-weight: 700;
    background: linear-gradient(90deg, #00d4ff, #00ffa3);
    -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
  .card.off .val { background: none; -webkit-text-fill-color: #3a4a5a; color: #3a4a5a; }
  .card .unit { font-size: 13px; color: #6b8ba8; margin-left: 3px; }
  .footer { margin-top: 30px; font-size: 11px; color: #3a4a5a; }
</style></head><body>
  <h1>💧 АкваСканер</h1>
  <div class="subtitle">Мониторинг качества воды + управление</div>
  <div class="status-bar offline" id="conn">Ожидание данных...</div>
  <h2 class="section">🎮 Управление</h2>
  <div class="pad">
    <button class="empty"></button>
    <button ontouchstart="cmd('f')" onmousedown="cmd('f')">▲</button>
    <button class="empty"></button>
    <button ontouchstart="cmd('l')" onmousedown="cmd('l')">◀</button>
    <button class="stop" ontouchstart="cmd('s')" onmousedown="cmd('s')">■</button>
    <button ontouchstart="cmd('r')" onmousedown="cmd('r')">▶</button>
    <button class="empty"></button>
    <button ontouchstart="cmd('b')" onmousedown="cmd('b')">▼</button>
    <button class="empty"></button>
  </div>
  <div class="status-cmd" id="st">Готов</div>
  <h2 class="section">📊 Датчики</h2>
  <div class="sensors">
    <div class="card off" id="c-turb"><div class="icon">🌊</div><h3>Мутность</h3>
      <div><span class="val" id="v-turb">--</span><span class="unit">NTU</span></div></div>
    <div class="card off" id="c-ph"><div class="icon">⚗️</div><h3>pH</h3>
      <div><span class="val" id="v-ph">--</span><span class="unit"></span></div></div>
    <div class="card off" id="c-tds"><div class="icon">🧪</div><h3>TDS</h3>
      <div><span class="val" id="v-tds">--</span><span class="unit">ppm</span></div></div>
    <div class="card off" id="c-temp"><div class="icon">🌡️</div><h3>Температура</h3>
      <div><span class="val" id="v-temp">--</span><span class="unit">°C</span></div></div>
  </div>
  <div class="footer">AquaScaner v1.3 · Wemos D1 Mini</div>
<script>
  let busy = false;
  function cmd(c) {
    if (busy) return; busy = true;
    document.getElementById('st').textContent = 'Команда: ' + c;
    fetch('/cmd?c=' + c).then(()=>{ busy=false; }).catch(()=>{ busy=false; });
  }
  document.querySelectorAll('.pad button:not(.empty)').forEach(b => {
    b.addEventListener('mouseup', ()=>cmd('s'));
    b.addEventListener('touchend', ()=>cmd('s'));
    b.addEventListener('mouseleave', ()=>cmd('s'));
  });
  function updateSensors() {
    fetch('/sensors').then(r => r.json()).then(d => {
      let vals = {turb: d.turb, ph: d.ph, tds: d.tds, temp: d.temp};
      for (let k in vals) {
        let el = document.getElementById('v-' + k);
        let card = document.getElementById('c-' + k);
        el.textContent = vals[k];
        if (vals[k] === '--') card.classList.add('off');
        else card.classList.remove('off');
      }
      let anyOnline = (d.turb !== '--' || d.ph !== '--' || d.tds !== '--' || d.temp !== '--');
      let conn = document.getElementById('conn');
      if (anyOnline) { conn.textContent = '🟢 Данные идут';
        conn.className = 'status-bar online'; }
      else { conn.textContent = '🔴 Нет связи с Arduino';
        conn.className = 'status-bar offline'; }
    }).catch(() => {
      let conn = document.getElementById('conn');
      conn.textContent = '🔴 Ошибка соединения';
      conn.className = 'status-bar offline';
    });
  }
  updateSensors(); setInterval(updateSensors, 1000);
</script></body></html>
)HTML";

void handleRoot() { server.send_P(200, "text/html; charset=utf-8", INDEX_HTML); }
void handleCmd() {
  String c = server.arg("c");
  if      (c == "f") forward();
  else if (c == "b") backward();
  else if (c == "l") turnLeft();
  else if (c == "r") turnRight();
  else               stopAll();
  server.send(200, "text/plain", "OK");
}
void handleSensors() {
  String json = "{";
  json += "\"turb\":\"" + dataTurbidity + "\",";
  json += "\"ph\":\""   + dataPH + "\",";
  json += "\"tds\":\""  + dataTDS + "\",";
  json += "\"temp\":\"" + dataTemp + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void parseArduinoData(String line) {
  if (!line.startsWith("DATA,")) return;
  int p1 = line.indexOf(',', 5);
  int p2 = line.indexOf(',', p1 + 1);
  int p3 = line.indexOf(',', p2 + 1);
  if (p1 < 0 || p2 < 0 || p3 < 0) return;
  dataTurbidity = line.substring(5, p1);
  dataPH        = line.substring(p1 + 1, p2);
  dataTDS       = line.substring(p2 + 1, p3);
  dataTemp      = line.substring(p3 + 1);
  lastReceiveTime = millis();
}

void setup() {
  Serial.begin(9600);     // аппаратный串口 — RXD/TXD
  Serial.setTimeout(50);
  delay(100);

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  stopAll();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.on("/sensors", handleSensors);
  server.begin();
}

void loop() {
  server.handleClient();
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    parseArduinoData(line);
  }
  if (lastReceiveTime > 0 && millis() - lastReceiveTime > TIMEOUT) {
    dataTurbidity = "--"; dataPH = "--";
    dataTDS = "--"; dataTemp = "--";
  }
}