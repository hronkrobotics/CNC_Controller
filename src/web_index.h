#pragma once
// Auto-generated single-page web UI (mirrors the LVGL pendant). See webserver.cpp.
static const char WEB_INDEX_HTML[] = R"WEBUI(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<meta name="theme-color" content="#15181C">
<title>CNC Pendant</title>
<style>
  /* ---- Theme tokens (mirror the device palette) ---- */
  :root{
    --bg:#15181C; --panel:#242A31; --panel2:#1c2127; --border:#333B44;
    --accent:#17C3B2; --text:#E6E9ED; --muted:#8A929B;
    --idle:#2EA043; --run:#2F81F7; --hold:#FFB300; --alarm:#E5484D; --disc:#3A424B;
    --sans:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
    --mono:ui-monospace,Menlo,Consolas,"SFMono-Regular",monospace;
    --radius:12px;
  }
  *{box-sizing:border-box}
  html,body{margin:0;height:100%}
  body{
    background:var(--bg); color:var(--text); font-family:var(--sans);
    font-size:15px; line-height:1.4; -webkit-font-smoothing:antialiased;
    padding:12px; padding-bottom:24px;
  }
  h1,h2,h3{margin:0}
  button{font-family:inherit;color:inherit;cursor:pointer}
  input,select{font-family:inherit;font-size:14px}

  /* ---- Layout grid ---- */
  .wrap{max-width:1100px;margin:0 auto;display:flex;flex-direction:column;gap:12px}
  .cols{display:grid;grid-template-columns:1fr;gap:12px}
  @media(min-width:860px){ .cols{grid-template-columns:1fr 1fr} }
  .col{display:flex;flex-direction:column;gap:12px;min-width:0}

  /* ---- Panels ---- */
  .panel{background:var(--panel);border:1px solid var(--border);border-radius:var(--radius);padding:14px}
  .panel h2{font-size:12px;letter-spacing:.14em;text-transform:uppercase;color:var(--muted);font-weight:600;margin-bottom:10px}
  .row{display:flex;align-items:center;gap:8px}
  .spread{justify-content:space-between}

  /* ---- Header ---- */
  header{display:flex;flex-wrap:wrap;align-items:center;gap:12px}
  .brand{display:flex;align-items:center;gap:10px}
  .brand .dot{width:11px;height:11px;border-radius:50%;background:var(--accent);box-shadow:0 0 10px var(--accent)}
  .brand h1{font-size:17px;letter-spacing:.18em;font-weight:700}
  .chip{
    flex:1;min-width:150px;text-align:center;font-weight:700;letter-spacing:.14em;
    padding:12px 14px;border-radius:var(--radius);background:var(--disc);color:#fff;
    text-transform:uppercase;transition:background .2s;border:1px solid rgba(255,255,255,.08);
  }
  .wifi{font-size:12px;color:var(--muted);text-align:right;min-width:150px;line-height:1.5}
  .wifi b{color:var(--text);font-weight:600}
  .wifi .mode{display:inline-block;background:var(--panel2);border:1px solid var(--border);
    border-radius:6px;padding:1px 7px;color:var(--accent);font-weight:700;letter-spacing:.05em}

  /* ---- DRO ---- */
  .dro-head{display:flex;justify-content:space-between;align-items:center;margin-bottom:8px}
  .seg{display:inline-flex;border:1px solid var(--border);border-radius:9px;overflow:hidden;background:var(--panel2)}
  .seg button{background:transparent;border:0;padding:7px 16px;font-size:12px;font-weight:700;
    letter-spacing:.1em;color:var(--muted)}
  .seg button.on{background:var(--accent);color:#08201e}
  .axis{display:grid;grid-template-columns:26px 1fr auto;align-items:baseline;gap:10px;
    padding:8px 0;border-bottom:1px solid var(--border)}
  .axis:last-child{border-bottom:0}
  .axis .lbl{font-family:var(--mono);font-size:22px;font-weight:700;color:var(--accent)}
  .axis .big{font-family:var(--mono);font-size:clamp(30px,8vw,46px);font-weight:700;
    text-align:right;letter-spacing:.02em;font-variant-numeric:tabular-nums}
  .axis .small{font-family:var(--mono);font-size:13px;color:var(--muted);text-align:right;white-space:nowrap;font-variant-numeric:tabular-nums}
  .feednow{margin-top:8px;font-family:var(--mono);font-size:13px;color:var(--muted);text-align:right}
  .feednow b{color:var(--text)}

  /* ---- Buttons ---- */
  .btn{border:1px solid var(--border);background:var(--panel2);border-radius:10px;
    padding:12px 10px;font-size:14px;font-weight:600;text-align:center;
    user-select:none;transition:filter .12s,transform .05s;min-height:46px}
  .btn:active{transform:translateY(1px)}
  .btn:hover{filter:brightness(1.12)}
  .btn.accent{background:var(--accent);color:#08201e;border-color:transparent}
  .btn.green{background:var(--idle);color:#fff;border-color:transparent}
  .btn.blue{background:var(--run);color:#fff;border-color:transparent}
  .btn.amber{background:var(--hold);color:#241a00;border-color:transparent}
  .btn.red{background:var(--alarm);color:#fff;border-color:transparent}
  .btn[disabled]{opacity:.4;pointer-events:none}
  .grid3{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
  .grid4{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}

  /* ---- Jog cross ---- */
  .jogcross{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-bottom:10px}
  .jogcross .sp{visibility:hidden}
  .jog-opts{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin:10px 0}
  .field{display:flex;flex-direction:column;gap:4px}
  .field label{font-size:11px;color:var(--muted);letter-spacing:.06em;text-transform:uppercase}
  .field .inp{display:flex;align-items:center;gap:8px}
  select,input[type=number],input[type=text]{
    width:100%;background:var(--panel2);border:1px solid var(--border);color:var(--text);
    border-radius:9px;padding:10px}
  .unit{color:var(--muted);font-size:13px;white-space:nowrap}
  .sub{font-size:11px;color:var(--muted);letter-spacing:.1em;text-transform:uppercase;margin:6px 0 6px}

  /* ---- Console ---- */
  #console{background:var(--panel2);border:1px solid var(--border);border-radius:10px;
    height:240px;overflow-y:auto;padding:10px;font-family:var(--mono);font-size:12.5px;line-height:1.5}
  #console .o{color:var(--accent)}
  #console .i{color:#7fd598}
  #console div{white-space:pre-wrap;word-break:break-word}
  .send-row{display:flex;gap:8px;margin-top:8px}
  .send-row input{flex:1}

  /* ---- Jobs ---- */
  .job-active{background:var(--panel2);border:1px solid var(--border);border-radius:10px;padding:12px;margin-bottom:12px}
  .job-active .nm{font-family:var(--mono);font-size:14px;word-break:break-all;margin-bottom:8px}
  .bar{height:10px;background:var(--bg);border-radius:6px;overflow:hidden;border:1px solid var(--border)}
  .bar > i{display:block;height:100%;width:0;background:var(--accent);transition:width .3s}
  .job-stat{display:flex;justify-content:space-between;font-size:12px;color:var(--muted);margin:6px 0 10px;font-family:var(--mono)}
  .filelist{display:flex;flex-direction:column;gap:6px;max-height:300px;overflow-y:auto}
  .file{display:grid;grid-template-columns:1fr auto auto auto;align-items:center;gap:8px;
    background:var(--panel2);border:1px solid var(--border);border-radius:9px;padding:8px 10px}
  .file .fn{min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-family:var(--mono);font-size:13px}
  .file .sz{font-size:11px;color:var(--muted);font-family:var(--mono)}
  .badge{font-size:10px;font-weight:700;letter-spacing:.08em;padding:2px 6px;border-radius:5px;
    background:var(--bg);border:1px solid var(--border);color:var(--accent)}
  .file .mini{padding:7px 11px;min-height:0;font-size:12px}
  .empty{color:var(--muted);font-size:13px;padding:10px;text-align:center}
  .up-row{display:flex;gap:8px;margin-top:12px;align-items:center;flex-wrap:wrap}
  .up-row input[type=file]{flex:1;font-size:12px;padding:8px}
  .hidden{display:none!important}
</style>
</head>
<body>
<div class="wrap">

  <!-- ============ HEADER ============ -->
  <header class="panel">
    <div class="brand"><span class="dot"></span><h1>CNC PENDANT</h1></div>
    <div id="chip" class="chip">DISCONNECTED</div>
    <div class="wifi" id="wifi">
      <span class="mode" id="wmode">--</span> &nbsp;<b id="wip">0.0.0.0</b><br>
      <span id="wssid">--</span> &nbsp; <span id="wrssi"></span>
    </div>
  </header>

  <div class="cols">
    <!-- ============ LEFT COLUMN ============ -->
    <div class="col">

      <!-- DRO -->
      <section class="panel">
        <div class="dro-head">
          <h2 style="margin:0">Position (mm)</h2>
          <div class="seg" id="seg">
            <button data-w="1" class="on">WORK</button>
            <button data-w="0">MACH</button>
          </div>
        </div>
        <div id="axes"></div>
        <div class="feednow">FEED <b id="feed">0</b> mm/min</div>
      </section>

      <!-- Machine control -->
      <section class="panel">
        <h2>Machine</h2>
        <div class="grid3" style="margin-bottom:8px">
          <button class="btn amber" data-cmd="hold">HOLD</button>
          <button class="btn green" data-cmd="resume">RESUME</button>
          <button class="btn red" data-cmd="stop">RESET / STOP</button>
        </div>
        <button class="btn accent" style="width:100%" data-cmd="unlock">UNLOCK</button>
      </section>

      <!-- Console -->
      <section class="panel">
        <h2>Console</h2>
        <div id="console"></div>
        <div class="send-row">
          <input id="gin" type="text" placeholder="Type G-code, e.g. G0 X0 Y0" autocomplete="off" spellcheck="false">
          <button class="btn accent" id="gsend" style="min-height:0;padding:10px 16px">Send</button>
        </div>
      </section>

    </div>

    <!-- ============ RIGHT COLUMN ============ -->
    <div class="col">

      <!-- Jog -->
      <section class="panel">
        <h2>Jog</h2>
        <div class="jogcross">
          <div class="sp"></div>
          <button class="btn" data-jog="Y" data-sign="1">Y+</button>
          <button class="btn" data-jog="Z" data-sign="1">Z+</button>
          <button class="btn" data-jog="X" data-sign="-1">X-</button>
          <button class="btn red" data-cmd="jogcancel">STOP</button>
          <button class="btn" data-jog="X" data-sign="1">X+</button>
          <div class="sp"></div>
          <button class="btn" data-jog="Y" data-sign="-1">Y-</button>
          <button class="btn" data-jog="Z" data-sign="-1">Z-</button>
        </div>

        <div class="jog-opts">
          <div class="field">
            <label>Distance</label>
            <div class="inp">
              <select id="dist">
                <option>0.1</option><option selected>1</option>
                <option>10</option><option>50</option><option>100</option>
              </select>
              <span class="unit">mm</span>
            </div>
          </div>
          <div class="field">
            <label>Feed</label>
            <div class="inp">
              <input id="feedin" type="number" value="1000" min="1" step="1">
              <span class="unit">mm/min</span>
            </div>
          </div>
        </div>

        <div class="sub">Home</div>
        <div class="grid3" style="margin-bottom:10px">
          <button class="btn blue" data-home="XY">HOME XY</button>
          <button class="btn blue" data-home="Z">HOME Z</button>
          <button class="btn blue" data-home="ALL">HOME ALL</button>
        </div>

        <div class="sub">Zero Work Offset</div>
        <div class="grid4">
          <button class="btn" data-zero="X">ZERO X</button>
          <button class="btn" data-zero="Y">ZERO Y</button>
          <button class="btn" data-zero="Z">ZERO Z</button>
          <button class="btn accent" data-zero="ALL">ZERO ALL</button>
        </div>
      </section>

      <!-- Jobs -->
      <section class="panel">
        <div class="row spread" style="margin-bottom:10px">
          <h2 style="margin:0">Jobs</h2>
          <button class="btn" id="refresh" style="min-height:0;padding:6px 12px;font-size:12px">Refresh</button>
        </div>

        <!-- active job -->
        <div id="jobActive" class="job-active hidden">
          <div class="nm" id="jobName">--</div>
          <div class="bar"><i id="jobBar"></i></div>
          <div class="job-stat">
            <span id="jobLines">0 / 0</span>
            <span id="jobPct">0%</span>
          </div>
          <div class="grid3">
            <button class="btn amber" data-cmd="jobpause">PAUSE</button>
            <button class="btn green" data-cmd="jobresume">RESUME</button>
            <button class="btn red" data-cmd="jobstop">STOP</button>
          </div>
        </div>

        <div class="filelist" id="files"><div class="empty">No files loaded.</div></div>

        <div class="up-row">
          <input type="file" id="file">
          <button class="btn accent" id="upload" style="min-height:0;padding:10px 16px">Upload</button>
        </div>
      </section>

    </div>
  </div>
</div>

<script>
"use strict";
(function(){
  // ---------- tiny DOM helpers ----------
  const $  = id => document.getElementById(id);
  const el = (tag,cls) => { const e=document.createElement(tag); if(cls)e.className=cls; return e; };

  // ---------- app state ----------
  let ws=null, wsReady=false, backoff=500;
  let showWork=true;                 // WORK vs MACH big display
  let last={ conn:false, state:"" }; // remember last state for button gating

  // ---------- build the DRO axes ----------
  const AX=["X","Y","Z"];
  const axParts={};
  (function initAxes(){
    const host=$("axes");
    AX.forEach((a,i)=>{
      const row=el("div","axis");
      const lbl=el("div","lbl"); lbl.textContent=a;
      const big=el("div","big"); big.textContent="0.000";
      const sm =el("div","small"); sm.textContent="0.000";
      row.append(lbl,big,sm); host.appendChild(row);
      axParts[a]={big,small:sm};
    });
  })();

  // ---------- state color mapping ----------
  function colorFor(state){
    switch(state){
      case "Idle":  return "var(--idle)";
      case "Run": case "Jog": case "Home": return "var(--run)";
      case "Hold": case "Door": return "var(--hold)";
      case "Alarm": return "var(--alarm)";
      default: return "var(--disc)";
    }
  }

  // ---------- render incoming STATE ----------
  function applyState(m){
    last.conn=!!m.conn; last.state=m.state||"";

    // chip
    const chip=$("chip");
    if(!m.conn){
      chip.textContent="DISCONNECTED";
      chip.style.background="var(--disc)";
    }else{
      chip.textContent=(m.state||"Unknown").toUpperCase();
      chip.style.background=colorFor(m.state);
    }

    // DRO
    const work=m.work||[0,0,0], mach=m.mach||[0,0,0];
    AX.forEach((a,i)=>{
      const big = showWork? work[i] : mach[i];
      const alt = showWork? mach[i] : work[i];
      const tag = showWork? "M" : "W";
      axParts[a].big.textContent   = fmt(big);
      axParts[a].small.textContent = tag+" "+fmt(alt);
    });
    $("feed").textContent = Math.round(m.feed||0);

    // wifi
    const w=m.wifi||{};
    $("wmode").textContent = w.mode||"OFF";
    $("wip").textContent   = w.ip||"0.0.0.0";
    $("wssid").textContent = w.ssid||"--";
    $("wrssi").textContent = (w.mode==="STA" && typeof w.rssi==="number") ? (w.rssi+" dBm") : "";

    // job
    const j=m.job||{};
    const active=!!j.active;
    $("jobActive").classList.toggle("hidden", !active);
    if(active){
      $("jobName").textContent = j.name||"(job)";
      $("jobLines").textContent = (j.done||0)+" / "+(j.total||0);
      $("jobPct").textContent  = (j.pct||0)+"%";
      $("jobBar").style.width  = (j.pct||0)+"%";
    }
    gateButtons();
  }

  function fmt(n){ return (typeof n==="number"?n:0).toFixed(3); }

  // enable/disable command controls when the link is down
  function gateButtons(){
    const off=!wsReady;
    document.querySelectorAll("[data-cmd],[data-jog],[data-home],[data-zero],#gsend")
      .forEach(b=>{ b.disabled=off; });
  }

  // ---------- console ----------
  const con=$("console"); const MAXLINES=300;
  function logLine(out, s){
    const d=el("div", out?"o":"i");
    d.textContent=(out?"> ":"< ")+s;
    con.appendChild(d);
    while(con.childElementCount>MAXLINES) con.removeChild(con.firstChild);
    con.scrollTop=con.scrollHeight;
  }

  // ---------- websocket ----------
  function connect(){
    const url="ws://"+location.hostname+":81/";
    try{ ws=new WebSocket(url); }
    catch(e){ scheduleReconnect(); return; }

    ws.onopen=()=>{ wsReady=true; backoff=500; gateButtons(); };
    ws.onclose=()=>{ wsReady=false; markDown(); scheduleReconnect(); };
    ws.onerror=()=>{ try{ws.close();}catch(e){} };
    ws.onmessage=ev=>{
      let m; try{ m=JSON.parse(ev.data); }catch(e){ return; }
      if(m.t==="state") applyState(m);
      else if(m.t==="log") logLine(!!m.o, m.s||"");
    };
  }
  function scheduleReconnect(){
    setTimeout(connect, backoff);
    backoff=Math.min(backoff*1.6, 8000);
  }
  function markDown(){
    const chip=$("chip");
    chip.textContent="DISCONNECTED"; chip.style.background="var(--disc)";
    gateButtons();
  }
  function send(obj){
    if(wsReady && ws && ws.readyState===1){ ws.send(JSON.stringify(obj)); }
  }

  // ---------- inputs ----------
  function curDist(){ return parseFloat($("dist").value)||1; }
  function curFeed(){ let f=parseInt($("feedin").value,10); if(!(f>=1))f=1; $("feedin").value=f; return f; }

  // ---------- wire up controls (event delegation) ----------
  document.addEventListener("click", e=>{
    const t=e.target.closest("button"); if(!t) return;

    if(t.dataset.jog){
      const sign=parseInt(t.dataset.sign,10);
      send({c:"jog", axis:t.dataset.jog, dist:curDist()*sign, feed:curFeed()});
      return;
    }
    if(t.dataset.home){ send({c:"home", which:t.dataset.home}); return; }
    if(t.dataset.zero){ send({c:"zero", axis:t.dataset.zero}); return; }

    switch(t.dataset.cmd){
      case "hold":      send({c:"hold"}); break;
      case "resume":    send({c:"resume"}); break;
      case "stop":      send({c:"stop"}); break;
      case "unlock":    send({c:"unlock"}); break;
      case "jogcancel": send({c:"jogcancel"}); break;
      case "jobpause":  send({c:"jobpause"}); break;
      case "jobresume": send({c:"jobresume"}); break;
      case "jobstop":   send({c:"jobstop"}); break;
    }
  });

  // WORK/MACH segmented toggle
  $("seg").addEventListener("click", e=>{
    const b=e.target.closest("button"); if(!b) return;
    showWork = b.dataset.w==="1";
    $("seg").querySelectorAll("button").forEach(x=>x.classList.toggle("on", x===b));
    send({c:"posmode", work:showWork});   // keep device display in sync
  });

  // manual G-code send
  function sendGcode(){
    const v=$("gin").value.trim(); if(!v) return;
    send({c:"gcode", line:v});
    $("gin").value="";
  }
  $("gsend").addEventListener("click", sendGcode);
  $("gin").addEventListener("keydown", e=>{ if(e.key==="Enter") sendGcode(); });

  // ---------- REST: file management ----------
  async function refreshFiles(){
    try{
      const r=await fetch("/api/files",{cache:"no-store"});
      const j=await r.json();
      renderFiles(j.files||[]);
    }catch(e){ /* offline: leave list as-is */ }
  }

  function renderFiles(files){
    const host=$("files"); host.innerHTML="";
    if(!files.length){ const e=el("div","empty"); e.textContent="No files on device."; host.appendChild(e); return; }
    files.forEach(f=>{
      const row=el("div","file");
      const nm=el("div","fn"); nm.textContent=f.name; nm.title=f.path||f.name;
      const sz=el("div","sz"); sz.textContent=humanSize(f.size||0);
      const bd=el("div","badge"); bd.textContent=(f.store||"").toUpperCase()||"FLASH";
      const acts=el("div","row");
      const run=el("button","btn accent mini"); run.textContent="Run";
      run.onclick=()=>send({c:"runjob", path:f.path});
      const del=el("button","btn red mini"); del.textContent="Delete";
      del.onclick=()=>deleteFile(f.path);
      acts.append(run,del);
      row.append(nm,sz,bd,acts);
      host.appendChild(row);
    });
  }

  function humanSize(b){
    if(b<1024) return b+" B";
    if(b<1048576) return (b/1024).toFixed(1)+" KB";
    return (b/1048576).toFixed(1)+" MB";
  }

  async function deleteFile(path){
    if(!path) return;
    try{
      await fetch("/api/delete?path="+encodeURIComponent(path),{cache:"no-store"});
    }catch(e){}
    refreshFiles();
  }

  async function uploadFile(){
    const inp=$("file"); if(!inp.files||!inp.files.length){ return; }
    const btn=$("upload"); const old=btn.textContent;
    btn.textContent="..."; btn.disabled=true;
    const fd=new FormData(); fd.append("file", inp.files[0]);
    try{
      await fetch("/api/upload",{method:"POST", body:fd});
    }catch(e){}
    btn.textContent=old; btn.disabled=false; inp.value="";
    refreshFiles();
  }
  $("upload").addEventListener("click", uploadFile);
  $("refresh").addEventListener("click", refreshFiles);

  // ---------- boot ----------
  markDown();
  connect();
  refreshFiles();
})();
</script>
</body>
</html>
)WEBUI";
