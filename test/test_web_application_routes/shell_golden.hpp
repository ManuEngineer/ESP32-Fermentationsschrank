#pragma once

// Frozen generator from product ab0ae55, independent of the split
// implementation.
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
        ";const L={de:{login:'Anmelden',logout:'Abmelden',offline:'Offline',s"
        "tale:'Veraltet',online:'Online',rq:'Lokale Provisionierung oder Reco"
        "very erforderlich',pl:'Passwort',pt:'Passwortschutz aktiv',ds:'Passw"
        "ortschutz deaktiviert',ack:'Quittieren',mute:'Stumm',stop:'Stop',don"
        "e:'Abschliessen',start:'Starten',sure:'Sicher?',ok:'Erledigt',fail:'"
        "Nicht ausgefuehrt',age:'Stand vor',failed:'Fehler',alerts:'Meldungen"
        "',qd:'quittiert',sm:'stumm',standby:'Bereit','active-run':'Lauf akti"
        "v',waiting:'Wartet',completed:'Abgeschlossen',restricted:'Eingeschra"
        "enkt',recovery:'Wiederherstellung',unavailable:'Nicht verfuegbar','c"
        "abinet-air':'Schrank',product:'Produkt',cooling:'Kuehlung'},en:{logi"
        "n:'Login',logout:'Logout',offline:'Offline',stale:'Stale',online:'On"
        "line',rq:'Local provisioning or recovery required',pl:'Password',pt:"
        "'Password protection enabled',ds:'Password protection is disabled',a"
        "ck:'Acknowledge',mute:'Mute',stop:'Stop',done:'Complete',start:'Star"
        "t',sure:'Sure?',ok:'Done',fail:'Not executed',age:'Age',failed:'Fail"
        "ed',alerts:'Alerts',qd:'acknowledged',sm:'muted',standby:'Ready','ac"
        "tive-run':'Run active',waiting:'Waiting',completed:'Completed',restr"
        "icted:'Restricted',recovery:'Recovery',unavailable:'Unavailable','ca"
        "binet-air':'Cabinet',product:'Product',cooling:'Cooling'},es:{login:"
        "'Iniciar sesion',logout:'Cerrar sesion',offline:'Sin conexion',stale"
        ":'Anticuado',online:'En linea',rq:'Se requiere provision local o rec"
        "uperacion',pl:'Contrasena',pt:'Proteccion por contrasena activa',ds:"
        "'Proteccion por contrasena desactivada',ack:'Confirmar',mute:'Silenc"
        "iar',stop:'Parar',done:'Finalizar',start:'Iniciar',sure:'Seguro?',ok"
        ":'Hecho',fail:'No ejecutado',age:'Hace',failed:'Fallo',alerts:'Alert"
        "as',qd:'confirmada',sm:'silenciada',standby:'Listo','active-run':'Ac"
        "tivo',waiting:'Esperando',completed:'Finalizado',restricted:'Restrin"
        "gido',recovery:'Recuperacion',unavailable:'No disponible','cabinet-a"
        "ir':'Armario',product:'Producto',cooling:'Enfriamiento'}};const q=x="
        ">document.querySelector(x),g=(navigator.language||'en').slice(0,2),l"
        "ocale=L[g]?g:'en',T=L[locale];document.documentElement.lang=locale;q"
        "('#login button').textContent=T.login;q('#password-label').textConte"
        "nt=T.pl;const au=q('#auth-state'),wr=q('#warning'),login=q('#login')"
        ",lo=q('#logout'),cn=q('#connection'),md=q('#mode'),tp=q('#temps'),ls"
        "=q('#alerts'),bd=q('#board');lo.textContent=T.logout;au.textContent="
        "A==='password-protected'?T.pt:A==='password-disabled'?T.ds:T.rq;wr.t"
        "extContent=T.ds;wr.hidden=A!=='password-disabled';login.hidden=A!=='"
        "password-protected'||U;lo.hidden=!U;function el(x,c){const e=documen"
        "t.createElement('p');e.textContent=x;e.className=c||'';return e;}fun"
        "ction draw(){const f=P?'online':LP?'stale':'offline';cn.textContent="
        "T[f]+(f==='stale'?' \u00b7 '+T.age+' '+((Date.now()-LT)/1e3|0)+' s':"
        "'');cn.dataset.state=f;bd.className=P?'':'x';bd.hidden=!LP;if(!LP)re"
        "turn;const[a,b,c]=LP;md.textContent=T[a.homeMode]||a.homeMode;tp.rep"
        "laceChildren(...['cabinet-air','product','cooling'].map(r=>{const e="
        "b.temperatures.find(x=>x.role===r);return el(T[r]+': '+(!e?T.unavail"
        "able:e.valid&&e.valueCelsius!=null?e.valueCelsius.toFixed(1)+' \u00b0"
        "C':'-- '+(T[e.quality]||T.unavailable)));}));const n=c.alerts.filter"
        "(x=>x.active);ls.replaceChildren(el(T.alerts+': '+n.length),...n.map"
        "(x=>el(x.code.replace(/-/g,' ')+(x.acknowledged?' \u00b7 '+T.qd:'')+"
        "(x.muted?' \u00b7 '+T.sm:''),x.severity)));}async function rf(){cons"
        "t my=++G,t0=Date.now();if(LT){if(t0-LT>15000)P=null;ui();draw();}try"
        "{const r=await Promise.all(['status','temperatures','alerts'].map(x="
        ">fetch('/api/v1/'+x,{cache:'no-store'})));if(r.some(x=>!x.ok))throw "
        "0;const p=await Promise.all(r.map(x=>x.json()));if(my<H)return;H=my;"
        "if(t0>LT){LP=p;LT=t0;}const c=p[0].revisions;P=Date.now()-t0<=15000?"
        "p:null;if(P&&U&&Q&&c.programCatalog&&c.programCatalog!==PC)lp();ui()"
        ";draw();}catch(e){if(my<H)return;H=my;P=null;ui();draw();}}login.add"
        "EventListener('submit',async e=>{e.preventDefault();const r=await fe"
        "tch('/api/v1/login',{method:'POST',headers:{'Content-Type':'applicat"
        "ion/json'},body:JSON.stringify({password:q('#password').value})});if"
        "(r.ok)location.reload();});lo.onclick=async()=>{await fetch('/api/v1"
        "/logout',{method:'POST',headers:{'Content-Type':'application/json','"
        "X-CSRF-Token':window.__WEB_CSRF__||''}});window.__WEB_CSRF__='';Q=0;"
        "P=null;ui();lo.hidden=true;login.hidden=A!=='password-protected';};c"
        "onst ops=q('#ops'),note=q('#note'),pg=q('#pg');let P=null,B=false,LB"
        "=false,K='',KC='',PC='',G=0,H=0,LP=null,LT=0;pg.onchange=()=>ui();fu"
        "nction al(f){return P&&P[2].alerts.find(m=>m.active&&f(m));}function"
        " ui(){ops.hidden=!U||!Q;const h=P&&P[0].homeMode,r=h==='active-run'|"
        "|h==='waiting',a=al(m=>!m.acknowledged),u=al(m=>!m.muted);const d={a"
        "ck:!a,mute:!u,stop:!r,done:h!=='completed',start:h!=='standby'||!pg."
        "value||!PC||P[0].revisions.programCatalog!==PC},g={ack:a,mute:u},c=P"
        "?[h,P[0].revisions.run,P[0].revisions.programCatalog,PC,pg.value].jo"
        "in():'';if(!P||KC&&c!==KC)K='';KC=K?KC||c:'';ops.querySelectorAll('b"
        "utton').forEach(b=>{const k=b.dataset.a;b.disabled=B||!P||d[k];if(K!"
        "==k)b.textContent=T[k]+(g[k]?' #'+g[k].id:'');});}async function sen"
        "d(i){K='';B=true;ui();const m={stateSequence:'s',run:'r',messages:'m"
        "',fault:'f',recoveryEpisode:'e',userConfiguration:'u',programCatalog"
        ":'c'},v=P[0].revisions,o={};for(const k in v)if(m[k])o[m[k]]=v[k];if"
        "(i.t==='start-program')o.c=PC;const body=JSON.stringify({v:1,r:o,i})"
        ",n=Q;let r=null,j=null;for(let t=0;t<2&&!r;t++){try{r=await fetch('/"
        "internal/ui/run',{method:'POST',headers:{'Content-Type':'application"
        "/json','X-CSRF-Token':window.__WEB_CSRF__,'X-Mutation-Seq':String(n)"
        "},body});}catch(e){}}if(r&&r.status!==503){try{j=await r.json();}cat"
        "ch(e){}}if(!j||!/^(applied|stale|busy|confirmation-required|rejected"
        "|too-large|write-failed)$/.test(j.outcome)){location.reload();return"
        ";}Q=n+1;B=false;note.textContent=j.outcome==='applied'?T.ok:T.fail;a"
        "wait rf();}ops.onclick=e=>{const a=e.target.dataset&&e.target.datase"
        "t.a;if(!a||B||e.target.disabled)return;if(a==='ack'||a==='mute'){con"
        "st m=al(x=>a==='ack'?!x.acknowledged:!x.muted);if(m)send({t:a+'-mess"
        "age',id:m.id});return;}if(K!==a){K=a;KC='';ui();e.target.textContent"
        "=T.sure;return;}K='';send(a==='start'?{t:'start-program',c:{p:pg.val"
        "ue}}:a==='stop'?{t:'stop-run',o:'abort-and-turn-off'}:{t:'complete-r"
        "un',c:false});};async function lp(){if(LB)return;LB=true;PC='';pg.re"
        "placeChildren();try{const r=await fetch('/api/v1/programs',{cache:'n"
        "o-store'});const j=r.ok&&await r.json();if(j&&/^[0-9]+$/.test(j.cata"
        "logRevision)){pg.replaceChildren(...j.programs.map(x=>new Option(x.n"
        "ame,x.id)));PC=j.catalogRevision;}}catch(e){}LB=false;ui();}window._"
        "_WEB_CSRF__=S;draw();rf();setInterval(rf,5000);</script></body></htm"
        "l>";
    return html;
}
}  // namespace shell_golden
