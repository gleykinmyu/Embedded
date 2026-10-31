const $=id=>document.getElementById(id);
/** Painting UI −150…+150 step5 → wire 00h…3Ch (E650/HE: Data×5). */
function paintHex(n){
  let v=parseInt(n,10); if(isNaN(v)) v=0;
  v=Math.round(v/5);
  if(v<-30)v=-30; if(v>30)v=30;
  return (v+30).toString(16).toUpperCase().padStart(2,'0');
}
function paintFromWire(code){
  const c=parseInt(code,10);
  if(isNaN(c)) return 0;
  return Math.max(-150,Math.min(150,(c-30)*5));
}
function showBar(cmd,d){
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
  if(d&&(d.awc!==undefined||d.model!==undefined||d.version!==undefined||d.gain_db!==undefined||d.gain_mode!==undefined)){
    const skip={ok:1,status:1,tx:1,rx:1,data:1,stream:1,phase:1,done:1};
    const lines=Object.keys(d).filter(k=>!skip[k]).map(k=>k+'='+d[k]);
    const el=$('camparams'); if(el) el.textContent=lines.join('\n');
    t+=' | params '+lines.length;
    applyCamParams(d);
  }
  const kind=(d&&d.ok)?'ok':(d?'err':'warn');
  const msg=(cmd?cmd+' · ':'')+t;
  if(window.IptvNote) IptvNote.show(msg, kind);
  else {
    const note=$('note');
    if(note){
      const te=note.querySelector('.note-text')||note;
      te.textContent=msg;
      note.className='note note-bar '+kind;
      note.classList.remove('hidden');
    }
  }
  if(d&&d.pan!==undefined && $('pos')){
    $('pos').textContent='pan 0x'+d.pan.toString(16).padStart(4,'0')+
      ' tilt 0x'+d.tilt.toString(16).padStart(4,'0')+
      (d.zoom!==undefined?(' z 0x'+Number(d.zoom).toString(16)):'');
  }
  if(d&&d.ok){
    if(d.zoom!==undefined && $('zoom')) $('zoom').value='0x'+Number(d.zoom).toString(16).toUpperCase().padStart(4,'0');
    if(d.focus!==undefined && $('focus')) $('focus').value='0x'+Number(d.focus).toString(16).toUpperCase().padStart(4,'0');
    if(d.iris!==undefined && $('iris')) $('iris').value='0x'+Number(d.iris).toString(16).toUpperCase().padStart(4,'0');
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
async function cam(cmd, el){
  const p={};
  let apiCmd=cmd;
  if(cmd==='menu_on'){ apiCmd='menu'; p.v='1'; }
  if(cmd==='menu_off'){ apiCmd='menu'; p.v='0'; }
  if(cmd==='awc_mode') p.v=$('awc').value;
  if(cmd==='scene') p.v=$('scene').value;
  if(cmd==='shutter_mode') p.v=$('shut').value;
  if(cmd==='synchro'){
    const n=Math.max(1,Math.min(255,parseInt($('sync').value,10)||1));
    p.data=n.toString(16).toUpperCase().padStart(3,'0');
  }
  if(cmd==='gain') p.v=$('cgain').value;
  if(cmd==='gain_mode'){
    if($('cgain_mode')&&$('cgain_mode').value==='agc'){ apiCmd='agc'; }
    else { apiCmd='gain'; p.v=$('cgain')?$('cgain').value:'0'; }
  }
  if(cmd==='detail') p.v=$('cdet').value;
  if(cmd==='hd_detail') p.v=$('chdet').value;
  if(cmd==='iris_am') p.v=$('ciris').value;
  if(cmd==='man_iris'){
    const pct=Math.max(0,Math.min(100,parseInt($('cmiris').value,10)||0));
    p.v=String(Math.round(pct*65535/100));
  }
  if(cmd==='filter') p.v=$('cfilt').value;
  if(cmd==='colorbar') p.v=$('cbar').value;
  if(cmd==='menu') p.v=$('cmenu')?$('cmenu').value:p.v;
  if(cmd==='af') p.v=$('caf').value;
  if(cmd==='negpos') p.v=$('cneg').value;
  if(cmd==='fieldframe') p.v=$('cff').value;
  if(cmd==='chroma') p.v=$('cchr').value;
  if(cmd==='black_shade') p.v=$('cbsh').value;
  if(cmd==='r_gain') p.data=paintHex($('crg').value);
  if(cmd==='b_gain') p.data=paintHex($('cbg').value);
  if(cmd==='r_ped') p.data=paintHex($('crp').value);
  if(cmd==='b_ped') p.data=paintHex($('cbp').value);
  if(cmd==='t_ped') p.data=paintHex($('ctped').value);
  if(cmd==='gamma') p.data=$('cgam').value;
  if(cmd==='agc_max') p.data=$('cagc').value;
  if(cmd==='total_dtl') p.data=$('cdtl').value;
  if(cmd==='genlock') p.data=$('cgen').value;
  if(cmd==='dig_gain') p.v=$('cdig')?$('cdig').value:(el&&el.value);
  if(cmd==='light_area') p.v=$('carea')?$('carea').value:(el&&el.value);
  if(cmd==='light_peak'){
    const n=Math.max(0,Math.min(100,parseInt((el&&el.value)||($('cpeak')&&$('cpeak').value)||50,10)));
    p.data=n.toString(16).toUpperCase().padStart(2,'0');
  }
  if(cmd==='dnr3d') p.v=$('c3dnr')?$('c3dnr').value:(el&&el.value);
  if(cmd==='osd_set'){
    const src=el||document.activeElement;
    p.v=(src&&src.dataset&&src.dataset.key)||'';
    let v=src&&src.value;
    if(src&&src.type==='range'&&src.dataset.key==='airis'){
      v=Math.max(0,Math.min(100,parseInt(v,10)||0)).toString(16).toUpperCase().padStart(2,'0');
    }else if(src&&src.type==='range'&&src.dataset.key==='wclip'){
      v=Math.max(0,Math.min(15,parseInt(v,10)||0)).toString(16).toUpperCase().padStart(2,'0');
    }else if(src&&src.type==='range'&&String(src.dataset.key||'').indexOf('mx_')===0){
      v=Math.max(0,Math.min(62,parseInt(v,10)||0)).toString(16).toUpperCase().padStart(2,'0');
    }
    p.data=v||'00';
  }
  if(cmd==='monitor'){p.data=$('cdxx').value;p.data2=$('cmon').value}
  if(cmd==='lens_h') p.data=$('clensh').value;
  if(cmd==='scene_data') p.data=$('cscened').value;
  if(cmd==='raw'||cmd==='raw_send'){
    p.data=val('camrawcmd','QAW').trim().toUpperCase();
    p.data2=val('camrawpay','');
  }
  const d=await call('cam',apiCmd,p);
  if((cmd==='menu_on'||cmd==='menu_off') && d&&d.ok) setOsdUi(cmd==='menu_on');
  if(cmd==='iris_am'||cmd==='shutter_mode'||cmd==='agc'||cmd==='gain'||cmd==='gain_mode'||cmd==='awc_mode'){
    if(cmd==='gain_mode'||cmd==='agc'||cmd==='gain'){
      window._gainAgc=($('cgain_mode')&&$('cgain_mode').value==='agc');
    }
    updateCamDeps();
  }
  return d;
}
/** Условная видимость как в меню E650/HE870. */
function updateCamDeps(){
  const irisAm=$('ciris')?$('ciris').value:'0';
  const shut=$('shut')?$('shut').value:'';
  const agc=($('cgain_mode')&&$('cgain_mode').value==='agc')||!!window._gainAgc;
  window._gainAgc=!!agc;
  const awc=$('awc')?String($('awc').value):'';
  const camId=(window._rangesCamId||($('cammodel')&&$('cammodel').value)||'').toLowerCase();
  const synV=window._shutSynchroV;
  const shutSyn=synV!=null && synV!=='' && shut===String(synV);
  /* E650 QAW: 0=ATW,2/3=A/B,4/5=3200/5600. HE: 0=ATW,1/2=A/B,3/4=3200/5600 */
  const awcHe=/^(he|ue)/.test(camId);
  const awcAtw=(awc==='0');
  const awcMem=awcHe?(awc==='1'||awc==='2'):(awc==='2'||awc==='3');
  const awcPreset=awcHe?(awc==='3'||awc==='4'):(awc==='4'||awc==='5');

  const camOk=(el)=>{
    const raw=el.getAttribute('data-cam');
    if(!raw) return true;
    const list=raw.toLowerCase().split(/\s+/);
    return list.includes(camId)||list.some(x=>camId.indexOf(x)===0);
  };
  const depOk=(el)=>{
    const dep=el.getAttribute('data-dep');
    if(!dep) return true;
    if(dep==='iris:manual') return irisAm==='0';
    if(dep==='iris:auto') return irisAm==='1';
    if(dep==='gain:manual') return !agc;
    if(dep==='gain:agc') return !!agc;
    if(dep==='shut:synchro') return shutSyn;
    if(dep==='awc:atw') return awcAtw;
    if(dep==='awc:mem') return awcMem;
    if(dep==='awc:nopreset') return !awcPreset;
    return true;
  };
  document.querySelectorAll('[data-dep],[data-cam]').forEach(el=>{
    el.hidden=!(camOk(el)&&depOk(el));
  });
  if($('cmiris')) $('cmiris').disabled=(irisAm==='1');
  if($('cgain')) $('cgain').disabled=agc;
  const awcExec=$('awc_exec');
  if(awcExec) awcExec.hidden=!awcMem;
}
function updateExposureUi(){ updateCamDeps(); }
document.querySelectorAll('.tabs .tab').forEach(b=>{
  b.onclick=()=>{
    document.querySelectorAll('.tabs .tab').forEach(x=>x.classList.remove('active'));
    document.querySelectorAll('.tab-panel').forEach(x=>{ x.hidden=true; });
    b.classList.add('active');
    const pan=$(b.dataset.tab);
    if(pan) pan.hidden=false;
    if(b.dataset.tab==='cam') loadCamParams(true);
    if(b.dataset.tab==='osd') applyOsdLabels();
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
  if(api==='cam') cam(cmd, el); else ptz(cmd);
}
const _slideDebounce=new WeakMap();
function updateSlideLabel(el){
  const id=el.id, v=el.value;
  if(id==='rate'){ spd.textContent=v; return; }
  if(id==='cgain'&&$('cgain_v')) $('cgain_v').textContent=v;
  if(id==='cmiris'&&$('cmiris_v')) $('cmiris_v').textContent=v;
  if(id==='cpeak'&&$('cpeak_v')) $('cpeak_v').textContent=String(parseInt(v,10)-50);
  if(id==='airis'&&$('airis_v')) $('airis_v').textContent=v;
  if(id==='cwclip'&&$('cwclip_v')) $('cwclip_v').textContent=String(95+parseInt(v,10));
  if(id==='cchr'){ /* chroma may be naked range in field */ }
  if(id==='sync'&&$('sync_v')){
    const n=Math.max(1,Math.min(255,parseInt(v,10)||1));
    $('sync_v').textContent=n.toString(16).toUpperCase().padStart(3,'0');
  }
  if((id==='crg'||id==='cbg'||id==='crp'||id==='cbp'||id==='ctped')&&$(id+'_v')) $(id+'_v').textContent=v;
  if(id.indexOf('cmx_')===0&&$(id+'_v')) $(id+'_v').textContent=String(parseInt(v,10)-31);
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
  if(el.id==='ciris'||el.id==='shut'||el.id==='awc'||el.id==='cgain_mode') updateCamDeps();
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
['cgain','cmiris','cpeak','airis','cwclip','crg','cbg','crp','cbp','ctped','cchr','rate','sync',
 'cmx_rg','cmx_rb','cmx_gr','cmx_gb','cmx_br','cmx_bg'].forEach(id=>{
  const el=$(id); if(el) updateSlideLabel(el);
});
updateExposureUi();
setOsdUi(false);
const HOLD_LIMIT_AXIS={tilt_up:1,tilt_down:2,pan_left:3,pan_right:4};
function setLimitArm(on){
  window._limitArm=!!on;
  const b=$('limit_arm');
  if(b){ b.classList.toggle('armed', !!on); b.setAttribute('aria-pressed', on?'true':'false'); }
}
function fireLimitAxis(ax){
  setLimitArm(false);
  if($('limax')) $('limax').value=String(ax);
  const caps=window._ptCaps||{};
  if(caps.limit_ctrl){
    if($('limact')) $('limact').value='1';
    return ptz('limit_ctrl');
  }
  return ptz('limit_toggle');
}
const limArmBtn=$('limit_arm');
if(limArmBtn){
  limArmBtn.addEventListener('click',()=>{
    setLimitArm(!window._limitArm);
  });
}
/* сброс Limit: клик в пустое (не Limit и не ось PT) */
document.addEventListener('pointerdown',e=>{
  if(!window._limitArm) return;
  const t=e.target;
  if(!t||!t.closest) return;
  if(t.closest('#limit_arm')) return;
  const hold=t.closest('[data-hold]');
  if(hold && HOLD_LIMIT_AXIS[hold.dataset.hold]) return;
  setLimitArm(false);
}, true);
function bindHold(el){
  const cmd=el.dataset.hold;
  let down=false;
  const go=ev=>{
    if(ev.pointerType==='mouse' && ev.button!==0) return;
    ev.preventDefault();
    const limAx=HOLD_LIMIT_AXIS[cmd];
    if(window._limitArm && limAx){
      try{ el.setPointerCapture(ev.pointerId); }catch(err){}
      fireLimitAxis(limAx);
      return;
    }
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
  function armed(){ return mode==='save'||mode==='delete'; }
  function syncInfo(){
    $('pr').value=String(selected);
    const m=mode==='recall'?'Recall':(mode==='save'?'Store…':'Delete…');
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
    padEl.classList.toggle('await-slot', armed());
    banksEl.querySelectorAll('button').forEach((b,i)=>b.classList.toggle('active', i===bank));
    document.querySelectorAll('#prmodes [data-pr-mode]').forEach(b=>{
      const m=b.dataset.prMode;
      b.classList.toggle('armed', mode===m);
      b.classList.toggle('idle', mode!==m);
    });
    syncInfo();
  }
  function setBank(b){ bank=Math.max(0,Math.min(BANKS-1,b)); paintPad(); }
  function setMode(m){
    if(m!=='save'&&m!=='delete'){ mode='recall'; paintPad(); return; }
    mode=(mode===m)?'recall':m; /* повтор = сброс в Recall */
    paintPad();
  }
  function clearArm(){ if(armed()){ mode='recall'; paintPad(); } }
  async function askDelete(msg){
    if(window.IptvConfirm && typeof IptvConfirm.ask==='function')
      return IptvConfirm.ask(msg, 'Удалить');
    return window.confirm(msg);
  }
  async function runSlot(n){
    if(n<1||n>MAX_PRESET) return;
    selected=n; $('pr').value=String(n); paintPad();
    if(mode==='recall') return ptz('preset_recall');
    if(mode==='save'){
      const d=await ptz('preset_save');
      if(d&&d.ok) occ[n-1]=true;
      mode='recall';
      paintPad();
      return d;
    }
    if(mode==='delete'){
      if(!(await askDelete('Удалить пресет P'+String(n).padStart(2,'0')+'?'))){
        mode='recall'; paintPad(); return;
      }
      const d=await ptz('preset_delete');
      if(d&&d.ok) occ[n-1]=false;
      mode='recall';
      paintPad();
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
  /* сброс залипания: пустое место (не Store/Delete и не номер пресета) */
  document.addEventListener('pointerdown',e=>{
    if(!armed()) return;
    const t=e.target;
    if(!t||!t.closest) return;
    if(t.closest('#prquick')) return;
    if(t.closest('#prmodes [data-pr-mode]')) return;
    clearArm();
  }, true);
  paintPad();
  window.PresetUI={applyMap, paintPad, clearArm};
})();
function applyInvertUi(d){
  if(!d) return;
  if(typeof d.inv_pan==='boolean') $('inv_pan').checked=d.inv_pan;
  if(typeof d.inv_tilt==='boolean') $('inv_tilt').checked=d.inv_tilt;
  if(typeof d.inv_zoom==='boolean') $('inv_zoom').checked=d.inv_zoom;
  if(typeof d.inv_focus==='boolean') $('inv_focus').checked=d.inv_focus;
}
function applyCaps(d){
  window._ptCaps=Object.assign({}, d&&d.pt_caps);
  const caps=Object.assign({}, window._ptCaps, d&&d.cam_caps);
  document.querySelectorAll('[data-cap]').forEach(el=>{
    const key=el.getAttribute('data-cap');
    if(caps[key]===undefined){ el.classList.remove('cap-off'); return; }
    el.classList.toggle('cap-off', !caps[key]);
  });
  /* Limit arm: PH #L или HE #LC */
  const lim=$('limit_arm');
  if(lim && caps.limit_toggle===false && caps.limit_ctrl){
    lim.classList.remove('cap-off');
  }
  updateCamDeps();
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
  applyOsdLabels();
  if(d.cam_id && d.cam_id!==window._rangesCamId) loadCamRanges();
}
/** Заполнить select/range из /api/cam?cmd=ranges (ccam camParamCatalog). */
function fillCamRanges(ranges){
  if(!ranges) return;
  Object.keys(ranges).forEach(key=>{
    const spec=ranges[key];
    const el=document.querySelector('[data-range="'+key+'"]');
    if(!el||!spec) return;
    if(spec.kind==='choices' && el.tagName==='SELECT'){
      const cur=el.value;
      el.innerHTML='';
      (spec.items||[]).forEach(it=>{
        const o=document.createElement('option');
        o.value=it.v; o.textContent=it.l;
        el.appendChild(o);
      });
      if(key==='shutter'){
        const syn=(spec.items||[]).find(it=>String(it.l||'').toLowerCase()==='synchro');
        window._shutSynchroV=syn?String(syn.v):'';
      }
      if(cur!==''&&cur!=null&&[...el.options].some(o=>o.value===cur)) el.value=cur;
    }else if(spec.kind==='hex_range' && el.type==='range'){
      el.min=String(spec.min); el.max=String(spec.max);
      const v=parseInt(el.value,10);
      if(isNaN(v)||v<spec.min||v>spec.max) el.value=String(spec.min);
      el.title='OMS '+Number(spec.min).toString(16).toUpperCase().padStart(spec.digits||3,'0')
        +'…'+Number(spec.max).toString(16).toUpperCase().padStart(spec.digits||3,'0');
      updateSlideLabel(el);
    }
  });
}
async function loadCamRanges(){
  try{
    const r=await fetch('/api/cam?cmd=ranges',{cache:'no-store'});
    const d=await r.json();
    if(!d||!d.ok||!d.ranges) return;
    window._rangesCamId=d.cam_id||'';
    fillCamRanges(d.ranges);
    loadOsdRanges();
  }catch(e){}
}
function setSel(id,v){ const el=$(id); if(el&&v!==undefined&&v!==null&&typeof v!=='string') el.value=String(v); else if(el&&typeof v==='string'&&v!=='timeout'&&v!=='io'&&v!=='nack'&&v!=='param') el.value=v; }
async function loadCamParams(quiet){
  if(window._camParamsBusy) return;
  window._camParamsBusy=true;
  const acc={ok:true,status:'ok'};
  try{
    const r=await fetch('/api/cam?cmd=params',{cache:'no-store'});
    if(!r.ok) throw new Error('http');
    /* NDJSON stream: поле за полем; fallback — один JSON (старая прошивка) */
    const ct=(r.headers.get('content-type')||'').toLowerCase();
    if(!r.body||!ct.includes('ndjson')){
      const d=await r.json();
      if(quiet){ if(d&&d.ok) applyCamParams(d); }
      else showBar('cam/params',d);
      return;
    }
    const reader=r.body.getReader();
    const dec=new TextDecoder();
    let buf='';
    for(;;){
      const {done,value}=await reader.read();
      if(done) break;
      buf+=dec.decode(value,{stream:true});
      for(;;){
        const i=buf.indexOf('\n');
        if(i<0) break;
        const line=buf.slice(0,i).trim();
        buf=buf.slice(i+1);
        if(!line) continue;
        let obj;
        try{ obj=JSON.parse(line); }catch(e){ continue; }
        Object.assign(acc,obj);
        if(obj.done) continue;
        applyCamParams(acc);
      }
    }
    if(buf.trim()){
      try{ Object.assign(acc, JSON.parse(buf.trim())); }catch(e){}
    }
    applyCamParams(acc);
    if(!quiet) showBar('cam/params',acc);
  }catch(e){
    if(!quiet) showBar('cam/params',{ok:false,status:'net'});
  }finally{
    window._camParamsBusy=false;
  }
}
function applyCamParams(d){
  if(!d||!d.ok) return;
  setSel('awc',d.awc); setSel('scene',d.scene); setSel('shut',d.shutter);
  setSel('cdet',d.detail); setSel('cbar',d.colorbar);
  setSel('caf',d.af); setSel('cfilt',d.filter); setSel('cneg',d.negpos);
  setSel('cff',d.fieldframe); setSel('ciris',d.iris_am);
  if(d.menu!==undefined&&d.menu!==null&&d.menu!=='timeout'&&d.menu!=='io'&&d.menu!=='nack'){
    setOsdUi(String(d.menu)==='1'||d.menu===1);
  }
  /* gain_mode из QGU (80h→agc). Числовой d.gain от PT exposure — не режим камеры. */
  const gmode=(d.gain_mode!==undefined&&d.gain_mode!=='')?d.gain_mode:d.gain;
  if(gmode==='agc'){
    window._gainAgc=true;
    setSel('cgain_mode','agc');
  }else if(gmode==='manual'||typeof d.gain_db==='number'){
    window._gainAgc=false;
    setSel('cgain_mode','manual');
    if(typeof d.gain_db==='number'&&$('cgain')){
      $('cgain').value=d.gain_db; if($('cgain_v')) $('cgain_v').textContent=d.gain_db;
    }
  }
  if(typeof d.iris==='number' && $('cmiris')){
    const pct=Math.max(0,Math.min(100,Math.round(d.iris*100/65535)));
    $('cmiris').value=pct; if($('cmiris_v')) $('cmiris_v').textContent=pct;
  }
  if(d.gamma!==undefined) setSel('cgam',d.gamma);
  if(d.agc_max!==undefined) setSel('cagc',d.agc_max);
  if(d.dig_gain!==undefined) setSel('cdig',d.dig_gain);
  if(d.light_area!==undefined) setSel('carea',d.light_area);
  if(d.knee!==undefined) setSel('cknee',d.knee);
  if(d.cdnr!==undefined) setSel('ccdnr',d.cdnr);
  if(d.atw!==undefined) setSel('catw',d.atw);
  if(d.dnr3d!==undefined) setSel('c3dnr',d.dnr3d);
  if(typeof d.light_peak==='number' && $('cpeak')){
    $('cpeak').value=d.light_peak; if($('cpeak_v')) $('cpeak_v').textContent=d.light_peak-50;
  }
  if(d.airis!==undefined && $('airis')){
    const a=(typeof d.airis==='number')?d.airis:parseInt(String(d.airis),16);
    if(!isNaN(a)){ $('airis').value=a; if($('airis_v')) $('airis_v').textContent=a; }
  }
  if(typeof d.wclip==='number' && $('cwclip')){
    $('cwclip').value=d.wclip; if($('cwclip_v')) $('cwclip_v').textContent=95+d.wclip;
  }
  if(typeof d.chroma==='number' && $('cchr')) $('cchr').value=d.chroma;
  // ORG/OGR: 0x1E=0 → UI=(code-30)*5 (−150…+150)
  const paintToSlider=(code,id,vid)=>{
    if(typeof code!=='number'||!$(id)) return;
    const s=paintFromWire(code);
    $(id).value=s; if($(vid)) $(vid).textContent=s;
  };
  paintToSlider(d.r_gain,'crg','crg_v');
  paintToSlider(d.b_gain,'cbg','cbg_v');
  paintToSlider(d.r_ped,'crp','crp_v');
  paintToSlider(d.b_ped,'cbp','cbp_v');
  paintToSlider(d.t_ped,'ctped','ctped_v');
  const mx=(code,id)=>{
    if(typeof code!=='number'||!$(id)) return;
    $(id).value=code; if($(id+'_v')) $(id+'_v').textContent=code-31;
  };
  mx(d.mx_rg,'cmx_rg'); mx(d.mx_rb,'cmx_rb'); mx(d.mx_gr,'cmx_gr');
  mx(d.mx_gb,'cmx_gb'); mx(d.mx_br,'cmx_br'); mx(d.mx_bg,'cmx_bg');
  updateCamDeps();
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
    if($('link')) link.textContent=d.link?'up':'down';
    const ptOk=!!d.pt_alive;
    const pwr=d.pt_power_s||(d.pt_power===undefined?'?':String(d.pt_power));
    const camOk=!!d.cam_alive;
    const camTx=!!d.cam_tx;
    if($('badge-eth')){
      $('badge-eth').textContent='ETH '+(d.link?'up':'down');
      $('badge-eth').className='badge '+(d.link?'on':'off');
    }
    if($('badge-pt')){
      const headOk=!!d.pt_powered;
      $('badge-pt').textContent=ptOk?(headOk?('PT '+pwr):('PT '+pwr+' · off')):'PT dead';
      $('badge-pt').className='badge '+(ptOk?(headOk?'on':'warn'):'off');
    }
    if($('badge-cam')){
      let camTxt='Cam —';
      let camCls='badge muted';
      if(!ptOk){camTxt='Cam ?';camCls='badge muted';}
      else if(!camTx){camTxt='Cam off path';camCls='badge muted';}
      else if(camOk){camTxt='Cam '+(d.cam_qid||'ok');camCls='badge on';}
      else{camTxt='Cam missing';camCls='badge '+(d.cam_fail>0?'off':'warn');}
      $('badge-cam').textContent=camTxt;
      $('badge-cam').className=camCls;
    }
    if($('linkmini')){
      linkmini.textContent=d.ip||'';
      linkmini.title='ETH '+(d.link?'up':'down')
        +' · PT '+(ptOk?('alive p'+d.pt_power+' '+pwr):'DEAD')
        +' · Cam '+(camOk?(d.cam_qid||'ok'):(camTx?'missing':'n/a'));
    }
    if($('ip')){ ip.textContent=d.ip;mask.textContent=d.mask;gw.textContent=d.gw;mac.textContent=d.mac; }
    if($('ptm')){ ptm.textContent=d.model;camm.textContent=d.cam||'—'; }
    if($('ptlink')){
      const err=(d.pt_error!=null&&d.pt_error!==255)?(' · err 0x'+Number(d.pt_error).toString(16)):'';
      const optics=d.pt_optics===true?' · GZ ok':(d.pt_optics===false?(' · GZ —'+(d.pt_optics_fail?('×'+d.pt_optics_fail):'')):'');
      $('ptlink').textContent=ptOk
        ?('alive · p'+d.pt_power+' ('+pwr+')'+optics+(d.pt_powered?'':' · head off')+err)
        :('DEAD · fails '+(d.pt_fail||0));
      $('ptlink').style.color=ptOk?(d.pt_powered?(d.pt_error&&d.pt_error!==0&&d.pt_error!==255?'var(--warn)':'var(--ok)'):'var(--warn)'):'var(--bad)';
    }
    if($('camlink')){
      if(!ptOk) $('camlink').textContent='— (PT dead)';
      else if(!camTx) $('camlink').textContent='CamTX off (p'+d.pt_power+')';
      else if(camOk) $('camlink').textContent='alive · QID '+(d.cam_qid||'?');
      else $('camlink').textContent='no reply · fails '+(d.cam_fail||0);
      $('camlink').style.color=camOk?'var(--ok)':(camTx?'var(--bad)':'');
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
if($('cam_read_params')) $('cam_read_params').onclick=()=>loadCamParams(false);
loadCamRanges();
/* params — только при входе на Camera (или Read params); на старте тормозит UI */
tick();setInterval(tick,4000);

