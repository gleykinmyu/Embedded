R"html(<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>PH350 · E650</title>
<style>
:root{--bg:#121418;--fg:#e8eaed;--muted:#8b93a0;--line:#2a3038;--acc:#3d8bfd;--danger:#e74c3c;--ok:#3dd68c;--bar:48px}
*{box-sizing:border-box}
body{margin:0;font-family:Segoe UI,system-ui,sans-serif;background:var(--bg);color:var(--fg);padding-bottom:calc(var(--bar) + 12px)}
main{max-width:720px;margin:0 auto;padding:14px 14px 8px}
h1{font-size:18px;margin:0 0 2px}
.sub,.hint{color:var(--muted);margin:0 0 10px;font-size:12px}
.tabs{display:flex;gap:6px;margin-bottom:12px;flex-wrap:wrap}
.tabs button{flex:1;min-width:90px;appearance:none;border:1px solid var(--line);background:#1c2128;color:var(--fg);
  border-radius:8px;padding:10px;font-size:14px;cursor:pointer}
.tabs button.on{background:var(--acc);border-color:var(--acc)}
.panel{display:none}.panel.on{display:block}
h2{font-size:13px;color:var(--muted);font-weight:600;margin:14px 0 8px;text-transform:uppercase;letter-spacing:.04em}
.pad{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px;max-width:240px;margin:0 auto 12px}
.grid{display:flex;flex-wrap:wrap;gap:6px;margin-bottom:8px}
.pad button,.grid button,.row button{appearance:none;border:1px solid var(--line);background:#1c2128;color:var(--fg);
  border-radius:8px;padding:10px 8px;font-size:13px;cursor:pointer}
.pad button{padding:14px 8px;font-size:16px}
.pad button:active,.grid button:active,.row button:active{background:var(--acc);border-color:var(--acc)}
.pad .sp{visibility:hidden}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-bottom:8px}
.row label,.field{display:flex;align-items:center;gap:6px;color:var(--muted);font-size:13px}
input,select{background:#1c2128;color:var(--fg);border:1px solid var(--line);border-radius:6px;padding:7px 8px;font-size:13px}
input[type=range]{width:120px;padding:0}
input[type=number]{width:4.5em}
input.wide{width:7em}
input.xwide{width:11em}
table{border-collapse:collapse;width:100%;margin-top:4px}
td{padding:6px 0;border-bottom:1px solid var(--line);font-size:13px}
td:first-child{color:var(--muted);width:7em}
.danger{border-color:#6b2a2a!important;background:#2a1717!important}
#bar{position:fixed;left:0;right:0;bottom:0;min-height:var(--bar);max-height:30vh;overflow:auto;padding:8px 14px;background:#0c0e11;border-top:1px solid var(--line);
  font-family:ui-monospace,Consolas,monospace;font-size:12px;color:var(--muted);display:flex;align-items:flex-start;gap:10px;z-index:9}
#bar .st{color:var(--fg);word-break:break-all;white-space:pre-wrap}
#bar.ok .st{color:var(--ok)}#bar.bad .st{color:var(--danger)}
#bar .cmd{color:var(--acc);flex:0 0 auto}
</style>
</head>
<body>
<main>
<h1 id="title">AW-PH350E · AW-E650E</h1>
<p class="sub">ESP32-ETH01 · RS-422 · тест всех команд</p>
<div class="tabs">
  <button class="on" data-tab="pt">Head PT</button>
  <button data-tab="cam">Camera E650</button>
  <button data-tab="info">Info</button>
</div>

<section id="pt" class="panel on">
  <div class="pad">
    <button class="sp">·</button>
    <button data-hold="tilt_up">▲</button>
    <button class="sp">·</button>
    <button data-hold="pan_left">◀</button>
    <button class="danger" data-cmd="stop">■</button>
    <button data-hold="pan_right">▶</button>
    <button class="sp">·</button>
    <button data-hold="tilt_down">▼</button>
    <button class="sp">·</button>
  </div>
  <div class="row">
    <label>скорость <span id="spd">30</span>
      <input id="rate" type="range" min="1" max="49" value="30" oninput="spd.textContent=this.value">
    </label>
    <button data-hold="zoom_tele">Zoom+</button>
    <button data-hold="zoom_wide">Zoom−</button>
    <button data-hold="focus_far">Focus+</button>
    <button data-hold="focus_near">Focus−</button>
  </div>
  <h2>Power / home / presets</h2>
  <div class="grid">
    <button data-cmd="power_off">Power OFF</button>
    <button data-cmd="power_on">Power ON</button>
    <button data-cmd="power_on_cam">ON + CamTX</button>
    <button data-cmd="power_on_nocam">ON no CamTX</button>
    <button data-cmd="home" title="PH350: recall preset #01 (не #APC)">Home (P01)</button>
    <button data-cmd="stop">Stop all</button>
  </div>
  <div class="row">
    <label>пресет <input id="pr" type="number" min="1" max="99" value="1"></label>
    <button data-cmd="preset_recall">Recall</button>
    <button data-cmd="preset_save">Save</button>
    <button data-cmd="preset_delete">Delete</button>
    <button data-cmd="preset_latest">Latest #</button>
  </div>
  <div class="row">
    <label>PrMode
      <select id="prmode"><option value="0">Normal</option><option value="1">Diagonal</option></select>
    </label>
    <button data-cmd="preset_mode">Set mode</button>
    <label>PrSpeed
      <select id="prspd"><option value="0">Slow</option><option value="1">Mid</option><option value="2">Fast</option></select>
    </label>
    <button data-cmd="preset_speed">Set speed</button>
    <label>unit <input id="prunit" class="wide" value="080" maxlength="3"></label>
    <button data-cmd="preset_speed_unit">Speed unit</button>
  </div>
  <h2>Position / lens</h2>
  <div class="grid">
    <button data-cmd="pos">Pos</button>
    <button data-cmd="pos_disp">Pos display</button>
    <button data-cmd="req_zoom">Q Zoom</button>
    <button data-cmd="req_focus">Q Focus</button>
    <button data-cmd="req_iris">Q Iris</button>
    <button data-cmd="req_lens">Q Lens info</button>
    <button data-cmd="req_exposure">Q Exposure</button>
    <button data-cmd="req_error">Q Error</button>
    <button data-cmd="req_landing">Q Landing</button>
    <button data-cmd="req_ver">Q Version</button>
    <button data-cmd="req_heater">Q Heater</button>
    <button data-cmd="req_defrost">Q Defroster</button>
  </div>
  <p class="hint">#APC/#APS на AW-PH350 по PDF не поддерживаются — используйте пресеты. Home = Recall #01.</p>
  <div class="row">
    <label>pan <input id="pan" class="wide" value="0x8000"></label>
    <label>tilt <input id="tilt" class="wide" value="0x8000"></label>
    <button data-cmd="set_pt" title="#APC — не для PH350">Set PT (#APC)</button>
    <button data-cmd="rel_pt">Rel PT</button>
  </div>
  <div class="row">
    <label>zoom <input id="zoom" class="wide" value="0x0000" title="0..FFFF Wide..Tele"></label>
    <label>focus <input id="focus" class="wide" value="0x8000" title="0..FFFF Near..Far"></label>
    <label>iris <input id="iris" class="wide" value="0x8000" title="0..FFFF Close..Open"></label>
    <button data-cmd="set_lens">Set lens</button>
    <button data-cmd="set_zoom">Set Z</button>
    <button data-cmd="set_focus">Set F</button>
    <button data-cmd="set_iris">Set I</button>
  </div>
  <div class="row">
    <label>lens ctrl
      <select id="lensctrl"><option value="0">Off</option><option value="1">On</option></select>
    </label>
    <button data-cmd="lens_ctrl">Set</button>
    <label>APS spd <input id="apsspd" type="number" min="1" max="30" value="15"></label>
    <label>table
      <select id="apstab"><option value="0">Slow</option><option value="1">Mid</option><option value="2">Fast</option></select>
    </label>
    <button data-cmd="set_pt_w_spd">PT+speed</button>
    <button data-cmd="rel_pt_w_spd">Rel+speed</button>
  </div>
  <h2>Options</h2>
  <div class="row">
    <label>limit
      <select id="lim"><option value="0">Release</option><option value="1">Set</option></select>
    </label>
    <button data-cmd="limitation">Limitation</button>
    <label>axis
      <select id="limax"><option value="1">TiltUp</option><option value="2">TiltDown</option><option value="3">PanLeft</option><option value="4">PanRight</option></select>
    </label>
    <button data-cmd="limit_ctrl">Limit ctrl</button>
    <label>tilt range
      <select id="tiltr"><option value="0">190°</option><option value="1">300°</option></select>
    </label>
    <button data-cmd="tilt_range">Set</button>
  </div>
  <div class="row">
    <label>tally <select id="tally"><option value="0">Off</option><option value="1">On</option></select></label>
    <button data-cmd="tally">Tally</button>
    <label>wireless <select id="wl"><option value="0">Off</option><option value="1">On</option></select></label>
    <button data-cmd="wireless">Wireless</button>
    <label>washer <select id="wash"><option value="0">Off</option><option value="1">On</option></select></label>
    <button data-cmd="washer">Washer</button>
    <label>option <select id="opt"><option value="0">Off</option><option value="1">On</option></select></label>
    <button data-cmd="option">Option</button>
  </div>
  <div class="row">
    <label>fan <select id="fan"><option value="0">Auto</option><option value="1">High</option><option value="2">Mid</option><option value="3">Low</option></select></label>
    <button data-cmd="fan">Fan</button>
    <button data-cmd="fan2">Fan2</button>
    <label>wiper <select id="wip"><option value="0">Off</option><option value="1">Fast</option><option value="2">Slow</option></select></label>
    <button data-cmd="wiper">Wiper</button>
  </div>
  <h2>Raw PT (#cmd)</h2>
  <div class="row">
    <label>cmd <input id="rawcmd" class="wide" value="O" maxlength="8"></label>
    <label>payload <input id="rawpay" class="xwide" value="1"></label>
    <button data-cmd="raw">Transact</button>
    <button data-cmd="raw_send">Send only</button>
  </div>
</section>

<section id="cam" class="panel">
  <h2>Identity / triggers</h2>
  <div class="grid">
    <button data-api="cam" data-cmd="model">Q Model</button>
    <button data-api="cam" data-cmd="version">Q Version</button>
    <button data-api="cam" data-cmd="awc_start">AWC start</button>
    <button data-api="cam" data-cmd="abc_start">ABC/ABB</button>
  </div>
  <h2>AWC / scene / shutter</h2>
  <div class="row">
    <label>AWC
      <select id="awc">
        <option value="0">ATW</option><option value="1">AWC A</option><option value="2">AWC B</option>
        <option value="3">3200K</option><option value="4">5600K</option><option value="5">4500K</option>
        <option value="6">6000K</option><option value="7">2800K</option><option value="8">VAR</option><option value="9">9</option>
      </select>
    </label>
    <button data-api="cam" data-cmd="awc_mode">Set AWC</button>
    <label>scene
      <select id="scene"><option value="0">Halogen</option><option value="1">Fluorescent</option><option value="2">Outdoor</option><option value="3">User</option></select>
    </label>
    <button data-api="cam" data-cmd="scene">Select</button>
  </div>
  <div class="row">
    <label>shutter
      <select id="shut">
        <option value="0">Off</option><option value="1">1/50</option><option value="2">1/60</option>
        <option value="3">1/100-120</option><option value="5">1/250</option><option value="10">Synchro</option><option value="11">ELC</option>
      </select>
    </label>
    <button data-api="cam" data-cmd="shutter_mode">Set shutter</button>
    <label>sync <input id="sync" class="wide" value="000"></label>
    <button data-api="cam" data-cmd="synchro">SynchroScan</button>
  </div>
  <h2>Gain / iris / detail / filter</h2>
  <p class="hint">Gain в дБ (E650 0..18, HE130 0..48, HE870 −6..18), внутри → OGU. Iris: Manual, затем 0..1023 (clamp к 0x3FF).</p>
  <div class="row">
    <label>gain dB <input id="cgain" type="number" min="-6" max="48" value="0"></label>
    <button data-api="cam" data-cmd="gain">Set gain</button>
    <button data-api="cam" data-cmd="agc">AGC ON</button>
    <label>detail <input id="cdet" type="number" min="0" max="2" value="0"></label>
    <button data-api="cam" data-cmd="detail">Detail</button>
    <label>HD detail <input id="chdet" type="number" min="0" max="2" value="0"></label>
    <button data-api="cam" data-cmd="hd_detail">HD detail</button>
  </div>
  <div class="row">
    <label>iris
      <select id="ciris"><option value="0">Manual (0)</option><option value="1">Auto (1)</option></select>
    </label>
    <button data-api="cam" data-cmd="iris_am">Iris A/M</button>
    <label>iris vol <input id="cmiris" class="wide" value="0x8000" title="0..FFFF → ORV 000..3FF"></label>
    <button data-api="cam" data-cmd="man_iris">Set iris</button>
    <label>filter <input id="cfilt" type="number" min="0" max="9" value="0"></label>
    <button data-api="cam" data-cmd="filter">Filter</button>
  </div>
  <h2>Picture / switches</h2>
  <p class="hint">Открыть меню — Menu SW (как кнопка MENU на RCU). OUS только вкл/выкл OSD. Смотреть видеовыход камеры.</p>
  <div class="row">
    <label>color bar <select id="cbar"><option value="0">Off</option><option value="1">On</option></select></label>
    <button data-api="cam" data-cmd="colorbar">Color bar</button>
    <label>OSD <select id="cmenu"><option value="0">Off</option><option value="1">On</option></select></label>
    <button data-api="cam" data-cmd="menu">Menu display (OUS)</button>
    <button data-api="cam" data-cmd="menu_sw">Menu / Up (DPG)</button>
    <button data-api="cam" data-cmd="item_sw">Item / Down (DIT)</button>
    <button data-api="cam" data-cmd="yes_sw">Yes / + (DUP)</button>
    <button data-api="cam" data-cmd="no_sw">No / − (DDW)</button>
  </div>
  <div class="row">
    <label>AF <select id="caf"><option value="0">Off</option><option value="1">On</option></select></label>
    <button data-api="cam" data-cmd="af">AutoFocus</button>
    <label>neg/pos <select id="cneg"><option value="0">Pos</option><option value="1">Neg</option></select></label>
    <button data-api="cam" data-cmd="negpos">NegPos</button>
    <label>F/F <select id="cff"><option value="0">Field</option><option value="1">Frame</option></select></label>
    <button data-api="cam" data-cmd="fieldframe">FieldFrame</button>
  </div>
  <div class="row">
    <label>chroma
      <select id="cchr">
        <option value="0">0</option><option value="1">1</option><option value="2">2</option><option value="3">3</option>
        <option value="4">4</option><option value="5">5</option><option value="6">6</option><option value="7">7</option>
        <option value="8">8</option><option value="9">9</option>
      </select>
    </label>
    <button data-api="cam" data-cmd="chroma">Chroma</button>
    <label>black shade <select id="cbsh"><option value="0">Off</option><option value="1">On</option></select></label>
    <button data-api="cam" data-cmd="black_shade">Black shade</button>
  </div>
  <h2>Paint / pedestal / OSD</h2>
  <div class="row">
    <label>R gain <input id="crg" class="wide" value="80"></label>
    <button data-api="cam" data-cmd="r_gain">Set</button>
    <label>B gain <input id="cbg" class="wide" value="80"></label>
    <button data-api="cam" data-cmd="b_gain">Set</button>
    <label>R ped <input id="crp" class="wide" value="80"></label>
    <button data-api="cam" data-cmd="r_ped">Set</button>
    <label>B ped <input id="cbp" class="wide" value="80"></label>
    <button data-api="cam" data-cmd="b_ped">Set</button>
  </div>
  <div class="row">
    <label>gamma <input id="cgam" class="wide" value="0"></label>
    <button data-api="cam" data-cmd="gamma">Gamma</button>
    <label>AGC max <input id="cagc" class="wide" value="0"></label>
    <button data-api="cam" data-cmd="agc_max">AGC max</button>
    <label>total dtl <input id="cdtl" class="wide" value="0"></label>
    <button data-api="cam" data-cmd="total_dtl">Total DTL</button>
    <label>genlock <input id="cgen" class="wide" value="0"></label>
    <button data-api="cam" data-cmd="genlock">Genlock</button>
  </div>
  <h2>Monitor / lens contact / scene data</h2>
  <div class="row">
    <label>dxx <input id="cdxx" class="wide" value="00"></label>
    <label>data <input id="cmon" class="wide" value="0"></label>
    <button data-api="cam" data-cmd="monitor">Monitor</button>
    <label>lens H <input id="clensh" class="wide" value="00"></label>
    <button data-api="cam" data-cmd="lens_h">Lens contact</button>
    <label>scene data <input id="cscened" class="wide" value="0"></label>
    <button data-api="cam" data-cmd="scene_data">SelectScene</button>
  </div>
</section>

<section id="info" class="panel">
  <table>
    <tr><td>Link</td><td id="link">…</td></tr>
    <tr><td>IP</td><td id="ip">…</td></tr>
    <tr><td>Mask</td><td id="mask">…</td></tr>
    <tr><td>GW</td><td id="gw">…</td></tr>
    <tr><td>MAC</td><td id="mac">…</td></tr>
    <tr><td>PT</td><td id="ptm">…</td></tr>
    <tr><td>Cam</td><td id="camm">…</td></tr>
    <tr><td>Rate</td><td id="rateinfo">…</td></tr>
    <tr><td>Pos</td><td id="pos">—</td></tr>
    <tr><td>Uptime</td><td id="up">…</td></tr>
  </table>
</section>
</main>
<div id="bar"><span class="cmd">ready</span><span class="st">—</span></div>
<script>
const $=id=>document.getElementById(id);
function showBar(cmd,d){
  const bar=$('bar');
  bar.className=(d&&d.ok)?'ok':(d?'bad':'');
  bar.querySelector('.cmd').textContent=cmd||'';
  let t=(d&&d.status)||'—';
  if(d&&d.data!==undefined&&d.data!=='') t+=' | '+d.data;
  if(d&&d.tx) t+=' | TX '+d.tx;
  if(d&&d.rx) t+=' | RX '+d.rx;
  else if(d&&d.rx==='') t+=' | RX —';
  if(d&&d.pan!==undefined) t+=' | pan=0x'+d.pan.toString(16).padStart(4,'0')+' tilt=0x'+d.tilt.toString(16).padStart(4,'0');
  if(d&&d.zoom!==undefined) t+=' z=0x'+Number(d.zoom).toString(16);
  if(d&&d.focus!==undefined) t+=' f=0x'+Number(d.focus).toString(16);
  if(d&&d.iris!==undefined) t+=' i=0x'+Number(d.iris).toString(16);
  if(d&&d.gain!==undefined) t+=' gain='+d.gain+' shut='+d.shutter+' nd='+d.nd;
  if(d&&d.error!==undefined) t+=' err='+d.error;
  if(d&&d.preset!==undefined) t+=' preset='+d.preset;
  if(d&&d.value!==undefined) t+=' value='+d.value;
  bar.querySelector('.st').textContent=t;
  if(d&&d.pan!==undefined){
    $('pos').textContent='pan 0x'+d.pan.toString(16).padStart(4,'0')+
      ' tilt 0x'+d.tilt.toString(16).padStart(4,'0')+
      (d.zoom!==undefined?(' z 0x'+Number(d.zoom).toString(16)):'');
  }
}
async function call(api,cmd,extra){
  const q=new URLSearchParams(Object.assign({cmd},extra||{}));
  try{
    const r=await fetch('/api/'+api+'?'+q,{cache:'no-store'});
    const d=await r.json();
    showBar(api+'/'+cmd,d);
    return d;
  }catch(e){
    showBar(api+'/'+cmd,{ok:false,status:'net'});
  }
}
function ptParams(){
  return{
    rate:$('rate').value,preset:$('pr').value,
    v:$('pan').value,v2:$('tilt').value,v3:$('zoom').value,v4:$('iris').value,
    data:$('focus').value,data2:$('rawpay').value
  };
}
function camParams(){
  return{
    v:$('awc').value,v2:$('scene').value,v3:$('shut').value,v4:$('cdet').value,
    data:$('cgain').value,data2:$('cmiris').value
  };
}
async function ptz(cmd){
  const p=ptParams();
  if(cmd==='preset_mode') p.v=$('prmode').value;
  if(cmd==='preset_speed') p.v=$('prspd').value;
  if(cmd==='preset_speed_unit') p.data=$('prunit').value;
  if(cmd==='lens_ctrl') p.v=$('lensctrl').value;
  if(cmd==='set_pt_w_spd'||cmd==='rel_pt_w_spd'){p.v3=$('apsspd').value;p.v4=$('apstab').value}
  if(cmd==='limitation') p.v=$('lim').value;
  if(cmd==='limit_ctrl'){p.v=$('limax').value;p.v2=$('lim').value}
  if(cmd==='tilt_range') p.v=$('tiltr').value;
  if(cmd==='tally') p.v=$('tally').value;
  if(cmd==='wireless') p.v=$('wl').value;
  if(cmd==='washer') p.v=$('wash').value;
  if(cmd==='option') p.v=$('opt').value;
  if(cmd==='fan'||cmd==='fan2') p.v=$('fan').value;
  if(cmd==='wiper') p.v=$('wip').value;
  if(cmd==='raw'||cmd==='raw_send'){p.data=$('rawcmd').value;p.data2=$('rawpay').value}
  if(cmd==='set_focus') p.v=$('focus').value;
  if(cmd==='set_zoom') p.v=$('zoom').value;
  if(cmd==='set_iris') p.v=$('iris').value;
  return call('ptz',cmd,p);
}
async function cam(cmd){
  const p=camParams();
  if(cmd==='awc_mode') p.v=$('awc').value;
  if(cmd==='scene') p.v=$('scene').value;
  if(cmd==='shutter_mode') p.v=$('shut').value;
  if(cmd==='synchro') p.data=$('sync').value;
  if(cmd==='gain') p.v=$('cgain').value;
  if(cmd==='detail') p.v=$('cdet').value;
  if(cmd==='hd_detail') p.v=$('chdet').value;
  if(cmd==='iris_am') p.v=$('ciris').value;
  if(cmd==='man_iris') p.v=$('cmiris').value;
  if(cmd==='filter') p.v=$('cfilt').value;
  if(cmd==='colorbar') p.v=$('cbar').value;
  if(cmd==='menu') p.v=$('cmenu').value;
  if(cmd==='af') p.v=$('caf').value;
  if(cmd==='negpos') p.v=$('cneg').value;
  if(cmd==='fieldframe') p.v=$('cff').value;
  if(cmd==='chroma') p.v=$('cchr').value;
  if(cmd==='black_shade') p.v=$('cbsh').value;
  if(cmd==='r_gain') p.data=$('crg').value;
  if(cmd==='b_gain') p.data=$('cbg').value;
  if(cmd==='r_ped') p.data=$('crp').value;
  if(cmd==='b_ped') p.data=$('cbp').value;
  if(cmd==='gamma') p.data=$('cgam').value;
  if(cmd==='agc_max') p.data=$('cagc').value;
  if(cmd==='total_dtl') p.data=$('cdtl').value;
  if(cmd==='genlock') p.data=$('cgen').value;
  if(cmd==='monitor'){p.data=$('cdxx').value;p.data2=$('cmon').value}
  if(cmd==='lens_h') p.data=$('clensh').value;
  if(cmd==='scene_data') p.data=$('cscened').value;
  return call('cam',cmd,p);
}
document.querySelectorAll('.tabs button').forEach(b=>{
  b.onclick=()=>{
    document.querySelectorAll('.tabs button').forEach(x=>x.classList.remove('on'));
    document.querySelectorAll('.panel').forEach(x=>x.classList.remove('on'));
    b.classList.add('on');
    $(b.dataset.tab).classList.add('on');
  };
});
document.body.addEventListener('click',e=>{
  const b=e.target.closest('[data-cmd]');
  if(!b||b.hasAttribute('data-hold')) return;
  const api=b.dataset.api||'ptz';
  if(api==='cam') cam(b.dataset.cmd); else ptz(b.dataset.cmd);
});
function bindHold(el){
  const cmd=el.dataset.hold;
  const go=ev=>{ev.preventDefault();ptz(cmd)};
  const stop=ev=>{ev.preventDefault();ptz('stop')};
  el.addEventListener('mousedown',go);
  el.addEventListener('mouseup',stop);
  el.addEventListener('mouseleave',stop);
  el.addEventListener('touchstart',go,{passive:false});
  el.addEventListener('touchend',stop);
  el.addEventListener('touchcancel',stop);
}
document.querySelectorAll('[data-hold]').forEach(bindHold);
async function tick(){
  try{
    const r=await fetch('/api/status',{cache:'no-store'});
    const d=await r.json();
    link.textContent=d.link?'up':'down';
    ip.textContent=d.ip;mask.textContent=d.mask;gw.textContent=d.gw;mac.textContent=d.mac;
    ptm.textContent=d.model;camm.textContent=d.cam||'—';
    rateinfo.textContent=d.rate;up.textContent=d.uptime_s+' s';
    if(d.rate){rate.value=d.rate;spd.textContent=d.rate}
  }catch(e){showBar('status',{ok:false,status:'net'})}
}
tick();setInterval(tick,4000);
</script>
</body>
</html>
)html"
