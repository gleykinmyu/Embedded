/** Раскладка OSD: studio (E650) ≠ HE — одни и те же Dxx, разная роль. */
function osdCamId(){
  return (window._rangesCamId||($('cammodel')&&$('cammodel').value)||'').toLowerCase();
}
function osdIsStudio(id){
  id=id||osdCamId();
  return id==='e650'||id==='e600'||id==='e800'||id.indexOf('e6')===0;
}
function osdSetBtn(el, cmd, label, title, show){
  if(!el) return;
  el.hidden=show===false;
  if(cmd) el.dataset.cmd=cmd;
  if(label!=null) el.textContent=label;
  if(title!=null) el.title=title;
}
function applyOsdLabels(){
  const nav=$('osd_nav'); if(!nav) return;
  const id=osdCamId();
  const studio=osdIsStudio(id);
  nav.dataset.osdLayout=studio?'studio':'he';

  const up=$('osd_up'), down=$('osd_down'), mid=$('osd_mid');
  const yes=$('osd_yes'), no=$('osd_no'), menu=$('osd_menu');
  const left=$('osd_left'), right=$('osd_right'), hint=$('osd_hint');

  if(studio){
    /* E650: Item↑ = Menu(DPG); Item↓ = ITEM(DIT); NO / YES(Enter) = значение */
    osdSetBtn(up, 'menu_sw', 'Item ↑', 'DPG · Menu (Item Up)', true);
    osdSetBtn(down, 'item_sw', 'Item ↓', 'DIT · следующая строка (Item Down)', true);
    osdSetBtn(no, 'no_sw', 'NO −', 'DDW · − значение', true);
    osdSetBtn(yes, 'yes_sw', 'YES (Enter)', 'DUP · + значение / Enter', true);
    osdSetBtn(menu, null, null, null, false);
    osdSetBtn(mid, null, null, null, false);
    if(hint) hint.textContent='E650: Item↑=Menu · Item↓=строки · NO / YES(Enter)=значение';
  }else{
    /* HE/UE: ↑↓ строки; Enter=DIT; Menu=DPG; ←→ только HE130 (cap) */
    osdSetBtn(up, 'yes_sw', '▲ Up', 'DUP · вверх по меню', true);
    osdSetBtn(down, 'no_sw', '▼ Down', 'DDW · вниз по меню', true);
    osdSetBtn(mid, 'item_sw', 'Enter', 'DIT · вход / выбор', true);
    osdSetBtn(menu, 'menu_sw', 'Menu', 'DPG · выход', true);
    osdSetBtn(yes, null, null, null, false);
    osdSetBtn(no, null, null, null, false);
    if(left){ left.textContent='←'; left.title='DLT · влево'; }
    if(right){ right.textContent='→'; right.title='DRT · вправо'; }
    if(hint){
      hint.textContent=(id==='he130')
        ?'HE130: ↑↓ строки · ←→ · Enter / Menu'
        :'PTZ: ↑↓ строки · Enter / Menu';
    }
  }
}
function setOsdUi(on){
  window._osdOn=!!on;
  const nav=$('osd_nav');
  if(nav) nav.hidden=!on;
  if($('osd_on')) $('osd_on').classList.toggle('is-on', !!on);
  if($('osd_off')) $('osd_off').classList.toggle('is-on', !on);
  applyOsdLabels();
}
function osdWire(row){
  const el=row.querySelector('[data-osd-val]');
  if(!el) return null;
  if(el.tagName==='SELECT') return el.value;
  const n=parseInt(el.value,10);
  if(isNaN(n)) return null;
  return n.toString(16).toUpperCase().padStart(2,'0');
}
function osdShowLab(row){
  const p=row._param;
  const lab=row.querySelector('.osd-lab');
  const el=row.querySelector('[data-osd-val]');
  if(!p||!lab||!el) return;
  if(p.kind==='choices'){
    const it=(p.items||[]).find(x=>x.v===el.value);
    lab.textContent=it?it.l:el.value;
    return;
  }
  const code=parseInt(el.value,10)||0;
  if(p.lo||p.hi){
    lab.textContent=code.toString(16).toUpperCase().padStart(2,'0');
    return;
  }
  const n=code+(p.bias||0);
  lab.textContent=String(n)+(p.unit||'');
}
function osdPaintRow(d,row){
  const res=row.querySelector('.osd-res');
  if(d&&d.ok&&d.data){
    const m=String(d.data).match(/:([0-9A-Fa-f]{2})\s*$/);
    if(m){
      const code=m[1].toUpperCase();
      const el=row.querySelector('[data-osd-val]');
      row._apply=true;
      if(el&&el.tagName==='SELECT'){
        if([...el.options].some(o=>o.value===code)) el.value=code;
        else if(res) res.dataset.outside=code;
      }else if(el){
        const n=parseInt(code,16);
        if(n>=parseInt(el.min,10)&&n<=parseInt(el.max,10)) el.value=String(n);
        else if(res) res.dataset.outside=code;
      }
      row._apply=false;
      osdShowLab(row);
    }
  }
  const bits=[];
  if(d&&d.data) bits.push(d.data);
  if(res&&res.dataset.outside) bits.push('вне шкалы '+res.dataset.outside);
  if(d&&d.tx) bits.push('TX '+d.tx);
  if(d&&d.rx) bits.push('RX '+d.rx);
  else if(d&&d.rx==='') bits.push('RX —');
  if(res){
    delete res.dataset.outside;
    res.textContent=(d&&d.ok?'ok':'err')+' · '+(bits.join(' · ')||(d&&d.status)||'—');
    res.classList.toggle('osd-ok', !!(d&&d.ok));
    res.classList.toggle('osd-bad', !!(d&&!d.ok));
  }
}
async function osdExchange(row, kind){
  const p={v:row.dataset.key};
  if(kind==='set'){
    const data=osdWire(row);
    if(data==null||data==='') return {ok:false,status:'param'};
    p.data=data;
  }
  const d=await call('cam', kind==='set'?'osd_set':'osd_get', p);
  osdPaintRow(d,row);
  return d;
}
function osdBuild(items){
  const list=$('osd_list');
  if(!list) return;
  list.innerHTML='';
  let group='';
  items.forEach(p=>{
    if(p.group!==group){
      group=p.group;
      const h=document.createElement('div');
      h.className='osd-grp';
      h.textContent=group;
      list.appendChild(h);
    }
    const row=document.createElement('div');
    row.className='osd-row';
    row.dataset.key=p.key;
    row._param=p;
    let ctrl='';
    if(p.kind==='choices'){
      ctrl='<select data-osd-val>'+(p.items||[]).map(it=>'<option value="'+it.v+'">'+it.l+'</option>').join('')+'</select>';
    }else{
      const lo=p.lo?(' title="'+p.lo+' … '+(p.hi||'')+'"'):'';
      ctrl='<input type="range" data-osd-val min="'+p.min+'" max="'+p.max+'" value="'+p.min+'"'+lo+'>';
    }
    row.innerHTML=
      '<div class="osd-head"><span class="osd-name"><span class="osd-code">'+p.code+'</span>'+p.name+'</span>'+
      '<button type="button" data-osd="get">Read</button></div>'+
      '<div class="osd-ctl">'+ctrl+'<b class="osd-lab"></b></div>'+
      (p.note?'<p class="osd-note">'+p.note+'</p>':'')+
      '<p class="osd-res"></p>';
    list.appendChild(row);
    osdShowLab(row);
  });
}
async function loadOsdRanges(){
  const list=$('osd_list');
  if(!list) return;
  try{
    const r=await fetch('/api/cam?cmd=osd_ranges',{cache:'no-store'});
    const d=await r.json();
    if(!d||!d.ok||!Array.isArray(d.items)){
      list.textContent='каталог OSD не пришёл';
      return;
    }
    osdBuild(d.items);
  }catch(e){
    list.textContent='каталог OSD не пришёл';
  }
}
(function initOsd(){
  const list=$('osd_list');
  if(!list) return;
  window._osdStop=false;
  list.addEventListener('click',e=>{
    const b=e.target.closest('[data-osd]');
    if(!b) return;
    const row=b.closest('.osd-row');
    if(row) osdExchange(row, b.dataset.osd);
  });
  list.addEventListener('change',e=>{
    const el=e.target.closest('select[data-osd-val]');
    if(!el) return;
    const row=el.closest('.osd-row');
    if(!row||row._apply) return;
    osdShowLab(row);
    osdExchange(row,'set');
  });
  list.addEventListener('input',e=>{
    const el=e.target;
    if(!el.matches('input[type=range][data-osd-val]')) return;
    const row=el.closest('.osd-row');
    if(!row||row._apply) return;
    osdShowLab(row);
    clearTimeout(row._t);
    row._t=setTimeout(()=>osdExchange(row,'set'),120);
  });
  if($('osd_read_all')) $('osd_read_all').onclick=async()=>{
    if(window._osdBusy) return;
    window._osdBusy=true;
    window._osdStop=false;
    try{
      for(const row of list.querySelectorAll('.osd-row')){
        if(window._osdStop) break;
        row.scrollIntoView({block:'nearest'});
        await osdExchange(row,'get');
      }
    }finally{
      window._osdBusy=false;
    }
  };
  if($('osd_read_stop')) $('osd_read_stop').onclick=()=>{ window._osdStop=true; };
})();
