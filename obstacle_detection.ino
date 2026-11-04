#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

const char* WIFI_SSID = "HOTSPOT_NAME";
const char* WIFI_PASS = "HOTSPOT_PASSWORD";
String TS_KEY = "API_KEY";

#define TRIG 5
#define ECHO 18
#define BUZZ 23

WebServer server(80);
int dist = 200;
int status = 0;
unsigned long lastRead = 0, lastTs = 0;

const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Robot Obstacle Radar</title>
<style>
body{margin:0;background:#0f172a;color:#e2e8f0;font-family:sans-serif;text-align:center}
#status{font-size:28px;font-weight:bold;padding:12px;margin:8px auto;border-radius:10px;max-width:420px}
.safe{background:#16a34a}.warn{background:#ca8a04}.danger{background:#dc2626}.off{background:#475569}
canvas{background:#1e293b;border-radius:10px;margin:6px;max-width:95%}
.big{font-size:40px;margin:6px}
</style></head><body>
<h2>🤖 Obstacle Detection Dashboard</h2>
<div id="status" class="off">Connecting...</div>
<div class="big"><span id="dist">--</span> cm</div>
<div>🚧 Danger events: <b id="cnt">0</b> &nbsp; 📏 Closest: <b id="min">--</b> cm</div>
<canvas id="view" width="360" height="360"></canvas><br>
<canvas id="chart" width="360" height="160"></canvas>
<script>
const MAX=200,hist=[];let prev=0,count=0,minD=999;
const v=document.getElementById('view').getContext('2d');
const c=document.getElementById('chart').getContext('2d');
const col=d=>d<30?'#dc2626':d<60?'#ca8a04':'#16a34a';
function draw(d){
  v.clearRect(0,0,360,360);v.strokeStyle='#334155';
  for(let r=60;r<=300;r+=60){v.beginPath();v.arc(180,340,r,Math.PI,0);v.stroke();}
  v.fillStyle=col(d)+'44';v.beginPath();v.moveTo(180,340);v.lineTo(80,40);v.lineTo(280,40);v.fill();
  v.font='32px sans-serif';v.fillText('🤖',162,358);
  if(d<MAX){const y=340-(d/MAX)*300;v.font='34px sans-serif';
    v.fillText(d<30?'🚧':'🧱',160,y);
    v.strokeStyle=col(d);v.setLineDash([5,5]);v.beginPath();
    v.moveTo(180,330);v.lineTo(180,y+10);v.stroke();v.setLineDash([]);}
}
function chart(){
  c.clearRect(0,0,360,160);
  c.strokeStyle='#dc2626';c.beginPath();c.moveTo(0,160-30/MAX*160);c.lineTo(360,160-30/MAX*160);c.stroke();
  c.strokeStyle='#38bdf8';c.beginPath();
  hist.forEach((d,i)=>{const x=i*6,y=160-Math.min(d,MAX)/MAX*160;i?c.lineTo(x,y):c.moveTo(x,y)});
  c.stroke();
}
async function tick(){
  const s=document.getElementById('status');
  try{
    const d=parseInt(await (await fetch('/d')).text());
    document.getElementById('dist').innerText=d>=MAX?'200+':d;
    if(d<30){s.className='danger';s.innerText='⛔ STOP! Obstacle very close'}
    else if(d<60){s.className='warn';s.innerText='⚠️ Approaching obstacle'}
    else{s.className='safe';s.innerText='✅ Path clear'}
    const st=d<30?2:0;
    if(st==2&&prev!=2){count++;document.getElementById('cnt').innerText=count}
    prev=st;if(d<minD){minD=d;document.getElementById('min').innerText=d}
    hist.push(d);if(hist.length>60)hist.shift();
    draw(d);chart();
  }catch(e){s.className='off';s.innerText='Offline...'}
  setTimeout(tick,300);
}
tick();
</script></body></html>
)rawliteral";

int readDistance() {
  digitalWrite(TRIG, LOW); delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long t = pulseIn(ECHO, HIGH, 25000);
  if (t == 0) return 200;
  return min(200, (int)(t * 0.034 / 2));
}

void sendThingSpeak() {
  HTTPClient http;
  String url = "http://api.thingspeak.com/update?api_key=" + TS_KEY +
               "&field1=" + String(dist) + "&field2=" + String(status);
  http.begin(url);
  int code = http.GET();
  Serial.println("ThingSpeak HTTP " + String(code) + " | entry: " + http.getString());
  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG, OUTPUT); pinMode(ECHO, INPUT); pinMode(BUZZ, OUTPUT);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nConnected! OPEN THIS IN BROWSER: http://" + WiFi.localIP().toString());
  server.on("/", []() { server.send_P(200, "text/html", PAGE); });
  server.on("/d", []() { server.send(200, "text/plain", String(dist)); });
  server.begin();
}

void loop() {
  server.handleClient();
  unsigned long now = millis();
  if (now - lastRead > 100) {
    lastRead = now;
    dist = readDistance();
    status = dist < 30 ? 2 : dist < 60 ? 1 : 0;
    digitalWrite(BUZZ, status == 2);
  }
  if (WiFi.status() == WL_CONNECTED && now - lastTs > 16000) {
    lastTs = now;
    sendThingSpeak();
  }
}
