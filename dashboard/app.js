/* =====================================================================
   PRAHARI Control Station — dashboard logic
   Team Cipher · SIH26039
   Data source: MQTT over WebSockets (HiveMQ public broker by default)
   Also includes a scripted DEMO MODE that needs no hardware.
   ===================================================================== */

// ---------- CONFIG ----------
const MQTT_HOST = "wss://broker.hivemq.com:8884/mqtt"; // free public broker, WSS
const TOPIC     = "prahari/cipher/telemetry";           // rover publishes here
const CLIENT_ID = "prahari-dash-" + Math.random().toString(16).slice(2, 8);

// DGMS / CMR 2017 statutory limits used for GO / NO-GO scoring
const LIMITS = {
  ch4:  { warn: 0.75, danger: 1.25, max: 2.0 },   // % by volume
  co:   { warn: 25,   danger: 50,   max: 100  },  // ppm
  temp: { warn: 40,   danger: 45,   max: 60   },  // deg C
  tilt: { warn: 25,   danger: 35,   max: 60   }   // degrees
};

// ---------- STATE ----------
let missionStart = Date.now();
let breadcrumbCount = 0;
let simTimer = null, simStep = 0;

// ---------- CHART ----------
const ctx = document.getElementById("trend").getContext("2d");
const trend = new Chart(ctx, {
  type: "line",
  data: { labels: [], datasets: [
    { label: "CH4 (%)", data: [], borderColor: "#f5a623", backgroundColor: "rgba(245,166,35,.1)", tension:.35, fill:true, yAxisID:"y" },
    { label: "CO (ppm)", data: [], borderColor: "#3b82f6", backgroundColor: "rgba(59,130,246,.1)", tension:.35, fill:true, yAxisID:"y1" }
  ]},
  options: {
    responsive:true, maintainAspectRatio:false, animation:false,
    plugins:{ legend:{ labels:{ color:"#94a3b8", font:{size:11} } } },
    scales:{
      x:{ ticks:{ color:"#64748b", font:{size:9} }, grid:{ color:"#1f2937" } },
      y:{ position:"left", ticks:{ color:"#f5a623", font:{size:9} }, grid:{ color:"#1f2937" }, title:{display:true,text:"CH4 %",color:"#f5a623",font:{size:9}} },
      y1:{ position:"right", ticks:{ color:"#3b82f6", font:{size:9} }, grid:{ drawOnChartArea:false }, title:{display:true,text:"CO ppm",color:"#3b82f6",font:{size:9}} }
    }
  }
});

// ---------- HELPERS ----------
function level(v, lim){ if(v>=lim.danger) return "danger"; if(v>=lim.warn) return "warn"; return "ok"; }
function pct(v, lim){ return Math.min(100, (v/lim.max)*100); }
function pad(n){ return n<10 ? "0"+n : ""+n; }

function log(msg, cls){
  const box = document.getElementById("logbox");
  const t = new Date().toLocaleTimeString();
  const div = document.createElement("div");
  div.innerHTML = `<span class="l-time">[${t}]</span> <span class="l-${cls}">${msg}</span>`;
  box.prepend(div);
  while(box.children.length > 60) box.removeChild(box.lastChild);
}

function dropBreadcrumb(rssi, ch4){
  breadcrumbCount++;
  const c = document.getElementById("crumbs");
  const el = document.createElement("div");
  const danger = ch4 >= LIMITS.ch4.danger;
  el.className = "crumb" + (danger ? " danger" : "");
  el.innerHTML = `<b>NODE-${pad(breadcrumbCount)}</b>RSSI ${rssi} dBm<br>CH4 ${ch4.toFixed(2)}%`;
  c.appendChild(el);
  log(`Breadcrumb NODE-${pad(breadcrumbCount)} deployed (RSSI ${rssi} dBm) - now a fixed gas station`, "warn");
}

// ---------- MAIN UPDATE ----------
function update(d){
  setGauge("ch4","ch4bar","g-ch4", d.ch4, LIMITS.ch4, d.ch4.toFixed(2));
  setGauge("co","cobar","g-co", d.co, LIMITS.co, Math.round(d.co));
  setGauge("temp","tempbar","g-temp", d.temp, LIMITS.temp, Math.round(d.temp));
  setGauge("tilt","tiltbar","g-tilt", d.tilt, LIMITS.tilt, Math.round(d.tilt));

  const lv = [level(d.ch4,LIMITS.ch4), level(d.co,LIMITS.co), level(d.temp,LIMITS.temp), level(d.tilt,LIMITS.tilt)];
  const worst = lv.includes("danger") ? "danger" : lv.includes("warn") ? "warn" : "safe";
  setVerdict(worst, d);

  const t = new Date().toLocaleTimeString().slice(3);
  trend.data.labels.push(t);
  trend.data.datasets[0].data.push(d.ch4);
  trend.data.datasets[1].data.push(d.co);
  if(trend.data.labels.length > 30){ trend.data.labels.shift(); trend.data.datasets.forEach(s=>s.data.shift()); }
  trend.update();

  mlForecast();

  if(d.victim){
    const v = document.getElementById("victim");
    v.className = "victim found";
    document.getElementById("victimTitle").textContent = "⚠ PROBABLE VICTIM DETECTED";
    document.getElementById("victimSub").textContent = "Thermal + motion + audio fusion · location pinned on map";
  }

  if(d.drop) dropBreadcrumb(d.rssi, d.ch4);
}

function setGauge(valId, barId, cardId, v, lim, display){
  document.getElementById(valId).textContent = display;
  document.getElementById(barId).style.width = pct(v,lim) + "%";
  document.getElementById(cardId).className = "gauge " + (level(v,lim)==="ok"?"":level(v,lim));
}

function setVerdict(worst, d){
  const el = document.getElementById("verdict");
  el.className = "verdict " + worst;
  const title = document.getElementById("verdictTitle");
  const sub = document.getElementById("verdictSub");
  const icon = el.querySelector(".verdict-icon");
  if(worst==="safe"){ title.textContent="ATMOSPHERE SAFE — GO"; sub.textContent="All gases within DGMS statutory limits"; icon.textContent="✓"; }
  else if(worst==="warn"){ title.textContent="CAUTION — RISING HAZARD"; sub.textContent="Approaching DGMS limit · monitor closely"; icon.textContent="!"; }
  else { title.textContent="DANGER — NO-GO · DO NOT ENTER"; sub.textContent="DGMS statutory limit exceeded · rescue team hold"; icon.textContent="✕"; }
}

function mlForecast(){
  const ch4 = trend.data.datasets[0].data;
  const co  = trend.data.datasets[1].data;
  const badge = document.getElementById("predBadge");
  const detail = document.getElementById("predDetail");
  if(ch4.length < 5){ badge.className="pred-badge stable"; badge.textContent="STABLE"; detail.textContent="Collecting baseline…"; return; }
  const n = ch4.length;
  const slopeCh4 = (ch4[n-1]-ch4[n-5])/4;
  const slopeCo  = (co[n-1]-co[n-5])/4;
  const proj = ch4[n-1] + slopeCh4*10;
  if(proj >= LIMITS.ch4.danger || slopeCo > 6){
    badge.className="pred-badge critical"; badge.textContent="CRITICAL SOON";
    detail.textContent=`Rate-of-rise trend projects CH4 ~ ${proj.toFixed(2)}% within ~10 s. Recommend abort/return-to-home.`;
  } else if(slopeCh4 > 0.03 || slopeCo > 2){
    badge.className="pred-badge rising"; badge.textContent="RISING";
    detail.textContent=`Positive rate-of-rise detected (CH4 +${(slopeCh4).toFixed(3)}%/s). Advancing with caution.`;
  } else {
    badge.className="pred-badge stable"; badge.textContent="STABLE";
    detail.textContent="Rate-of-rise within normal bounds.";
  }
}

// ---------- clocks ----------
setInterval(()=>{
  const s = Math.floor((Date.now()-missionStart)/1000);
  document.getElementById("missionTime").textContent = pad(Math.floor(s/60))+":"+pad(s%60);
}, 1000);

// ---------- camera ----------
document.getElementById("camBtn").onclick = ()=>{
  const url = document.getElementById("camUrl").value.trim();
  if(!url) return;
  const img = document.getElementById("camFeed");
  img.src = url; img.style.display="block";
  document.getElementById("camPlaceholder").style.display="none";
  log("Camera stream connected: "+url, "ok");
};

// ---------- MQTT ----------
let client;
function connectMQTT(){
  try{
    client = mqtt.connect(MQTT_HOST, { clientId: CLIENT_ID, connectTimeout: 4000 });
    client.on("connect", ()=>{
      setConn(true, "Live · MQTT connected");
      client.subscribe(TOPIC);
      log("Connected to control-station broker · subscribed to "+TOPIC, "ok");
    });
    client.on("message", (topic, payload)=>{
      try{ update(JSON.parse(payload.toString())); }
      catch(e){ console.warn("bad payload", e); }
    });
    client.on("error", ()=> setConn(false, "MQTT error"));
    client.on("close", ()=> setConn(false, "Disconnected"));
  }catch(e){ setConn(false, "MQTT unavailable"); }
}
function setConn(ok, txt){
  document.getElementById("dot").className = "dot " + (ok?"online":"offline");
  document.getElementById("connText").textContent = txt;
}
connectMQTT();

// ---------- DEMO MODE (scripted mission, no hardware needed) ----------
const script = [
  {ch4:0.10, co:2,  temp:28, tilt:2,  msg:"Rover deployed into mock-drift · autonomous navigation started", cls:"ok"},
  {ch4:0.15, co:4,  temp:29, tilt:5,  msg:"Obstacle detected at 14 cm · reversing and turning right", cls:"ok"},
  {ch4:0.22, co:6,  temp:30, tilt:3},
  {ch4:0.35, co:9,  temp:31, tilt:8,  msg:"Advancing deeper · gas rising slowly", cls:"ok"},
  {ch4:0.48, co:14, temp:33, tilt:6,  drop:true, rssi:-81},
  {ch4:0.62, co:19, temp:35, tilt:4,  msg:"Rate-of-rise trend flagged by ML · caution", cls:"warn"},
  {ch4:0.78, co:24, temp:38, tilt:12, msg:"CAUTION zone entered · CH4 above 0.75%", cls:"warn"},
  {ch4:0.95, co:31, temp:41, tilt:9,  drop:true, rssi:-84},
  {ch4:1.10, co:42, temp:44, tilt:7,  victim:true, msg:"THERMAL + MOTION signature - probable trapped worker located", cls:"crit"},
  {ch4:1.32, co:55, temp:47, tilt:5,  msg:"DANGER · DGMS limit exceeded · NO-GO issued to rescue team", cls:"crit"},
  {ch4:1.28, co:52, temp:46, tilt:38, msg:"Tilt cut-off triggered (38 deg) · motors safely stopped", cls:"crit"},
  {ch4:0.90, co:34, temp:42, tilt:6,  msg:"Return-to-home · retracing odometry path", cls:"warn"},
  {ch4:0.40, co:12, temp:34, tilt:3,  msg:"Rover recovered · black-box mission log saved", cls:"ok"},
];
document.getElementById("simBtn").onclick = function(){
  if(simTimer){ clearInterval(simTimer); simTimer=null; this.classList.remove("active"); this.textContent="▶ Demo Mode"; return; }
  this.classList.add("active"); this.textContent="⏸ Stop Demo";
  missionStart = Date.now(); simStep = 0;
  setConn(true, "Demo Mode · scripted mission");
  simTimer = setInterval(()=>{
    const s = script[simStep % script.length];
    update({
      ch4: s.ch4 + (Math.random()-.5)*.03,
      co:  s.co  + (Math.random()-.5)*1,
      temp:s.temp+ (Math.random()-.5)*.5,
      tilt:s.tilt+ (Math.random()-.5)*1,
      rssi:s.rssi||-70, victim:s.victim||false, drop:s.drop||false
    });
    if(s.msg) log(s.msg, s.cls||"ok");
    simStep++;
  }, 2200);
};

log("Control Station initialised · awaiting rover telemetry", "ok");
