#pragma once

// Frozen generator from product ab0ae55, independent of the split
// implementation. Approved single-flight JS delta and spelling-only aliases
// applied explicitly for plan 8094b7f; the head/provisioning generator is
// frozen.
namespace shell_golden {
using fermentation::WebAuthenticationState;
constexpr std::size_t kMaximumWebShellBytes = 8192U;
const char* authenticationStateName(WebAuthenticationState state) noexcept {
    switch (state) {
        case WebAuthenticationState::PasswordProtected:
            return "password-protected";
        case WebAuthenticationState::PasswordDisabled:
            return "password-disabled";
        case WebAuthenticationState::Unprovisioned:
            return "unprovisioned";
        case WebAuthenticationState::RecoveryRequired:
            return "recovery-required";
        case WebAuthenticationState::Indeterminate:
            return "indeterminate";
    }
    return "indeterminate";
}
std::string provisioningShell() {
    return "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width,"
           "initial-scale=1\"><title>Fermentation</"
           "title><style>body{font-family:"
           "system-ui;max-width:48rem;margin:auto;padding:1rem}form{display:"
           "grid;"
           "gap:.6rem}input,button{font:inherit;padding:.45rem}#m{font-weight:"
           "600}"
           "</style></head><body><main><h1>Fermentation</h1><form id=\"f\">"
           "<label><input type=\"radio\" name=\"o\" value=\"p\" checked> "
           "<span id=\"a\"></span></label><label><input type=\"radio\" "
           "name=\"o\" value=\"d\"> <span id=\"b\"></span></label>"
           "<label id=\"lw\"><span id=\"c\"></span> <input id=\"w\" "
           "type=\"password\" autocomplete=\"new-password\"></label>"
           "<label><span id=\"e\"></span> <input id=\"n\" type=\"password\" "
           "inputmode=\"numeric\" autocomplete=\"off\"></label>"
           "<p id=\"x\" hidden></p><label id=\"lk\" hidden><input id=\"k\" "
           "type=\"checkbox\"> <span id=\"y\"></span></label>"
           "<button id=\"g\"></button><p id=\"m\"></p></form></main><script>"
           "const L={de:{a:'Mit Passwort schuetzen "
           "(empfohlen)',b:'Passwortschutz "
           "deaktivieren',c:'Passwort',e:'Service-PIN',x:'Warnung: Ohne "
           "Passwort "
           "kann jedes Geraet im lokalen Netz bedienen. Der Servicebereich "
           "bleibt "
           "per PIN geschuetzt.',y:'Verstanden, Passwortschutz deaktivieren',"
           "g:'Einrichten',ok:'Eingerichtet. Seite neu laden.',na:'Noch nicht "
           "freigegeben: Web-Setup zuerst am Geraet freigeben, dann erneut "
           "versuchen.',ic:'Passwort oder PIN abgelehnt.',ap:'Bereits "
           "eingerichtet.',rc:'Lokale Wiederherstellung "
           "noetig.',fl:'Einrichtung "
           "fehlgeschlagen.'},en:{a:'Protect with a password (recommended)',b:'"
           "Disable password protection',c:'Password',e:'Service "
           "PIN',x:'Warning:"
           " without a password every device in the local network can operate "
           "this device. The service area stays PIN protected.',y:'I "
           "understand, "
           "disable password protection',g:'Set up',ok:'Set up. Reload the "
           "page.',"
           "na:'Not released yet: release web setup on the device first, then "
           "retry.',ic:'Password or PIN rejected.',ap:'Already set "
           "up.',rc:'Local "
           "recovery needed.',fl:'Setup failed.'},es:{a:'Proteger con "
           "contrasena "
           "(recomendado)',b:'Desactivar la proteccion',c:'Contrasena',e:'PIN "
           "de "
           "servicio',x:'Aviso: sin contrasena cualquier dispositivo de la red "
           "local puede operar el equipo. El servicio sigue protegido por "
           "PIN.',"
           "y:'Entendido, desactivar la proteccion',g:'Configurar',ok:'Listo. "
           "Recargue la pagina.',na:'Aun no permitido: permita la "
           "configuracion "
           "web en el equipo y reintente.',ic:'Contrasena o PIN "
           "rechazados.',ap:'Ya"
           " configurado.',rc:'Requiere recuperacion local.',fl:'Fallo la "
           "configuracion.'}};const g=(navigator.language||'en').slice(0,2),"
           "T=L[g]||L.en,q=i=>document.getElementById(i);"
           "document.documentElement.lang=L[g]?g:'en';for(const i of "
           "['a','b','c','e','x','y','g'])q(i).textContent=T[i];"
           "const "
           "d=()=>document.querySelector('input[name=o]:checked').value==='d';"
           "const "
           "u=()=>{q('lw').hidden=d();q('x').hidden=q('lk').hidden=!d();};"
           "document.querySelectorAll('input[name=o]').forEach(i=>i.onchange=u)"
           ";"
           "q('f').onsubmit=async "
           "e=>{e.preventDefault();if(d()&&!q('k').checked)"
           "{q('m').textContent=T.y;return;}const b=d()?{mode:'disable',"
           "servicePin:q('n').value,confirmDisable:true}:{mode:'protect',"
           "password:"
           "q('w').value,servicePin:q('n').value};try{const r=await "
           "fetch('/api/v1/provision',{method:'POST',credentials:'same-origin',"
           "headers:{'Content-Type':'application/"
           "json'},body:JSON.stringify(b)});"
           "const j=await "
           "r.json().catch(()=>({}));q('m').textContent=r.ok?T.ok:"
           "j.error==='provisioning-not-allowed'?T.na:r.status===409?T.ap:r."
           "status===422?T.ic:"
           "j.error==='recovery-required'?T.rc:T.fl;if(r.ok){q('w').value=q('n'"
           ")"
           ".value='';}}catch(x){q('m').textContent=T.fl;}};u();</script></"
           "body>"
           "</html>";
}

std::string webShell(WebAuthenticationState state, const std::string& csrfToken,
                     bool authenticated, std::uint64_t mutationSequence) {
    if (state == WebAuthenticationState::Unprovisioned) {
        return provisioningShell();
    }
    // This is intentionally a small static shell. It has no framework,
    // generated bundle, history or chart. The only mutation controls are the
    // run actions below. All three API requests are repeated together so
    // a reconnect receives a full snapshot.
    std::string html;
    html.reserve(kMaximumWebShellBytes);
    html +=
        "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta"
        " name=\"viewport\" content=\"width=device-width,initial-scale=1\"><t"
        "itle>Fermentation</title><style>body{font-family:system-ui;max-width"
        ":48rem;margin:auto;padding:1rem}main{display:grid;gap:1rem}section{b"
        "order:1px solid #bbb;border-radius:.5rem;padding:1rem}button,input,s"
        "elect{font:inherit;padding:.45rem;min-height:2.75rem}#connection{fon"
        "t-weight:600}#cols{display:grid;gap:1rem}@media(min-width:40rem){#co"
        "ls{grid-template-columns:1fr 1fr}}.x{opacity:.5}.safety-fault,.decis"
        "ion-required{color:#b00}</style></head><body><main><h1>Fermentation<"
        "/h1><p id=\"auth-state\"></p><p id=\"warning\" hidden></p><form id=\""
        "login\"><label><span id=\"password-label\">Password</span> <input id"
        "=\"password\"type=\"password\" autocomplete=\"current-password\"></l"
        "abel><button>Login</button></form><button id=\"logout\" hidden>Logou"
        "t</button><section><div id=\"connection\">Offline</div></section><se"
        "ction id=\"board\" hidden><h2 id=\"mode\"></h2><div id=\"cols\"><div"
        " id=\"temps\"></div><div id=\"alerts\"></div></div></section><sectio"
        "n id=\"ops\" hidden><button data-a=\"ack\"></button> <button data-a="
        "\"mute\"></button> <button data-a=\"stop\"></button> <button data-a="
        "\"done\"></button> <select id=\"pg\"></select> <button data-a=\"star"
        "t\"></button> <span id=\"note\"></span></section></main><script>cons"
        "t S=";
    html += "'";
    html += csrfToken;
    html += "';const A='";
    html += authenticationStateName(state);
    html += "';const U=";
    html += authenticated ? "true" : "false";
    html += ";let Q=";
    html += std::to_string(mutationSequence);
    html +=
        ";const E='password-protected',D='password-disabled',N=Date.now,V"
        "='no-store',J='hidden',X='textContent',Y='replaceChildren',Z='pr"
        "ogramCatalog';const L={de:{login:'Anmelden',logout:'Abmelden',of"
        "fline:'Offline',stale:'Veraltet',online:'Online',rq:'Lokale Prov"
        "isionierung oder Recovery erforderlich',pl:'Passwort',pt:'Passwo"
        "rtschutz aktiv',ds:'Passwortschutz deaktiviert',ack:'Quittieren'"
        ",mute:'Stumm',stop:'Stop',done:'Abschliessen',start:'Starten',su"
        "re:'Sicher?',ok:'Erledigt',fail:'Nicht ausgefuehrt',age:'Stand v"
        "or',failed:'Fehler',alerts:'Meldungen',qd:'quittiert',sm:'stumm'"
        ",standby:'Bereit','active-run':'Lauf aktiv',waiting:'Wartet',com"
        "pleted:'Abgeschlossen',restricted:'Eingeschraenkt',recovery:'Wie"
        "derherstellung',unavailable:'Nicht verfuegbar','cabinet-air':'Sc"
        "hrank',product:'Produkt',cooling:'Kuehlung'},en:{login:'Login',l"
        "ogout:'Logout',offline:'Offline',stale:'Stale',online:'Online',r"
        "q:'Local provisioning or recovery required',pl:'Password',pt:'Pa"
        "ssword protection enabled',ds:'Password protection is disabled',"
        "ack:'Acknowledge',mute:'Mute',stop:'Stop',done:'Complete',start:"
        "'Start',sure:'Sure?',ok:'Done',fail:'Not executed',age:'Age',fai"
        "led:'Failed',alerts:'Alerts',qd:'acknowledged',sm:'muted',standb"
        "y:'Ready','active-run':'Run active',waiting:'Waiting',completed:"
        "'Completed',restricted:'Restricted',recovery:'Recovery',unavaila"
        "ble:'Unavailable','cabinet-air':'Cabinet',product:'Product',cool"
        "ing:'Cooling'},es:{login:'Iniciar sesion',logout:'Cerrar sesion'"
        ",offline:'Sin conexion',stale:'Anticuado',online:'En linea',rq:'"
        "Se requiere provision local o recuperacion',pl:'Contrasena',pt:'"
        "Proteccion por contrasena activa',ds:'Proteccion por contrasena "
        "desactivada',ack:'Confirmar',mute:'Silenciar',stop:'Parar',done:"
        "'Finalizar',start:'Iniciar',sure:'Seguro?',ok:'Hecho',fail:'No e"
        "jecutado',age:'Hace',failed:'Fallo',alerts:'Alertas',qd:'confirm"
        "ada',sm:'silenciada',standby:'Listo','active-run':'Activo',waiti"
        "ng:'Esperando',completed:'Finalizado',restricted:'Restringido',r"
        "ecovery:'Recuperacion',unavailable:'No disponible','cabinet-air'"
        ":'Armario',product:'Producto',cooling:'Enfriamiento'}};const q=x"
        "=>document.querySelector(x),g=(navigator.language||'en').slice(0"
        ",2),lc=L[g]?g:'en',T=L[lc];document.documentElement.lang=lc;q('#"
        "login button')[X]=T.login;q('#password-label')[X]=T.pl;const au="
        "q('#auth-state'),wr=q('#warning'),lg=q('#login'),lo=q('#logout')"
        ",cn=q('#connection'),md=q('#mode'),tp=q('#temps'),ls=q('#alerts'"
        "),bd=q('#board');lo[X]=T.logout;au[X]=A===E?T.pt:A===D?T.ds:T.rq"
        ";wr[X]=T.ds;wr[J]=A!==D;lg[J]=A!==E||U;lo[J]=!U;function el(x,c)"
        "{const e=document.createElement('p');e[X]=x;e.className=c||'';re"
        "turn e;}function dr(){const f=P?'online':LP?'stale':'offline';cn"
        "[X]=T[f]+(f==='stale'?' \u00b7 '+T.age+' '+((N()-LT)/1e3|0)+' s':'');"
        "cn.dataset.state=f;bd.className=P?'':'x';bd[J]=!LP;if(!LP)return"
        ";const[a,b,c]=LP;md[X]=T[a.homeMode]||a.homeMode;tp[Y](...['cabi"
        "net-air','product','cooling'].map(r=>{const e=b.temperatures.fin"
        "d(x=>x.role===r);return el(T[r]+': '+(!e?T.unavailable:e.valid&&"
        "e.valueCelsius!=null?e.valueCelsius.toFixed(1)+' \u00b0C':'-- '+(T[e."
        "quality]||T.unavailable)));}));const n=c.alerts.filter(x=>x.acti"
        "ve);ls[Y](el(T.alerts+': '+n.length),...n.map(x=>el(x.code.repla"
        "ce(/-/g,' ')+(x.acknowledged?' \u00b7 '+T.qd:'')+(x.muted?' \u00b7 "
        "'+T.sm:"
        "''),x.severity)));}function rf(f=0){const t0=N();if(LT){if(t0-LT"
        ">15000)P=null;ui();dr();}if(f){++G;C&&C.abort();if(!W)W=(async()"
        "=>{await F;W=0;return rf();})();return W;}if(F||W)return;const m"
        "y=++G,c=new AbortController();C=c;const e=()=>{c.abort();if(my=="
        "=G){P=null;ui();dr();}},z=setTimeout(e,3e4),r=['status','tempera"
        "tures','alerts'].map(async x=>{const a=await fetch('/api/v1/'+x,"
        "{cache:V,signal:c.signal});if(!a.ok)throw 0;return a.json();});r"
        "eturn F=(async()=>{try{const p=await Promise.all(r);if(my!==G||c"
        ".signal.aborted)return;LP=p;LT=t0;const v=p[0].revisions[Z];P=N("
        ")-t0<=15000?p:null;if(P&&U&&Q&&v&&v!==PC)lp();ui();dr();}catch{e"
        "();}finally{await Promise.allSettled(r);clearTimeout(z);F=C=0;}}"
        ")();}lg.addEventListener('submit',async e=>{e.preventDefault();c"
        "onst r=await fetch('/api/v1/login',{method:'POST',headers:{'Cont"
        "ent-Type':'application/json'},body:JSON.stringify({password:q('#"
        "password').value})});if(r.ok)location.reload();});lo.onclick=asy"
        "nc()=>{await fetch('/api/v1/logout',{method:'POST',headers:{'Con"
        "tent-Type':'application/json','X-CSRF-Token':window.__WEB_CSRF__"
        "||''}});window.__WEB_CSRF__='';Q=0;P=null;ui();lo[J]=1;lg[J]=A!="
        "=E;};const ops=q('#ops'),nt=q('#note'),pg=q('#pg');let P=null,B="
        "false,LB=0,K='',KC='',PC='',G=0,F=0,C=0,W=0,LP=null,LT=0;pg.onch"
        "ange=()=>ui();function al(f){return P&&P[2].alerts.find(m=>m.act"
        "ive&&f(m));}function ui(){ops[J]=!U||!Q;const h=P&&P[0].homeMode"
        ",r=h==='active-run'||h==='waiting',a=al(m=>!m.acknowledged),u=al"
        "(m=>!m.muted);const d={ack:!a,mute:!u,stop:!r,done:h!=='complete"
        "d',start:h!=='standby'||!pg.value||!PC||P[0].revisions[Z]!==PC},"
        "g={ack:a,mute:u},c=P?[h,P[0].revisions.run,P[0].revisions[Z],PC,"
        "pg.value].join():'';if(!P||KC&&c!==KC)K='';KC=K?KC||c:'';ops.que"
        "rySelectorAll('button').forEach(b=>{const k=b.dataset.a;b.disabl"
        "ed=B||!P||d[k];if(K!==k)b[X]=T[k]+(g[k]?' #'+g[k].id:'');});}asy"
        "nc function send(i){K='';B=true;ui();const m={stateSequence:'s',"
        "run:'r',messages:'m',fault:'f',recoveryEpisode:'e',userConfigura"
        "tion:'u',programCatalog:'c'},v=P[0].revisions,o={};for(const k i"
        "n v)if(m[k])o[m[k]]=v[k];if(i.t==='start-program')o.c=PC;const b"
        "ody=JSON.stringify({v:1,r:o,i}),n=Q;let r=null,j=null;for(let t="
        "0;t<2&&!r;t++){try{r=await fetch('/internal/ui/run',{method:'POS"
        "T',headers:{'Content-Type':'application/json','X-CSRF-Token':win"
        "dow.__WEB_CSRF__,'X-Mutation-Seq':String(n)},body});}catch(e){}}"
        "if(r&&r.status!==503){try{j=await r.json();}catch(e){}}if(!j||!/"
        "^(applied|stale|busy|confirmation-required|rejected|too-large|wr"
        "ite-failed)$/.test(j.outcome)){location.reload();return;}Q=n+1;P"
        "=null;B=false;ui();dr();nt[X]=j.outcome==='applied'?T.ok:T.fail;"
        "await rf(1);}ops.onclick=e=>{const a=e.target.dataset&&e.target."
        "dataset.a;if(!a||B||e.target.disabled)return;if(a==='ack'||a==='"
        "mute'){const m=al(x=>a==='ack'?!x.acknowledged:!x.muted);if(m)se"
        "nd({t:a+'-message',id:m.id});return;}if(K!==a){K=a;KC='';ui();e."
        "target[X]=T.sure;return;}K='';send(a==='start'?{t:'start-program"
        "',c:{p:pg.value}}:a==='stop'?{t:'stop-run',o:'abort-and-turn-off"
        "'}:{t:'complete-run',c:false});};async function lp(){if(LB)retur"
        "n;LB=1;PC='';pg[Y]();try{const r=await fetch('/api/v1/programs',"
        "{cache:V});const j=r.ok&&await r.json();if(j&&/^[0-9]+$/.test(j."
        "catalogRevision)){pg[Y](...j.programs.map(x=>new Option(x.name,x"
        ".id)));PC=j.catalogRevision;}}catch(e){}LB=0;ui();}window.__WEB_"
        "CSRF__=S;dr();rf();setInterval(rf,5000);</script></body></html>";
    return html;
}
}  // namespace shell_golden
