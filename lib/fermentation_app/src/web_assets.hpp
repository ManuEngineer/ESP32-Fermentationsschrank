#pragma once

namespace fermentation::web_assets {

// Compile-time assets only: no device identity, credentials, sessions, or
// build secrets are embedded. Browser language and bounded polling are UI
// concerns and do not create application state.
inline constexpr char kIndexHtml[] = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title data-t="title">Fermentation</title><link rel="stylesheet" href="/assets/app.css"></head>
<body><main><header><h1 data-t="title">Fermentation</h1><label><span data-t="language">Sprache</span>
<select id="language"><option value="de">DE</option><option value="en">EN</option><option value="es">ES</option></select></label></header>
<section id="loginPanel"><h2 data-t="login">Anmeldung</h2><form id="login"><label><span data-t="password">Passwort</span>
<input name="password" type="password" autocomplete="current-password" required></label><button data-t="signIn">Anmelden</button></form></section>
<section id="app" hidden><p id="state" aria-live="polite"></p><section><h2 data-t="overview">Übersicht</h2><pre id="overview"></pre></section>
<section><h2 data-t="network">Netzwerk</h2><select id="networkMode"></select><button id="applyMode" data-t="apply">Anwenden</button>
<button id="reconfigure" data-t="reconfigure">WLAN neu konfigurieren</button></section>
<section><h2 data-t="service">Service</h2><label><span data-t="servicePin">Service-PIN</span><input id="servicePin" inputmode="numeric" maxlength="4" type="password" autocomplete="off"></label>
<button id="unlockService" data-t="unlock">Freigeben</button><p id="serviceState"></p>
<label><input id="passwordEnabled" type="checkbox"><span data-t="passwordMode">Passwortschutz aktiv</span></label>
<input id="modePassword" type="password" autocomplete="new-password" data-t-placeholder="newPassword">
<button id="applyPasswordMode" data-t="apply">Anwenden</button></section><section><h2 data-t="alerts">Meldungen</h2><pre id="alerts"></pre></section>
<button id="logout" data-t="logout">Abmelden</button></section><p id="error" role="status" aria-live="polite"></p></main>
<script src="/assets/app.js" defer></script></body></html>)HTML";

inline constexpr char kProvisioningHtml[] =
    "<!doctype html><meta charset=utf-8><meta name=viewport content=width=device-width,initial-scale=1>"
    "<title>Fermentation</title><main><h1>Fermentation</h1>"
    "<p>Local authentication provisioning is required.</p></main>";

inline constexpr char kAppCss[] = R"CSS(body{font:1rem system-ui;margin:auto;max-width:58rem;padding:1rem}main{display:grid;gap:1rem}
header{display:flex;justify-content:space-between;gap:1rem;align-items:center}section{border:1px solid #888;border-radius:.5rem;padding:1rem}
form{display:grid;gap:.75rem;max-width:30rem}input,button,select{font:inherit;padding:.5rem;margin:.25rem}pre{white-space:pre-wrap;overflow-wrap:anywhere}
@media(min-width:45rem){#app{display:grid;grid-template-columns:1fr 1fr;gap:1rem}#app>section:first-child{grid-column:1/-1}})CSS";

inline constexpr char kAppJs[] = R"JS((()=>{
const t={de:{title:'Fermentation',language:'Sprache',login:'Anmeldung',password:'Passwort',signIn:'Anmelden',overview:'Übersicht',network:'Netzwerk',apply:'Anwenden',reconfigure:'WLAN neu konfigurieren',service:'Service',servicePin:'Service-PIN',unlock:'Freigeben',passwordMode:'Passwortschutz aktiv',newPassword:'Neues Passwort',alerts:'Meldungen',logout:'Abmelden',offline:'offline / veraltet'},en:{title:'Fermentation',language:'Language',login:'Login',password:'Password',signIn:'Sign in',overview:'Overview',network:'Network',apply:'Apply',reconfigure:'Reconfigure Wi-Fi',service:'Service',servicePin:'Service PIN',unlock:'Unlock',passwordMode:'Password protection active',newPassword:'New password',alerts:'Alerts',logout:'Sign out',offline:'offline / stale'},es:{title:'Fermentación',language:'Idioma',login:'Acceso',password:'Contraseña',signIn:'Entrar',overview:'Resumen',network:'Red',apply:'Aplicar',reconfigure:'Reconfigurar Wi-Fi',service:'Servicio',servicePin:'PIN de servicio',unlock:'Liberar',passwordMode:'Protección por contraseña activa',newPassword:'Nueva contraseña',alerts:'Alertas',logout:'Salir',offline:'sin conexión / obsoleto'}};
let lang=localStorage.getItem('fs-language')||'de',csrf='',nextSeq=1,expectedRevision=null,pollTimer=0,pollDelay=10000,pollFailures=0;
const $=id=>document.getElementById(id),showError=x=>{$('error').textContent=x||''};
function translate(){const d=t[lang]||t.en;document.documentElement.lang=lang;document.querySelectorAll('[data-t]').forEach(e=>e.textContent=d[e.dataset.t]||t.en[e.dataset.t]||e.dataset.t);document.querySelectorAll('[data-t-placeholder]').forEach(e=>e.placeholder=d[e.dataset.tPlaceholder]||t.en[e.dataset.tPlaceholder]||'');}
function sequence(data){if(Number.isSafeInteger(data.nextMutationSeq)&&data.nextMutationSeq>0)nextSeq=data.nextMutationSeq;}
async function request(path,options={}){const h=Object.assign({},options.headers||{});if(csrf)h['X-CSRF-Token']=csrf;if(options.mutation){h['X-UI-Mutation-Seq']=String(nextSeq);options.body=JSON.stringify(options.body||{});h['Content-Type']='application/json'}const r=await fetch(path,Object.assign({},options,{headers:h}));const d=await r.json().catch(()=>({}));if(d.nextMutationSeq)sequence(d);if(options.mutation&&r.ok)nextSeq++;if(!r.ok)throw new Error(d.message||d.code||String(r.status));return d;}
function render(data){sequence(data);expectedRevision=data.expectedUserConfigurationRevision||null;$('state').textContent=data.ready?'ready':'unavailable';$('overview').textContent=JSON.stringify({networkMode:data.networkMode,processState:data.processState,activeRunId:data.activeRunId,temperatures:data.temperatures},null,2);const s=$('networkMode');s.replaceChildren(...(data.selectableModes||['AP_ONLY','HOME_WIFI']).map(m=>{const o=document.createElement('option');o.value=m;o.textContent=m;return o;}));s.value=data.networkMode==='UNSELECTED'?'AP_ONLY':data.networkMode;if(typeof data.webPasswordEnabled==='boolean')$('passwordEnabled').checked=data.webPasswordEnabled;$('alerts').textContent='';}
function schedulePoll(){clearTimeout(pollTimer);pollTimer=setTimeout(poll,pollDelay);}
async function poll(){try{const data=await request('/internal/ui/snapshot');render(data);pollFailures=0;pollDelay=data.homeMode===1?2000:10000;showError('');}catch(e){pollFailures=Math.min(pollFailures+1,4);pollDelay=Math.min(30000,Math.max(10000,pollDelay*2));$('state').textContent=(t[lang]||t.en).offline;showError(e.message);}schedulePoll();}
$('language').value=lang;$('language').onchange=e=>{lang=e.target.value;localStorage.setItem('fs-language',lang);translate();};translate();
$('login').onsubmit=async e=>{e.preventDefault();try{const r=await fetch('/login',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({password:e.target.password.value})});const d=await r.json().catch(()=>({}));if(!r.ok)throw new Error(d.message||d.code||String(r.status));csrf=d.csrfToken||'';nextSeq=d.nextMutationSeq||1;$('loginPanel').hidden=true;$('app').hidden=false;await poll();}catch(x){showError(x.message);}};
$('applyMode').onclick=async()=>{try{const b={mode:$('networkMode').value};if(expectedRevision)b.expectedUserConfigurationRevision=expectedRevision;await request('/internal/ui/network/mode',{method:'POST',mutation:true,body:b});await poll();}catch(x){showError(x.message);await poll();}};
$('reconfigure').onclick=async()=>{try{await request('/internal/ui/network/reconfigure',{method:'POST',mutation:true});await poll();}catch(x){showError(x.message);}};
$('unlockService').onclick=async()=>{try{await request('/internal/service/unlock',{method:'POST',mutation:true,body:{servicePin:$('servicePin').value}});$('serviceState').textContent='active';$('servicePin').value='';}catch(x){showError(x.message);}};
$('applyPasswordMode').onclick=async()=>{try{const enabled=$('passwordEnabled').checked;const password=$('modePassword').value;const body={enabled,confirmed:true};body[enabled?'newPassword':'currentPassword']=password;await request('/internal/auth/password-mode',{method:'POST',mutation:true,body});$('modePassword').value='';await poll();}catch(x){showError(x.message);}};
$('logout').onclick=async()=>{try{await request('/logout',{method:'POST',mutation:true});}finally{location.reload();}};
})())JS";

}  // namespace fermentation::web_assets
