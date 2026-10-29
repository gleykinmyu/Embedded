R"html(<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>PH350 · E650</title>
<style>
:root{--bg:#0e1116;--panel:#161b22;--fg:#e6edf3;--muted:#8b949e;--line:#30363d;--acc:#2f81f7;--danger:#f85149;--ok:#3fb950;--bar:44px}
*{box-sizing:border-box}
body{margin:0;font-family:Segoe UI,system-ui,sans-serif;background:var(--bg);color:var(--fg);padding-bottom:calc(var(--bar)+14px)}
main{max-width:780px;margin:0 auto;padding:12px 12px 8px}
.top{display:flex;justify-content:space-between;align-items:baseline;gap:8px;margin-bottom:8px}
h1{font-size:17px;margin:0;font-weight:600}
.sub{color:var(--muted);font-size:12px;margin:0}
.tabs{display:flex;gap:4px;margin-bottom:10px;overflow-x:auto;-webkit-overflow-scrolling:touch}
.tabs button{appearance:none;border:1px solid var(--line);background:var(--panel);color:var(--muted);
  border-radius:6px;padding:8px 12px;font-size:13px;cursor:pointer;white-space:nowrap}
.tabs button.on{background:var(--acc);border-color:var(--acc);color:#fff}
.panel{display:none}.panel.on{display:block}
[data-cap].cap-off{display:none!important}
h2{font-size:11px;color:var(--muted);font-weight:600;margin:14px 0 6px;text-transform:uppercase;letter-spacing:.06em}
.hint{color:var(--muted);margin:0 0 8px;font-size:11px;line-height:1.35}
.pads{display:flex;flex-wrap:wrap;justify-content:center;gap:14px;margin-bottom:10px}
.pad{display:grid;grid-template-columns:1fr 1fr 1fr;gap:6px;width:168px}
.pad.rocker{grid-template-columns:1fr;width:72px}
.pad .lbl{text-align:center;font-size:10px;color:var(--muted);text-transform:uppercase;letter-spacing:.04em;padding:2px 0}
.grid{display:flex;flex-wrap:wrap;gap:6px;margin-bottom:6px}
.pad button,.grid button,.row button{appearance:none;border:1px solid var(--line);background:var(--panel);color:var(--fg);
  border-radius:6px;padding:9px 10px;font-size:13px;cursor:pointer}
.pad button{padding:14px 8px;font-size:16px}
.pad button:active,.grid button:active,.row button:active{background:var(--acc);border-color:var(--acc)}
.pad .sp{visibility:hidden}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-bottom:6px}
.row label,.field{display:flex;align-items:center;gap:6px;color:var(--muted);font-size:12px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:10px;margin-bottom:8px}
input,select{background:#0d1117;color:var(--fg);border:1px solid var(--line);border-radius:5px;padding:6px 8px;font-size:13px}
input[type=range]{width:140px;padding:0;accent-color:var(--acc)}
input[type=checkbox]{width:1.1em;height:1.1em;accent-color:var(--acc)}
input[type=number]{width:4.8em}
.slide{display:flex;align-items:center;gap:8px;margin-bottom:8px;width:100%}
.slide .nm{flex:0 0 5.5em;color:var(--muted);font-size:12px}
.slide input[type=range]{flex:1;min-width:100px;max-width:420px}
.slide b{flex:0 0 3.2em;text-align:right;font-size:13px;font-variant-numeric:tabular-nums}
.slide .unit{flex:0 0 2em;color:var(--muted);font-size:11px}
input.wide{width:6.5em}
input.xwide{width:10em}
table{border-collapse:collapse;width:100%}
td{padding:6px 0;border-bottom:1px solid var(--line);font-size:13px}
td:first-child{color:var(--muted);width:6.5em}
.danger{border-color:#6e2a2a!important;background:#2a1515!important}
.danger.on{background:#a32020!important;border-color:#f85149!important;color:#fff}
.banks,.modes{display:flex;flex-wrap:wrap;gap:4px;margin-bottom:8px}
.banks button,.modes button{flex:1;min-width:2.4em;padding:8px 6px;font-size:12px}
.banks button.on,.modes button.on{background:var(--acc);border-color:var(--acc);color:#fff}
.modes button.save.on{background:#238636;border-color:#238636}
.presets{display:grid;grid-template-columns:repeat(5,1fr);gap:6px;margin-bottom:8px}
.presets button{padding:14px 4px;font-size:14px;font-weight:600}
.presets button.sel{outline:2px solid var(--acc);outline-offset:1px}
.presets button.occ{border-color:#3fb950}
#bar{position:fixed;left:0;right:0;bottom:0;min-height:var(--bar);max-height:28vh;overflow:auto;padding:7px 12px;background:#010409;border-top:1px solid var(--line);
  font-family:ui-monospace,Consolas,monospace;font-size:11px;color:var(--muted);display:flex;align-items:flex-start;gap:8px;z-index:9}
#bar .st{color:var(--fg);word-break:break-all;white-space:pre-wrap}
#bar.ok .st{color:var(--ok)}#bar.bad .st{color:var(--danger)}
#bar .cmd{color:var(--acc);flex:0 0 auto}
</style>
</head>
<body>
<main>
<div class="top">
  <div>
    <h1 id="title">AW-PH350E · AW-E650E</h1>
    <p class="sub">Convertible Protocol · live control</p>
  </div>
  <p class="sub" id="linkmini">…</p>
</div>
<div class="tabs">
  <button class="on" data-tab="ptz">PTZ</button>
  <button data-tab="preset">Preset</button>
  <button data-tab="cam">Camera</button>
  <button data-tab="paint">Paint</button>
  <button data-tab="sys">System</button>
</div>

<section id="ptz" class="panel on">
  <div class="card">
    <div class="pads">
      <div class="pad" title="Pan / Tilt">
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
      <div class="pad rocker">
        <div class="lbl">Zoom</div>
        <button data-hold="zoom_tele" title="Tele">▲</button>
        <button data-hold="zoom_wide" title="Wide">▼</button>
      </div>
      <div class="pad rocker">
        <div class="lbl">Focus</div>
        <button data-hold="focus_far" title="Far">▲</button>
        <button data-hold="focus_near" title="Near">▼</button>
      </div>
    </div>
    <div class="row">
      <label>Speed <b id="spd">60</b>
        <input id="rate" type="range" min="1" max="100" value="60" title="Скорость 1…100%">
      </label>
    </div>
  </div>
  <h2>Power</h2>
  <div class="grid">
    <button data-cmd="boot" title="#O2 → wait → QID">Boot</button>
    <button data-cmd="power_on">ON</button>
    <button data-cmd="power_on_cam">ON + CamTX</button>
    <button data-cmd="power_on_nocam">ON no CamTX</button>
    <button class="danger" data-cmd="power_off">OFF</button>
    <button data-cmd="home" title="PH350: recall preset #01 / HE: #APC center">Home</button>
    <button data-cmd="stop">Stop</button>
  </div>
  <div data-cap="abs_pt">
    <h2>Absolute PT (#APC)</h2>
    <p class="hint">Только Integrated PTZ (HE/UE). 0000..FFFF.</p>
    <div class="row">
      <label>Pan <input id="apan" class="wide" value="0x8000"></label>
      <label>Tilt <input id="atilt" class="wide" value="0x8000"></label>
      <button data-cmd="set_pt">Go #APC</button>
    </div>
  </div>
  <div data-cap="lens_q">
    <h2>Lens position (PT)</h2>
    <p class="hint">#AXZ/#AXF/#AXI · 0000..FFFF — integrated PTZ.</p>
    <div class="row">
      <label>Z <input id="zoom" class="wide" value="0x0000"></label>
      <label>F <input id="focus" class="wide" value="0x8000"></label>
      <label>I <input id="iris" class="wide" value="0x8000"></label>
      <button data-cmd="set_lens">Set all</button>
      <button data-cmd="set_zoom">Z</button>
      <button data-cmd="set_focus">F</button>
      <button data-cmd="set_iris">I</button>
    </div>
    <div class="grid">
      <button data-cmd="pos" data-cap="pt_pos">Pos</button>
      <button data-cmd="req_zoom">Q Z</button>
      <button data-cmd="req_focus">Q F</button>
      <button data-cmd="req_iris">Q I</button>
      <button data-cmd="req_lens">Q Lens</button>
    </div>
  </div>
</section>

<section id="preset" class="panel">
  <div class="card">
    <p class="hint">Банк × 10 слотов (1–99). Выбери режим, затем номер. Удаление — с подтверждением.</p>
    <div class="modes" id="prmodes">
      <button type="button" class="on" data-pr-mode="recall">Recall</button>
      <button type="button" class="save" data-pr-mode="save">Save</button>
      <button type="button" class="danger" data-pr-mode="delete">Delete</button>
    </div>
    <div class="banks" id="prbanks"></div>
    <div class="presets" id="prquick"></div>
    <div class="row">
      <span class="hint" id="prinfo">P01 · Recall</span>
      <input id="pr" type="hidden" value="1">
      <button data-cmd="preset_latest">Latest #S</button>
      <button data-cmd="preset_map" data-cap="preset_map" title="#PE">Map #PE</button>
    </div>
  </div>
  <h2>Motion</h2>
  <div class="row">
    <label>Mode
      <select id="prmode" data-send="preset_mode">
        <option value="0">Normal</option><option value="1">Diagonal</option>
      </select>
    </label>
    <label>Speed
      <select id="prspd" data-send="preset_speed">
        <option value="0">Slow</option><option value="1">Mid</option><option value="2">Fast</option>
      </select>
    </label>
    <label>Unit <input id="prunit" class="wide" value="080" maxlength="3" data-send="preset_speed_unit" data-send-param="data"></label>
  </div>
  <div class="row">
    <label>lens ctrl
      <select id="lensctrl" data-send="lens_ctrl"><option value="0">Off</option><option value="1">On</option></select>
    </label>
  </div>
</section>

<section id="cam" class="panel">
  <div class="grid">
    <button data-api="cam" data-cmd="params">Read params</button>
    <button data-api="cam" data-cmd="awc_start">AWC Exec</button>
    <button data-api="cam" data-cmd="abc_start" data-cap="abc">ABC/ABB</button>
    <button data-api="cam" data-cmd="model">Q Model</button>
    <button data-api="cam" data-cmd="version">Q Ver</button>
  </div>
  <pre id="camparams" style="font-size:11px;color:var(--muted);white-space:pre-wrap;margin:6px 0 0;max-height:12em;overflow:auto"></pre>

  <h2>White / scene / shutter</h2>
  <p class="hint">OAW 0..9 · OSF Halogen/Fluorescent/Outdoor/User · OSH 0/1/2/3/5/A/B (E650).</p>
  <div class="row">
    <label>AWC
      <select id="awc" data-api="cam" data-send="awc_mode">
        <option value="0">0 ATW</option><option value="1">1 AWC A</option><option value="2">2 AWC B</option>
        <option value="3">3 3200K</option><option value="4">4 5600K</option><option value="5">5 4500K</option>
        <option value="6">6 6000K</option><option value="7">7 2800K</option><option value="8">8 VAR</option><option value="9">9</option>
      </select>
    </label>
    <label>Scene
      <select id="scene" data-api="cam" data-send="scene">
        <option value="0">Halogen</option><option value="1">Fluorescent</option>
        <option value="2">Outdoor</option><option value="3">User</option>
      </select>
    </label>
  </div>
  <div class="row">
    <label>Shutter
      <select id="shut" data-api="cam" data-send="shutter_mode">
        <option value="0">Off</option>
        <option value="1">1/50</option>
        <option value="2">1/60</option>
        <option value="3">1/100·120</option>
        <option value="5">1/250</option>
        <option value="10">A Synchro</option>
        <option value="11">B ELC</option>
      </select>
    </label>
    <label data-cap="synchro">Synchro <input id="sync" class="wide" value="001" maxlength="3" data-api="cam" data-send="synchro" data-send-param="data" title="OMS 001..0FF"></label>
  </div>

  <h2>Gain / iris / ND</h2>
  <p class="hint">OGU E650: 0..18 dB · AGC=80h. Iris Manual: объём 0..100% → ORV 000..3FF.</p>
  <div class="slide">
    <span class="nm">Gain</span>
    <input id="cgain" type="range" min="0" max="18" value="0" data-api="cam" data-send="gain">
    <b id="cgain_v">0</b><span class="unit">dB</span>
  </div>
  <div class="row">
    <button data-api="cam" data-cmd="agc">AGC ON</button>
    <label>Iris
      <select id="ciris" data-api="cam" data-send="iris_am">
        <option value="0">Manual</option><option value="1">Auto</option>
      </select>
    </label>
  </div>
  <div class="slide">
    <span class="nm">Iris vol</span>
    <input id="cmiris" type="range" min="0" max="100" value="50" data-api="cam" data-send="man_iris" title="0% close … 100% open">
    <b id="cmiris_v">50</b><span class="unit">%</span>
  </div>
  <div class="row">
    <label>ND filter
      <select id="cfilt" data-api="cam" data-send="filter">
        <option value="0">Clear</option><option value="1">1/4</option>
        <option value="2">1/16</option><option value="3">1/64</option>
      </select>
    </label>
    <label>Color bar
      <select id="cbar" data-api="cam" data-send="colorbar">
        <option value="0">Off</option><option value="1">On</option>
      </select>
    </label>
    <label>AF
      <select id="caf" data-api="cam" data-send="af">
        <option value="0">Off</option><option value="1">On</option>
      </select>
    </label>
  </div>

  <h2>Menu / switches</h2>
  <div class="row">
    <label>OSD
      <select id="cmenu" data-api="cam" data-send="menu">
        <option value="0">Off</option><option value="1">On</option>
      </select>
    </label>
    <button data-api="cam" data-cmd="menu_sw">Menu ↑</button>
    <button data-api="cam" data-cmd="item_sw">Item ↓</button>
    <button data-api="cam" data-cmd="yes_sw">Yes +</button>
    <button data-api="cam" data-cmd="no_sw">No −</button>
  </div>
  <div class="row">
    <label data-cap="neg_pos">Neg/Pos
      <select id="cneg" data-api="cam" data-send="negpos">
        <option value="0">Positive</option><option value="1">Negative</option>
      </select>
    </label>
    <label data-cap="field_frame">Field/Frame
      <select id="cff" data-api="cam" data-send="fieldframe">
        <option value="0">Field</option><option value="1">Frame</option>
      </select>
    </label>
    <label data-cap="black_shade">Black shade
      <select id="cbsh" data-api="cam" data-send="black_shade">
        <option value="0">Off</option><option value="1">On</option>
      </select>
    </label>
  </div>
</section>

<section id="paint" class="panel">
  <p class="hint">OGR/OGB/ORD/OBD: −30…+30 · ODT Off/Low/High · OCG −3…+3.</p>
  <div class="card" data-cap="paint">
    <div class="slide">
      <span class="nm">R Gain</span>
      <input id="crg" type="range" min="-30" max="30" value="0" data-api="cam" data-send="r_gain" data-paint>
      <b id="crg_v">0</b>
    </div>
    <div class="slide">
      <span class="nm">B Gain</span>
      <input id="cbg" type="range" min="-30" max="30" value="0" data-api="cam" data-send="b_gain" data-paint>
      <b id="cbg_v">0</b>
    </div>
    <div class="slide">
      <span class="nm">R Ped</span>
      <input id="crp" type="range" min="-30" max="30" value="0" data-api="cam" data-send="r_ped" data-paint>
      <b id="crp_v">0</b>
    </div>
    <div class="slide">
      <span class="nm">B Ped</span>
      <input id="cbp" type="range" min="-30" max="30" value="0" data-api="cam" data-send="b_ped" data-paint>
      <b id="cbp_v">0</b>
    </div>
    <div class="slide" data-cap="chroma">
      <span class="nm">Chroma</span>
      <input id="cchr" type="range" min="0" max="6" value="3" data-api="cam" data-send="chroma" title="0=−3 … 3=0 … 6=+3">
      <b id="cchr_v">0</b>
    </div>
  </div>
  <div class="row">
    <label>Detail
      <select id="cdet" data-api="cam" data-send="detail">
        <option value="0">Off</option><option value="1">Low</option><option value="2">High</option>
      </select>
    </label>
    <label data-cap="hd_detail">HD Detail
      <select id="chdet" data-api="cam" data-send="hd_detail" title="В основном HE870">
        <option value="0">Off</option><option value="1">Low</option><option value="2">High</option>
      </select>
    </label>
  </div>
  <h2>OSD / menu extras</h2>
  <div class="row">
    <label data-cap="gamma">Gamma <input id="cgam" class="wide" value="0" data-api="cam" data-send="gamma" data-send-param="data"></label>
    <label data-cap="agc_max">AGC max <input id="cagc" class="wide" value="0" data-api="cam" data-send="agc_max" data-send-param="data"></label>
    <label data-cap="total_dtl">Total DTL <input id="cdtl" class="wide" value="0" data-api="cam" data-send="total_dtl" data-send-param="data"></label>
    <label data-cap="genlock">Genlock <input id="cgen" class="wide" value="0" data-api="cam" data-send="genlock" data-send-param="data"></label>
  </div>
  <div class="row">
    <label data-cap="monitor">dxx <input id="cdxx" class="wide" value="00"></label>
    <label data-cap="monitor">mon <input id="cmon" class="wide" value="0"></label>
    <button data-api="cam" data-cmd="monitor" data-cap="monitor">Monitor</button>
    <label data-cap="lens_h">lens H <input id="clensh" class="wide" value="00" data-api="cam" data-send="lens_h" data-send-param="data"></label>
    <label>scene data <input id="cscened" class="wide" value="0" data-api="cam" data-send="scene_data" data-send-param="data"></label>
  </div>
</section>

<section id="sys" class="panel">
  <h2>Models</h2>
  <p class="hint">Выбор класса протокола (NVS). Меняет доступные команды в UI по карте PDF.</p>
  <div class="card">
    <div class="row">
      <label>PT
        <select id="ptmodel">
          <option value="ph350">AW-PH350E</option>
          <option value="ph360">AW-PH360</option>
          <option value="ph650">AW-PH650</option>
          <option value="ph405">AW-PH405</option>
          <option value="he130">AW-HE130 PT</option>
          <option value="he870">AW-HE870 PT</option>
          <option value="ue150">AW-UE150 PT</option>
        </select>
      </label>
      <label>Camera
        <select id="cammodel">
          <option value="e650">AW-E650E</option>
          <option value="e600">AW-E600</option>
          <option value="he130">AW-HE130</option>
          <option value="he870">AW-HE870</option>
          <option value="ue150">AW-UE150</option>
        </select>
      </label>
      <button data-cmd="set_models" id="savemodels">Save</button>
    </div>
    <p class="hint" id="pairhint"></p>
  </div>
  <h2>Axis invert</h2>
  <p class="hint">Логические направления UI ↔ физика головки. Сохраняется в NVS (и для лимитов #L).</p>
  <div class="card">
    <div class="row">
      <label><input id="inv_pan" type="checkbox"> Invert Pan</label>
      <label><input id="inv_tilt" type="checkbox"> Invert Tilt</label>
      <label><input id="inv_zoom" type="checkbox"> Invert Zoom</label>
      <label><input id="inv_focus" type="checkbox"> Invert Focus</label>
    </div>
  </div>
  <h2>Limits / options</h2>
  <p class="hint">PH: подъехать к упору → Toggle #L1..4. #LC/#APC/#PE — Integrated PTZ (HE/UE).</p>
  <div class="row">
    <label data-cap="limit_toggle">axis
      <select id="limax">
        <option value="1">TiltUp #L1</option><option value="2">TiltDown #L2</option>
        <option value="3">PanLeft #L3</option><option value="4">PanRight #L4</option>
      </select>
    </label>
    <button data-cmd="limit_toggle" data-cap="limit_toggle">Toggle #L</button>
    <label data-cap="limit_ctrl">#LC
      <select id="limact"><option value="1">Set</option><option value="0">Release</option></select>
    </label>
    <button data-cmd="limit_ctrl" data-cap="limit_ctrl">Apply #LC</button>
  </div>
  <div class="row">
    <label data-cap="tilt_range">Tilt range
      <select id="tiltr" data-send="tilt_range"><option value="0">190°</option><option value="1">300°</option></select>
    </label>
    <label data-cap="tally">Tally <select id="tally" data-send="tally"><option value="0">Off</option><option value="1">On</option></select></label>
    <label data-cap="wireless">Wireless <select id="wl" data-send="wireless"><option value="0">Off</option><option value="1">On</option></select></label>
    <label data-cap="washer">Washer <select id="wash" data-send="washer"><option value="0">Off</option><option value="1">On</option></select></label>
    <label data-cap="option">Option <select id="opt" data-send="option"><option value="0">Off</option><option value="1">On</option></select></label>
  </div>
  <div class="row">
    <label data-cap="fan">Fan
      <select id="fan" data-send="fan">
        <option value="0">Auto</option><option value="1">High</option><option value="2">Mid</option><option value="3">Low</option>
      </select>
    </label>
    <button data-cmd="fan2" data-cap="fan">Fan2</button>
    <label data-cap="wiper">Wiper
      <select id="wip" data-send="wiper">
        <option value="0">Off</option><option value="1">Fast</option><option value="2">Slow</option>
      </select>
    </label>
  </div>
  <h2>Queries</h2>
  <div class="grid">
    <button data-cmd="pos_disp" data-cap="pos_disp">Pos disp</button>
    <button data-cmd="req_exposure" data-cap="exposure_q">Exposure</button>
    <button data-cmd="req_error">Error</button>
    <button data-cmd="req_landing" data-cap="landing">Landing</button>
    <button data-cmd="req_ver" data-cap="software_ver">Version</button>
    <button data-cmd="req_heater" data-cap="heater">Heater</button>
    <button data-cmd="req_defrost" data-cap="defroster">Defroster</button>
  </div>
  <h2>Raw PT</h2>
  <div class="row">
    <label>cmd <input id="rawcmd" class="wide" value="O" maxlength="8"></label>
    <label>payload <input id="rawpay" class="xwide" value="1"></label>
    <button data-cmd="raw">Transact</button>
    <button data-cmd="raw_send">Send</button>
  </div>
  <h2>Info</h2>
  <table>
    <tr><td>Link</td><td id="link">…</td></tr>
    <tr><td>IP</td><td id="ip">…</td></tr>
    <tr><td>Mask</td><td id="mask">…</td></tr>
    <tr><td>GW</td><td id="gw">…</td></tr>
    <tr><td>MAC</td><td id="mac">…</td></tr>
    <tr><td>PT</td><td id="ptm">…</td></tr>
    <tr><td>PT link</td><td id="ptlink">…</td></tr>
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
function paintHex(n){
  let v=parseInt(n,10); if(isNaN(v)) v=0;
  if(v<-30)v=-30; if(v>30)v=30;
  return (v+30).toString(16).toUpperCase().padStart(2,'0');
}
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
  if(d&&d.limit!==undefined) t+=' axis='+d.axis+' limit='+d.limit;
  if(d&&(d.awc!==undefined||d.model!==undefined||d.version!==undefined||d.gain_db!==undefined)){
    const skip={ok:1,status:1,tx:1,rx:1,data:1};
    const lines=Object.keys(d).filter(k=>!skip[k]).map(k=>k+'='+d[k]);
    const el=$('camparams'); if(el) el.textContent=lines.join('\n');
    t+=' | params '+lines.length;
    applyCamParams(d);
  }
  bar.querySelector('.st').textContent=t;
  if(d&&d.pan!==undefined){
    $('pos').textContent='pan 0x'+d.pan.toString(16).padStart(4,'0')+
      ' tilt 0x'+d.tilt.toString(16).padStart(4,'0')+
      (d.zoom!==undefined?(' z 0x'+Number(d.zoom).toString(16)):'');
  }
  if(cmd==='ptz/preset_map'&&d&&d.ok&&Array.isArray(d.presets)&&window.PresetUI)
    window.PresetUI.applyMap(d.presets);
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
function val(id, def=''){ const el=$(id); return el ? el.value : def; }
function ptParams(){
  return{
    rate:val('rate','60'), preset:val('pr','1'),
    v:val('pan'), v2:val('tilt'), v3:val('zoom'), v4:val('iris'),
    data:val('focus'), data2:val('rawpay')
  };
}
async function ptz(cmd){
  const p=ptParams();
  if(cmd==='preset_mode') p.v=$('prmode').value;
  if(cmd==='preset_speed') p.v=$('prspd').value;
  if(cmd==='preset_speed_unit') p.data=$('prunit').value;
  if(cmd==='lens_ctrl') p.v=$('lensctrl').value;
  if(cmd==='limit_toggle'||cmd==='limitation'||cmd==='limit_ctrl') p.v=$('limax').value;
  if(cmd==='limit_ctrl') p.v2=$('limact').value;
  if(cmd==='set_pt'){p.v=val('apan','0x8000');p.v2=val('atilt','0x8000')}
  if(cmd==='tilt_range') p.v=$('tiltr').value;
  if(cmd==='tally') p.v=$('tally').value;
  if(cmd==='wireless') p.v=$('wl').value;
  if(cmd==='washer') p.v=$('wash').value;
  if(cmd==='option') p.v=$('opt').value;
  if(cmd==='fan'||cmd==='fan2') p.v=$('fan').value;
  if(cmd==='wiper') p.v=$('wip').value;
  if(cmd==='raw'||cmd==='raw_send'){p.data=$('rawcmd').value;p.data2=$('rawpay').value}
  if(cmd==='set_models'){p.data=$('ptmodel').value;p.data2=$('cammodel').value}
  if(cmd==='set_focus') p.v=$('focus').value;
  if(cmd==='set_zoom') p.v=$('zoom').value;
  if(cmd==='set_iris') p.v=$('iris').value;
  const d=await call('ptz',cmd,p);
  if(cmd==='set_models'&&d&&d.ok) tick();
  return d;
}
async function cam(cmd){
  const p={};
  if(cmd==='awc_mode') p.v=$('awc').value;
  if(cmd==='scene') p.v=$('scene').value;
  if(cmd==='shutter_mode') p.v=$('shut').value;
  if(cmd==='synchro') p.data=$('sync').value;
  if(cmd==='gain') p.v=$('cgain').value;
  if(cmd==='detail') p.v=$('cdet').value;
  if(cmd==='hd_detail') p.v=$('chdet').value;
  if(cmd==='iris_am') p.v=$('ciris').value;
  if(cmd==='man_iris'){
    const pct=Math.max(0,Math.min(100,parseInt($('cmiris').value,10)||0));
    p.v=String(Math.round(pct*65535/100));
  }
  if(cmd==='filter') p.v=$('cfilt').value;
  if(cmd==='colorbar') p.v=$('cbar').value;
  if(cmd==='menu') p.v=$('cmenu').value;
  if(cmd==='af') p.v=$('caf').value;
  if(cmd==='negpos') p.v=$('cneg').value;
  if(cmd==='fieldframe') p.v=$('cff').value;
  if(cmd==='chroma') p.v=$('cchr').value;
  if(cmd==='black_shade') p.v=$('cbsh').value;
  if(cmd==='r_gain') p.data=paintHex($('crg').value);
  if(cmd==='b_gain') p.data=paintHex($('cbg').value);
  if(cmd==='r_ped') p.data=paintHex($('crp').value);
  if(cmd==='b_ped') p.data=paintHex($('cbp').value);
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
function sendFromEl(el){
  const cmd=el.dataset.send; if(!cmd) return;
  const api=el.dataset.api||'ptz';
  if(api==='cam') cam(cmd); else ptz(cmd);
}
const _slideDebounce=new WeakMap();
function updateSlideLabel(el){
  const id=el.id, v=el.value;
  if(id==='rate'){ spd.textContent=v; return; }
  if(id==='cgain'&&$('cgain_v')) $('cgain_v').textContent=v;
  if(id==='cmiris'&&$('cmiris_v')) $('cmiris_v').textContent=v;
  if(id==='cchr'&&$('cchr_v')) $('cchr_v').textContent=String(parseInt(v,10)-3);
  if((id==='crg'||id==='cbg'||id==='crp'||id==='cbp')&&$(id+'_v')) $(id+'_v').textContent=v;
}
function onSlideInput(el){
  updateSlideLabel(el);
  if(el.id==='rate') return; /* persist on change */
  if(!el.dataset.send) return;
  clearTimeout(_slideDebounce.get(el));
  _slideDebounce.set(el,setTimeout(()=>sendFromEl(el),120));
}
document.body.addEventListener('change',e=>{
  const el=e.target.closest('[data-send]');
  if(!el) return;
  if(el.matches('input[type=number]')) return;
  if(el.matches('input[type=range]')){
    if(el.id==='rate'){
      window._rateLocal=true;
      call('ptz','set_rate',{rate:el.value}).then(d=>{ if(!d||!d.ok) window._rateLocal=false; });
      return;
    }
    /* range уже ушёл с debounce на input; на change — догон без дубля если тот же */
    clearTimeout(_slideDebounce.get(el));
    sendFromEl(el);
    return;
  }
  sendFromEl(el);
  if(el.id==='ciris'){
    $('cmiris').disabled=el.value==='1';
  }
});
document.body.addEventListener('input',e=>{
  const el=e.target;
  if(el.matches('input[type=range]')){
    if(el.id==='rate') window._rateLocal=true;
    onSlideInput(el);
    return;
  }
  if(el.matches('input[type=number][data-send]')) sendFromEl(el);
});
['cgain','cmiris','crg','cbg','crp','cbp','cchr','rate'].forEach(id=>{
  const el=$(id); if(el) updateSlideLabel(el);
});
$('cmiris').disabled=$('ciris').value==='1';
function bindHold(el){
  const cmd=el.dataset.hold;
  let down=false;
  const go=ev=>{
    if(ev.pointerType==='mouse' && ev.button!==0) return;
    ev.preventDefault();
    try{ el.setPointerCapture(ev.pointerId); }catch(err){}
    if(down) return;
    down=true;
    ptz(cmd);
  };
  const halt=ev=>{
    if(!down) return;
    down=false;
    if(ev) ev.preventDefault();
    ptz('stop');
  };
  el.addEventListener('pointerdown',go);
  el.addEventListener('pointerup',halt);
  el.addEventListener('pointercancel',halt);
  window.addEventListener('pointerup',halt);
}
document.querySelectorAll('[data-hold]').forEach(bindHold);
const PresetUI=(()=>{
  const BANKS=10, SLOTS=10, MAX_PRESET=99;
  let bank=0, mode='recall', selected=1;
  const occ=new Array(100).fill(false);
  const banksEl=$('prbanks'), padEl=$('prquick'), info=$('prinfo');
  function absNum(slot){ return bank*SLOTS + slot + 1; }
  function syncInfo(){
    $('pr').value=String(selected);
    const m=mode==='recall'?'Recall':(mode==='save'?'Save':'Delete');
    info.textContent='P'+String(selected).padStart(2,'0')+' · '+m+' · bank '+(bank+1);
  }
  function paintPad(){
    padEl.querySelectorAll('button').forEach((b,i)=>{
      const n=absNum(i);
      b.hidden=n>MAX_PRESET;
      b.dataset.n=String(n);
      b.textContent=String(n).padStart(2,'0');
      b.classList.toggle('sel', n===selected);
      b.classList.toggle('occ', n<=MAX_PRESET && !!occ[n-1]);
    });
    banksEl.querySelectorAll('button').forEach((b,i)=>b.classList.toggle('on', i===bank));
    document.querySelectorAll('#prmodes [data-pr-mode]').forEach(b=>b.classList.toggle('on', b.dataset.prMode===mode));
    syncInfo();
  }
  function setBank(b){ bank=Math.max(0,Math.min(BANKS-1,b)); paintPad(); }
  function setMode(m){ mode=m; paintPad(); }
  async function runSlot(n){
    if(n<1||n>MAX_PRESET) return;
    selected=n; $('pr').value=String(n); paintPad();
    if(mode==='recall') return ptz('preset_recall');
    if(mode==='save'){
      const d=await ptz('preset_save');
      if(d&&d.ok){ occ[n-1]=true; paintPad(); }
      return d;
    }
    if(mode==='delete'){
      if(!confirm('Удалить пресет P'+String(n).padStart(2,'0')+'?')) return;
      const d=await ptz('preset_delete');
      if(d&&d.ok){ occ[n-1]=false; paintPad(); }
      return d;
    }
  }
  function applyMap(list){
    occ.fill(false);
    (list||[]).forEach(n=>{ if(n>=1&&n<=MAX_PRESET) occ[n-1]=true; });
    paintPad();
  }
  for(let i=0;i<BANKS;i++){
    const b=document.createElement('button');
    b.type='button';
    b.textContent=String(i+1);
    const lo=i*SLOTS+1, hi=Math.min(lo+SLOTS-1, MAX_PRESET);
    b.title='Presets '+lo+'–'+hi;
    b.onclick=()=>setBank(i);
    banksEl.appendChild(b);
  }
  for(let i=0;i<SLOTS;i++){
    const b=document.createElement('button');
    b.type='button';
    b.onclick=()=>runSlot(absNum(i));
    padEl.appendChild(b);
  }
  document.querySelectorAll('#prmodes [data-pr-mode]').forEach(b=>{
    b.onclick=()=>setMode(b.dataset.prMode);
  });
  paintPad();
  window.PresetUI={applyMap, paintPad};
})();
function applyInvertUi(d){
  if(!d) return;
  if(typeof d.inv_pan==='boolean') $('inv_pan').checked=d.inv_pan;
  if(typeof d.inv_tilt==='boolean') $('inv_tilt').checked=d.inv_tilt;
  if(typeof d.inv_zoom==='boolean') $('inv_zoom').checked=d.inv_zoom;
  if(typeof d.inv_focus==='boolean') $('inv_focus').checked=d.inv_focus;
}
function applyCaps(d){
  const caps=Object.assign({}, d&&d.pt_caps, d&&d.cam_caps);
  document.querySelectorAll('[data-cap]').forEach(el=>{
    const key=el.getAttribute('data-cap');
    if(caps[key]===undefined){ el.classList.remove('cap-off'); return; }
    el.classList.toggle('cap-off', !caps[key]);
  });
}
function applyModels(d){
  if(!d) return;
  if(d.pt_id && $('ptmodel')) $('ptmodel').value=d.pt_id;
  if(d.cam_id && $('cammodel')) $('cammodel').value=d.cam_id;
  if(d.model||d.cam){
    const t=(d.model||'')+(d.cam?(' · '+d.cam):'');
    if(t){ $('title').textContent=t; document.title=t; }
  }
  updatePairHint();
}
function setSel(id,v){ const el=$(id); if(el&&v!==undefined&&v!==null&&typeof v!=='string') el.value=String(v); else if(el&&typeof v==='string'&&v!=='timeout'&&v!=='io'&&v!=='nack'&&v!=='param') el.value=v; }
function applyCamParams(d){
  if(!d||!d.ok) return;
  setSel('awc',d.awc); setSel('scene',d.scene); setSel('shut',d.shutter);
  setSel('cdet',d.detail); setSel('cbar',d.colorbar); setSel('cmenu',d.menu);
  setSel('caf',d.af); setSel('cfilt',d.filter); setSel('cneg',d.negpos);
  setSel('cff',d.fieldframe); setSel('ciris',d.iris_am);
  if(typeof d.gain_db==='number' && $('cgain')){
    $('cgain').value=d.gain_db; if($('cgain_v')) $('cgain_v').textContent=d.gain_db;
  }
  if(typeof d.iris==='number' && $('cmiris')){
    const pct=Math.max(0,Math.min(100,Math.round(d.iris*100/65535)));
    $('cmiris').value=pct; if($('cmiris_v')) $('cmiris_v').textContent=pct;
  }
  if(typeof d.chroma==='number' && $('cchr')){
    $('cchr').value=d.chroma; if($('cchr_v')) $('cchr_v').textContent=(d.chroma-3);
  }
  // OGR: 0x1E = 0 → slider = code-30
  const paintToSlider=(code,id,vid)=>{
    if(typeof code!=='number'||!$(id)) return;
    const s=Math.max(-30,Math.min(30,code-30));
    $(id).value=s; if($(vid)) $(vid).textContent=s;
  };
  paintToSlider(d.r_gain,'crg','crg_v');
  paintToSlider(d.b_gain,'cbg','cbg_v');
}
const PAIR={ph350:'e650',ph360:'e650',ph650:'e650',ph405:'e650',he130:'he130',he870:'he870',ue150:'ue150'};
function updatePairHint(){
  const h=$('pairhint'); if(!h) return;
  const pt=$('ptmodel')&&$('ptmodel').value, cam=$('cammodel')&&$('cammodel').value;
  if(!pt||!cam){ h.textContent=''; return; }
  const sug=PAIR[pt];
  if(sug && sug!==cam) h.textContent='⚠ Обычно с '+pt+' ставят cam='+sug;
  else if((pt==='he130'||pt==='he870'||pt==='ue150') && cam===pt) h.textContent='✓ Пара PTZ + camera совпадает';
  else if(pt.indexOf('ph')===0 && (cam==='e650'||cam==='e600')) h.textContent='✓ Головка PH + студийная камера';
  else h.textContent='';
}
['ptmodel','cammodel'].forEach(id=>{
  const el=$(id); if(!el) return;
  el.addEventListener('change',()=>{
    if(id==='ptmodel' && PAIR[el.value]) $('cammodel').value=PAIR[el.value];
    updatePairHint();
  });
});
async function saveInvert(){
  window._invLocal=true;
  const d=await call('ptz','set_invert',{
    v:$('inv_pan').checked?1:0,
    v2:$('inv_tilt').checked?1:0,
    v3:$('inv_zoom').checked?1:0,
    v4:$('inv_focus').checked?1:0
  });
  if(d&&d.ok) applyInvertUi(d);
  else window._invLocal=false;
}
['inv_pan','inv_tilt','inv_zoom','inv_focus'].forEach(id=>{
  $(id).addEventListener('change',()=>saveInvert());
});
async function tick(){
  try{
    const r=await fetch('/api/status',{cache:'no-store'});
    const d=await r.json();
    link.textContent=d.link?'up':'down';
    const ptOk=!!d.pt_alive;
    const pwr=['Off','On','On+Cam','On no Cam'][d.pt_power]||'?';
    linkmini.textContent=(d.link?'●':'○')+(ptOk?'●':'○')+' '+(d.ip||'');
    linkmini.title='ETH '+(d.link?'up':'down')+' · PT '+(ptOk?('alive p'+d.pt_power+' '+pwr):'DEAD');
    ip.textContent=d.ip;mask.textContent=d.mask;gw.textContent=d.gw;mac.textContent=d.mac;
    ptm.textContent=d.model;camm.textContent=d.cam||'—';
    if($('ptlink')){
      $('ptlink').textContent=ptOk
        ?('alive · p'+d.pt_power+' ('+pwr+')'+(d.pt_powered?'':' · off'))
        :('DEAD · fails '+(d.pt_fail||0));
      $('ptlink').style.color=ptOk?'var(--ok)':'var(--danger)';
    }
    rateinfo.textContent=d.rate;up.textContent=d.uptime_s+' s';
    /* не перетирать слайдер, пока пользователь крутит / уже задал локально */
    if(d.rate && !window._rateLocal && document.activeElement!==rate){
      rate.value=d.rate; spd.textContent=d.rate;
    }
    if(!window._invLocal) applyInvertUi(d);
    if(document.activeElement!==$('ptmodel') && document.activeElement!==$('cammodel'))
      applyModels(d);
    applyCaps(d);
  }catch(e){showBar('status',{ok:false,status:'net'})}
}
tick();setInterval(tick,4000);
</script>
</body>
</html>
)html"
