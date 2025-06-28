#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <WebSocketsServer.h>

// put function declarations here:
int myFunction(int, int);

Preferences preferences;
WebServer server(80);
WebSocketsServer wsServer(81);

String ssid = "";
String password = "";
bool shouldConnectSTA = false;

const char* ap_ssid = "ESP32-Setup";
const char* ap_password = "12345678";

String htmlForm = R"rawliteral(
<!DOCTYPE html>
<html lang='th'>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>ตั้งค่า WiFi ESP32</title>
  <style>
    body { font-family: 'Kanit', 'Prompt', 'Tahoma', sans-serif; background: #f0f4f8; margin: 0; padding: 0; }
    .container { max-width: 400px; margin: 40px auto; background: #fff; border-radius: 12px; box-shadow: 0 2px 12px rgba(0,0,0,0.08); padding: 32px 24px; }
    h2 { color: #1976d2; text-align: center; margin-bottom: 24px; }
    label { display: block; margin-bottom: 8px; color: #333; font-weight: 500; }
    select, input[type='password'] { width: 100%; padding: 10px; margin-bottom: 18px; border: 1px solid #bdbdbd; border-radius: 6px; font-size: 1em; }
    button, input[type='submit'] { width: 100%; background: #1976d2; color: #fff; border: none; border-radius: 6px; padding: 12px; font-size: 1.1em; font-weight: bold; cursor: pointer; margin-bottom: 10px; transition: background 0.2s; }
    button:hover, input[type='submit']:hover { background: #1565c0; }
    .status { text-align: center; color: #388e3c; margin-bottom: 12px; font-size: 1em; }
    .error { color: #d32f2f; }
    @media (max-width: 500px) { .container { padding: 18px 4vw; } }
  </style>
  <link href="https://fonts.googleapis.com/css2?family=Kanit:wght@400;600&family=Prompt:wght@400;600&display=swap" rel="stylesheet">
  <script>
    function loadSSIDs() {
      let sel = document.getElementById('ssid');
      sel.innerHTML = '<option>กำลังค้นหา...</option>';
      fetch('/scan').then(r => r.json()).then(data => {
        sel.innerHTML = '';
        data.forEach(function(ssid) {
          let opt = document.createElement('option');
          opt.value = ssid;
          opt.text = ssid;
          sel.appendChild(opt);
        });
      }).catch(() => {
        sel.innerHTML = '<option>ไม่พบ WiFi</option>';
      });
    }
    window.onload = loadSSIDs;
  </script>
</head>
<body>
  <div class='container'>
    <h2>ตั้งค่า WiFi สำหรับ ESP32</h2>
    <form action='/save' method='POST'>
      <label for='ssid'>เลือก WiFi</label>
      <select id='ssid' name='ssid'></select>
      <label for='password'>รหัสผ่าน</label>
      <input id='password' name='password' type='password' maxlength='64' placeholder='Password'>
      <input type='submit' value='บันทึกและเชื่อมต่อ'>
    </form>
    <button onclick='loadSSIDs()' type='button'>สแกน WiFi ใหม่</button>
    <div class='status'>กรุณาเลือก WiFi และกรอกรหัสผ่าน</div>
  </div>
</body>
</html>
)rawliteral";

String joystickPage = R"rawliteral(
<!DOCTYPE html>
<html lang='th'>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>ESP32 Joystick Control</title>
  <link href='https://fonts.googleapis.com/css2?family=Kanit:wght@400;600&display=swap' rel='stylesheet'>
  <style>
    body { background: #f0f4f8; font-family: 'Kanit', Tahoma, sans-serif; margin: 0; }
    .container { max-width: 420px; margin: 40px auto; background: #fff; border-radius: 16px; box-shadow: 0 2px 16px rgba(0,0,0,0.10); padding: 32px 24px; }
    h2 { color: #1976d2; text-align: center; margin-bottom: 24px; }
    #joystick-zone { width: 260px; height: 260px; margin: 0 auto 24px auto; background: #e3eaf2; border-radius: 50%; box-shadow: 0 1px 8px rgba(25,118,210,0.08); display: flex; align-items: center; justify-content: center; }
    .status { text-align: center; color: #388e3c; margin-bottom: 12px; font-size: 1em; }
    .ws-status { text-align: center; margin-bottom: 18px; font-size: 1em; }
    .ws-status.connected { color: #388e3c; }
    .ws-status.disconnected { color: #d32f2f; }
    @media (max-width: 500px) { .container { padding: 18px 2vw; } #joystick-zone { width: 90vw; height: 90vw; max-width: 320px; max-height: 320px; } }
  </style>
  <script src='https://cdn.jsdelivr.net/npm/nipplejs@0.9.0/dist/nipplejs.min.js'></script>
</head>
<body>
  <div class='container'>
    <h2>ESP32 Joystick Control</h2>
    <div id='joystick-zone'></div>
    <div class='ws-status' id='wsStatus'>WebSocket: <span>Connecting...</span></div>
    <div class='status' id='joyVal'>x: 0, y: 0</div>
  </div>
  <script>
    let ws;
    function connectWS() {
      let proto = location.protocol === 'https:' ? 'wss' : 'ws';
      ws = new WebSocket(proto + '://' + location.hostname + ':81/');
      ws.onopen = () => {
        document.getElementById('wsStatus').innerHTML = 'WebSocket: <span class="connected">เชื่อมต่อแล้ว</span>';
      };
      ws.onclose = () => {
        document.getElementById('wsStatus').innerHTML = 'WebSocket: <span class="disconnected">หลุดการเชื่อมต่อ</span>';
        setTimeout(connectWS, 2000);
      };
      ws.onerror = () => ws.close();
    }
    connectWS();

    let joy = nipplejs.create({
      zone: document.getElementById('joystick-zone'),
      mode: 'static',
      position: { left: '50%', top: '50%' },
      color: '#1976d2',
      size: 200
    });
    joy.on('move', function(evt, data) {
      let x = 0, y = 0;
      if (data && data.vector) {
        x = Math.round(data.vector.x * 100);
        y = Math.round(data.vector.y * 100);
      }
      document.getElementById('joyVal').innerText = `x: ${x}, y: ${y}`;
      if (ws && ws.readyState === 1) {
        ws.send(JSON.stringify({x:x, y:y}));
      }
    });
    joy.on('end', function() {
      document.getElementById('joyVal').innerText = 'x: 0, y: 0';
      if (ws && ws.readyState === 1) {
        ws.send(JSON.stringify({x:0, y:0}));
      }
    });
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", htmlForm);
}

void handleSave() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    ssid = server.arg("ssid");
    password = server.arg("password");
    Serial.print("[DEBUG] Save SSID: "); Serial.println(ssid);
    Serial.print("[DEBUG] Save Password: "); Serial.println(password);
    preferences.begin("wifi", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();
    server.send(200, "text/html", "<html><body><div style='font-family:sans-serif;text-align:center;margin-top:40px;'><h2>บันทึกแล้ว<br>กำลังรีสตาร์ท...</h2></div></body></html>");
    delay(1000);
    shouldConnectSTA = true;
  } else {
    server.send(400, "text/html", "<html><body><div style='font-family:sans-serif;text-align:center;margin-top:40px;'><h2 style='color:#d32f2f'>กรุณากรอกข้อมูลให้ครบ</h2></div></body></html>");
  }
}

void handleScan() {
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    json += "\"" + WiFi.SSID(i) + "\"";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleJoystickPage() {
  server.send(200, "text/html", joystickPage);
}

void wsEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    String msg = String((char*)payload);
    Serial.print("[WS] Data: "); Serial.println(msg);
    // สามารถ parse JSON และนำไปควบคุมอุปกรณ์จริงได้ที่นี่
  }
}

void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/joystick", handleJoystickPage);
  server.begin();
  wsServer.begin();
  wsServer.onEvent(wsEvent);
  Serial.println("AP Mode Started. Connect to WiFi and browse 192.168.4.1");
}

void connectToWiFi() {
  preferences.begin("wifi", true);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  preferences.end();
  Serial.print("[DEBUG] Read SSID: "); Serial.println(ssid);
  Serial.print("[DEBUG] Read Password: "); Serial.println(password);
  if (ssid.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.print("Connecting to "); Serial.println(ssid);
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n[INFO] Connected!");
      Serial.print("[INFO] IP: "); Serial.println(WiFi.localIP());
      server.on("/joystick", handleJoystickPage);
      server.begin();
      wsServer.begin();
      wsServer.onEvent(wsEvent);
    } else {
      Serial.println("\n[WARN] Failed to connect. Starting AP mode.");
      startAPMode();
    }
  } else {
    Serial.println("[INFO] No WiFi credentials found. Starting AP mode.");
    startAPMode();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n===== ESP32 WiFi Configurator =====");
  connectToWiFi();
}

void loop() {
  if (WiFi.getMode() == WIFI_AP) {
    server.handleClient();
    wsServer.loop();
    if (shouldConnectSTA) {
      delay(1000);
      ESP.restart();
    }
  } else if (WiFi.getMode() == WIFI_STA) {
    server.handleClient();
    wsServer.loop();
  }
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}