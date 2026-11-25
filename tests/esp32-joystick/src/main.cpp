#include <Arduino.h>
#include <ETH.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <GyverJoy.h>

constexpr uint8_t pins[] = {36, 39, 35};
constexpr uint8_t buttonPin = 32;
GyverJoyVirt axes[3];
uint16_t raw[3], center[3];
int16_t output[3];
uint8_t curve = 0, dead = 20;
bool pressed = false, candidate = false;
uint32_t changed = 0, sampled = 0;
Preferences prefs;
WebServer server(80);

const char page[] PROGMEM = R"HTML(
<!doctype html><html lang="ru"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Joystick test</title>
<style>
body{font:16px system-ui;max-width:650px;margin:30px auto;padding:0 16px;background:#161b22;color:#eee}
.axis{padding:14px 0}.track{height:16px;background:#343b45;position:relative;border-radius:5px}
.track:after{content:'';position:absolute;left:50%;height:100%;border-left:1px solid white}
.fill{position:absolute;height:100%;background:#57a6ff;border-radius:4px}
button,select,input{font:inherit;margin:8px 0}canvas{width:100%;max-width:480px;background:#222933}
</style>
<h1>Joystick test</h1><p id="status">Connecting...</p>
<div id="axes"></div><p>Button: <strong id="btn">—</strong></p>
<label>Curve <select id="curve"><option value="0">Linear</option><option value="1">Square</option><option value="2">Cubic</option></select></label>
<label>Dead zone <input id="dead" type="number" min="0" max="100" value="20"></label>
<button id="apply">Save</button><br>
<canvas id="plot" width="480" height="240"></canvas><br>
<button id="zero">Set current position as zero</button>
<script>
const $=id=>document.getElementById(id);
$('axes').innerHTML=['X','Y','Z'].map((a,i)=>'<div class="axis">'+a+' <b id="v'+i+'">—</b> <small id="r'+i+'"></small><div class="track"><div class="fill" id="f'+i+'"></div></div></div>').join('');
function plot(){let c=$('plot'),g=c.getContext('2d'),w=c.width,h=c.height;g.clearRect(0,0,w,h);g.strokeStyle='#667';g.beginPath();g.moveTo(0,h/2);g.lineTo(w,h/2);g.moveTo(w/2,0);g.lineTo(w/2,h);g.stroke();g.strokeStyle='#57a6ff';g.beginPath();for(let i=0;i<=w;i++){let x=i/w*2-1,y=Math.sign(x)*Math.pow(Math.abs(x),Number($('curve').value)+1);g.lineTo(i,(1-y)*h/2)}g.stroke()}
$('curve').onchange=plot;
let initialized=false;
async function poll(){try{let r=await fetch('/state',{cache:'no-store'});if(!r.ok)throw Error();let s=await r.json();$('status').textContent='Connected';if(!initialized){$('curve').value=s.curve;$('dead').value=s.dead;initialized=true;plot()}s.value.forEach((v,i)=>{$('v'+i).textContent=v;$('r'+i).textContent='ADC '+s.raw[i]+' / zero '+s.center[i];let f=$('f'+i);f.style.left=(50+Math.min(0,v)/255*50)+'%';f.style.width=Math.abs(v)/255*50+'%'});$('btn').textContent=s.button?'Pressed':'Released'}catch(e){$('status').textContent='Disconnected'}finally{setTimeout(poll,50)}}
async function post(url){try{let r=await fetch(url,{method:'POST'});if(!r.ok)throw Error(await r.text());}catch(e){alert(e.message)}}
$('apply').onclick=()=>post('/settings?curve='+$('curve').value+'&dead='+$('dead').value);
$('zero').onclick=()=>post('/zero');
poll();plot();
</script></html>
)HTML";

void configure() {
  for (int i=0;i<3;i++) {
    axes[i].calibrate(center[i]);
    axes[i].deadzone(dead);
    axes[i].exponent(static_cast<GJTYPE>(curve));
  }
}
void sample() {
  for(int i=0;i<3;i++) {
    raw[i]=analogRead(pins[i]);
    output[i]=axes[i].compute(raw[i]);
  }
}
void state() {
  char buf[256];
  snprintf(buf,sizeof(buf),"{\"raw\":[%u,%u,%u],\"center\":[%u,%u,%u],\"value\":[%d,%d,%d],\"button\":%s,\"curve\":%u,\"dead\":%u}",
    raw[0],raw[1],raw[2],center[0],center[1],center[2],output[0],output[1],output[2],pressed?"true":"false",curve,dead);
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json",buf);
}
void setup() {
  Serial.begin(115200);
  analogReadResolution(10); // GyverJoy expects 0..1023
  for(auto pin:pins) analogSetPinAttenuation(pin,ADC_11db);
  pinMode(buttonPin,INPUT_PULLUP);
  prefs.begin("joystick",false);
  curve=constrain(prefs.getUChar("curve",0),0,2);
  dead=constrain(prefs.getUChar("dead",20),0,100);
  for(int i=0;i<3;i++){char key[8];snprintf(key,sizeof(key),"zero%d",i);center[i]=constrain(prefs.getUShort(key,512),51,972);}
  configure();
  sample();
  ETH.begin(1,16,23,18,ETH_PHY_LAN8720,ETH_CLOCK_GPIO0_IN);
  server.on("/",HTTP_GET,[]{server.send_P(200,"text/html",page);});
  server.on("/state",HTTP_GET,state);
  server.on("/settings",HTTP_POST,[]{
    if(!server.hasArg("curve")||!server.hasArg("dead")){server.send(400,"text/plain","Missing settings");return;}
    int c=server.arg("curve").toInt(),d=server.arg("dead").toInt();
    if(c<0||c>2||d<0||d>100){server.send(400,"text/plain","Invalid settings");return;}
    curve=c;dead=d;configure();prefs.putUChar("curve",curve);prefs.putUChar("dead",dead);sample();state();
  });
  server.on("/zero",HTTP_POST,[]{
    uint16_t next[3];for(int i=0;i<3;i++){uint32_t sum=0;for(int j=0;j<32;j++)sum+=analogRead(pins[i]);next[i]=sum/32;if(next[i]<51||next[i]>972){server.send(400,"text/plain","Center too close to ADC endpoint");return;}}
    for(int i=0;i<3;i++){center[i]=next[i];char key[8];snprintf(key,sizeof(key),"zero%d",i);prefs.putUShort(key,center[i]);}
    configure();sample();state();
  });
  server.begin();
  ArduinoOTA.setHostname("joystick");
}
void loop() {
  static bool otaStarted=false;
  if(!otaStarted && ETH.localIP()!=IPAddress(0,0,0,0)){
    Serial.println(ETH.localIP());ArduinoOTA.begin();otaStarted=true;
  }
  if(otaStarted) ArduinoOTA.handle();
  uint32_t now=millis();
  if(now-sampled>=10){sampled=now;sample();}
  bool b=digitalRead(buttonPin)==LOW;
  if(b!=candidate){candidate=b;changed=now;}
  if(now-changed>=20)pressed=candidate;
  server.handleClient();
  delay(1);
}
