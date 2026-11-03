/** Вход по cookie-сессии + UI токена Bearer. */
window.AUTH=window.AUTH||{};
const AUTH=window.AUTH;
Object.assign(AUTH,{
  user:'',
  async api(cmd, extra){
    const q=new URLSearchParams(Object.assign({cmd}, extra||{}));
    const r=await fetch('/api/auth?'+q,{cache:'no-store', credentials:'same-origin'});
    let d=null;
    try{ d=await r.json(); }catch(e){ d={ok:false,status:'net'}; }
    d._http=r.status;
    return d;
  },
  showGate(on, err){
    const gate=$('login_gate'), wrap=$('app_wrap');
    if(gate) gate.hidden=!on;
    if(wrap) wrap.hidden=!!on;
    document.body.classList.toggle('login', !!on);
    const er=$('login_err');
    if(er){
      if(err){ er.classList.remove('hidden'); er.textContent=err; }
      else { er.classList.add('hidden'); er.textContent=''; }
    }
  },
  setWho(user, ttl){
    this.user=user||'';
    const ttlS=(ttl!=null)?ttl:this.sessTtl;
    let t=user?('login: '+user+' · session'):'login: —';
    if(user&&ttlS) t+=' · '+ttlS+'с';
    if($('auth_who')) $('auth_who').textContent=t;
    if($('auth_session_hint')) $('auth_session_hint').textContent=t;
  },
  applyTtlUi(d){
    if(!d) return;
    if(typeof d.sess_ttl_s==='number'){
      this.sessTtl=d.sess_ttl_s;
      if($('auth_sess_ttl')&&document.activeElement!==$('auth_sess_ttl'))
        $('auth_sess_ttl').value=String(d.sess_ttl_s);
    }
  },
  async refreshTokenUi(){
    const d=await this.api('token');
    if(!d||!d.ok) return;
    this.applyTtlUi(d);
    if($('auth_token')) $('auth_token').value=d.token||'';
    const host=location.host||'device';
    if($('auth_curl')){
      $('auth_curl').textContent=
        'curl -s -H "Authorization: Bearer '+(d.token||'')+'" http://'+host+'/api/status';
    }
  },
  async enterApp(){
    this.showGate(false);
    this.setWho(this.user||'admin', this.sessTtl);
    await this.refreshTokenUi();
    this.setWho(this.user||'admin', this.sessTtl);
    if(typeof loadCamRanges==='function') loadCamRanges();
    if(typeof tick==='function'){ tick(); if(!window._authTick){ window._authTick=setInterval(tick,4000); } }
  },
  async ensure(){
    const d=await this.api('me');
    if(d&&d.ok){
      this.user=d.user||'admin';
      this.applyTtlUi(d);
      await this.enterApp();
      return true;
    }
    this.applyTtlUi(d);
    this.showGate(true);
    return false;
  },
  async login(user, pass){
    const d=await this.api('login',{user, pass});
    if(d&&d.ok){
      this.user=d.user||user;
      this.applyTtlUi(d);
      await this.enterApp();
      return true;
    }
    this.showGate(true, 'Неверный логин или пароль');
    return false;
  },
  async logout(){
    await this.api('logout');
    this.user='';
    this.setWho('');
    if(window._authTick){ clearInterval(window._authTick); window._authTick=null; }
    this.showGate(true);
    if($('login_pass')) $('login_pass').value='';
  }
});
AUTH.sessTtl=300;

(function initAuthUi(){
  const form=$('login_form');
  if(form){
    form.addEventListener('submit',async e=>{
      e.preventDefault();
      await AUTH.login(val('login_user','admin'), val('login_pass',''));
    });
  }
  if($('btn_logout')) $('btn_logout').onclick=()=>AUTH.logout();
  if($('auth_pass_save')) $('auth_pass_save').onclick=async()=>{
    const cur=val('auth_pass_cur','');
    const neu=val('auth_pass_new','');
    if(!neu){ showBar('auth/set_pass',{ok:false,status:'param'}); return; }
    const d=await AUTH.api('set_pass',{pass:cur, data:neu});
    showBar('auth/set_pass', d);
    if(d&&d.ok){
      if($('auth_pass_cur')) $('auth_pass_cur').value='';
      if($('auth_pass_new')) $('auth_pass_new').value='';
    }
  };
  if($('auth_token_copy')) $('auth_token_copy').onclick=async()=>{
    const t=$('auth_token')&&$('auth_token').value;
    if(!t) return;
    try{
      if(navigator.clipboard&&navigator.clipboard.writeText) await navigator.clipboard.writeText(t);
      else {
        $('auth_token').select();
        document.execCommand('copy');
      }
      showBar('auth/token',{ok:true,status:'copied'});
    }catch(e){
      showBar('auth/token',{ok:false,status:'copy'});
    }
  };
  if($('auth_token_reissue')) $('auth_token_reissue').onclick=async()=>{
    const ok=window.IptvConfirm
      ? await IptvConfirm.ask('Перевыпустить API token? Старый перестанет работать.','Перевыпустить')
      : window.confirm('Перевыпустить API token?');
    if(!ok) return;
    const d=await AUTH.api('reissue_token');
    showBar('auth/reissue_token', d);
    if(d&&d.ok&&$('auth_token')){
      $('auth_token').value=d.token||'';
      await AUTH.refreshTokenUi();
    }
  };
  if($('auth_sess_ttl_save')) $('auth_sess_ttl_save').onclick=async()=>{
    let sec=parseInt(val('auth_sess_ttl','300'),10);
    if(isNaN(sec)) sec=300;
    if(sec<30) sec=30;
    if(sec>300) sec=300;
    if($('auth_sess_ttl')) $('auth_sess_ttl').value=String(sec);
    const d=await AUTH.api('set_sess_ttl',{v:String(sec)});
    showBar('auth/set_sess_ttl', d);
    if(d&&d.ok){
      AUTH.applyTtlUi(d);
      AUTH.setWho(AUTH.user, AUTH.sessTtl);
    }
  };
  AUTH.ensure();
  /* Idle: фоновый /api/status не трогает TTL; клики/клавиши → touch (throttle). */
  let _touchAt=0;
  const onTouchResult=(d)=>{
    if(!d) return;
    if(d._http===401 || (d.ok===false && d.status==='auth')){
      if(window._ptHold>0){ window._authDeferredGate=true; return; }
      AUTH.showGate(true,'Сессия истекла');
      return;
    }
    if(d.ok) AUTH.applyTtlUi(d);
  };
  AUTH.touchNow=()=>{
    if(document.body.classList.contains('login')) return;
    _touchAt=Date.now();
    AUTH.api('touch').then(onTouchResult).catch(()=>{});
  };
  const bump=()=>{
    if(document.body.classList.contains('login')) return;
    const now=Date.now();
    if(now-_touchAt<(window._ptHold>0?5000:12000)) return;
    AUTH.touchNow();
  };
  ['pointerdown','keydown','touchstart','click','change','input'].forEach(ev=>{
    document.addEventListener(ev, bump, {capture:true, passive:true});
  });
})();
