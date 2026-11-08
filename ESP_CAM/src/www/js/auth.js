/** Вход по cookie-сессии + UI токена Bearer.
 *  Пароль на устройство не уходит: SHA-256(salt||password), затем
 *  proof = SHA-256(nonce||hash). WebCrypto на http:// недоступен. */
function utf8Bytes(s){ return new TextEncoder().encode(s); }
function catBytes(a,b){
  const o=new Uint8Array(a.length+b.length);
  o.set(a,0); o.set(b,a.length);
  return o;
}
function hexToBytes(hex){
  if(!hex||hex.length%2) return new Uint8Array(0);
  const o=new Uint8Array(hex.length/2);
  for(let i=0;i<o.length;i++){
    const v=parseInt(hex.substr(i*2,2),16);
    if(isNaN(v)) return new Uint8Array(0);
    o[i]=v;
  }
  return o;
}
function bytesToHex(b){
  let s='';
  for(let i=0;i<b.length;i++) s+=b[i].toString(16).padStart(2,'0');
  return s;
}
function sha256(bytes){
  const K=new Uint32Array([
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
  ]);
  const rotr=(x,n)=>((x>>>n)|(x<<(32-n)))>>>0;
  let h0=0x6a09e667,h1=0xbb67ae85,h2=0x3c6ef372,h3=0xa54ff53a;
  let h4=0x510e527f,h5=0x9b05688c,h6=0x1f83d9ab,h7=0x5be0cd19;
  const bitLen=bytes.length*8;
  const blocks=Math.ceil((bytes.length+9)/64)*64;
  const buf=new Uint8Array(blocks);
  buf.set(bytes);
  buf[bytes.length]=0x80;
  const view=new DataView(buf.buffer);
  view.setUint32(blocks-4, bitLen>>>0, false);
  const w=new Uint32Array(64);
  for(let i=0;i<blocks;i+=64){
    for(let t=0;t<16;t++) w[t]=view.getUint32(i+t*4,false);
    for(let t=16;t<64;t++){
      const s0=rotr(w[t-15],7)^rotr(w[t-15],18)^(w[t-15]>>>3);
      const s1=rotr(w[t-2],17)^rotr(w[t-2],19)^(w[t-2]>>>10);
      w[t]=(w[t-16]+s0+w[t-7]+s1)>>>0;
    }
    let a=h0,b=h1,c=h2,d=h3,e=h4,f=h5,g=h6,h=h7;
    for(let t=0;t<64;t++){
      const S1=rotr(e,6)^rotr(e,11)^rotr(e,25);
      const ch=(e&f)^((~e)>>>0&g);
      const t1=(h+S1+ch+K[t]+w[t])>>>0;
      const S0=rotr(a,2)^rotr(a,13)^rotr(a,22);
      const maj=(a&b)^(a&c)^(b&c);
      const t2=(S0+maj)>>>0;
      h=g; g=f; f=e; e=(d+t1)>>>0; d=c; c=b; b=a; a=(t1+t2)>>>0;
    }
    h0=(h0+a)>>>0; h1=(h1+b)>>>0; h2=(h2+c)>>>0; h3=(h3+d)>>>0;
    h4=(h4+e)>>>0; h5=(h5+f)>>>0; h6=(h6+g)>>>0; h7=(h7+h)>>>0;
  }
  const out=new Uint8Array(32);
  const ov=new DataView(out.buffer);
  [h0,h1,h2,h3,h4,h5,h6,h7].forEach((v,i)=>ov.setUint32(i*4,v,false));
  return out;
}
window.AUTH=window.AUTH||{};
const AUTH=window.AUTH;
Object.assign(AUTH,{
  user:'',
  _epoch:0,
  mark(){ return this._epoch|0; },
  stale(mark){ return (this._epoch|0)!==mark; },
  bumpEpoch(){ this._epoch=(this._epoch+1)>>>0; },
  async api(cmd, extra){
    const mark=this.mark();
    const q=new URLSearchParams(Object.assign({cmd}, extra||{}));
    const r=await apiFetch('/api/auth?'+q);
    let d=null;
    try{ d=await r.json(); }catch(e){ d={ok:false,status:'net'}; }
    d._http=r.status;
    d._stale=this.stale(mark);
    return d;
  },
  /** Не открывать gate сразу после логина (in-flight 401 / гонка cookie). */
  inGateGrace(){ return Date.now() < (this._gateGraceUntil||0); },
  showGate(on, err){
    /* Пока форма входа на экране, /api/status больше не крутится.
       Иначе его 401 приходит уже после нового логина и снова выкидывает. */
    if(on){
      if(this.inGateGrace()) return;
      this.bumpEpoch();
      window._authDeferredGate=false;
      if(window._authTick){ clearInterval(window._authTick); window._authTick=null; }
    }
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
  setWho(user, ttl, left){
    this.user=user||'';
    const ttlS=(ttl!=null)?ttl:this.sessTtl;
    let t=user?('login: '+user+' · session'):'login: —';
    const show=(left!=null)?left:ttlS;
    if(user&&show!=null) t+=' · '+show+'с';
    if($('auth_who')) $('auth_who').textContent=t;
    if($('auth_session_hint')) $('auth_session_hint').textContent=t;
  },
  noteLeft(d){
    if(!d) return;
    this.applyTtlUi(d);
    if(typeof d.sess_left_s==='number'){
      this.sessLeft=d.sess_left_s;
      this.setWho(this.user, this.sessTtl, d.sess_left_s);
    }
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
    this._gateGraceUntil=Date.now()+5000;
    this.setWho(this.user||'admin', this.sessTtl);
    await this.refreshTokenUi();
    /* Cookie должна прилипнуть до фона — иначе tick/Latest ловят 401 и снова gate. */
    const me=await this.api('me');
    if(!me||!me.ok){
      this._gateGraceUntil=0;
      this.showGate(true, 'Сессия не установилась');
      return;
    }
    this.applyTtlUi(me);
    this.noteLeft(me);
    this.setWho(this.user||'admin', this.sessTtl, me.sess_left_s);
    if(typeof loadCamRanges==='function') loadCamRanges();
    if(typeof tick==='function'){
      if(window._authTick){ clearInterval(window._authTick); window._authTick=null; }
      tick();
      window._authTick=setInterval(tick,4000);
    }
    /* Control открыт по умолчанию — подтянуть Latest (#S) */
    if(window.PresetUI && typeof window.PresetUI.refreshLatest==='function')
      window.PresetUI.refreshLatest();
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
  async openProof(password){
    const ch=await this.api('challenge');
    if(!ch||!ch.ok||!ch.salt||!ch.nonce||!ch.newsalt) return null;
    const salt=hexToBytes(ch.salt);
    const nonce=hexToBytes(ch.nonce);
    const newsalt=hexToBytes(ch.newsalt);
    if(salt.length!==16||nonce.length!==16||newsalt.length!==16) return null;
    const ver=sha256(catBytes(salt, utf8Bytes(password)));
    const proof=sha256(catBytes(nonce, ver));
    return {proof:bytesToHex(proof), newsalt};
  },
  async login(user, pass){
    const opened=await this.openProof(pass);
    if(!opened){
      this._gateGraceUntil=0;
      this.showGate(true, 'Нет ответа');
      return false;
    }
    const d=await this.api('login',{user, proof:opened.proof});
    if(d&&d.ok){
      this.bumpEpoch();
      window._authDeferredGate=false;
      this._gateGraceUntil=Date.now()+5000;
      this.user=d.user||user;
      this.applyTtlUi(d);
      if($('login_pass')) $('login_pass').value='';
      await this.enterApp();
      return true;
    }
    this._gateGraceUntil=0;
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
    if(!cur||!neu||neu.length>64){ showBar('auth/set_pass',{ok:false,status:'param'}); return; }
    const opened=await AUTH.openProof(cur);
    if(!opened){ showBar('auth/set_pass',{ok:false,status:'net'}); return; }
    const nh=sha256(catBytes(opened.newsalt, utf8Bytes(neu)));
    const d=await AUTH.api('set_pass',{proof:opened.proof, hash:bytesToHex(nh)});
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
  /* Keepalive: жест ставит флаг, ближайший tick шлёт /api/status с X-Esp-Active
     и снимает флаг после ответа. Сессия живёт TTL после жеста (+ до одного tick).
     Gate — только из tick. */
  AUTH._activeSeq=0;
  AUTH._activeAcked=0;
  AUTH.markActive=()=>{ AUTH._activeSeq++; };
  AUTH.isActive=()=> window._ptHold>0 || AUTH._activeSeq!==AUTH._activeAcked;
  /* seq — значение на момент отправки; жест во время запроса не теряется. */
  AUTH.ackActive=(seq)=>{ AUTH._activeAcked=seq; };
  const bump=()=>{
    if(document.body.classList.contains('login')) return;
    AUTH.markActive();
  };
  ['pointerdown','keydown','touchstart','click','change','input'].forEach(ev=>{
    document.addEventListener(ev, bump, {capture:true, passive:true});
  });
})();
