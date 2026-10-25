'use strict';
/* Only presentation lives here. Every state value and integration step is C. */
const $ = id => document.getElementById(id);
const fieldNames = ['x','y','vx','vy','time','heading','radius','speed','energy','mx','my',
  'distance','relative','status','phase','dv','mode','burn1','burn2','end'];
const canvas = $('map'), ctx = canvas.getContext('2d');
const sprite = new Image();
sprite.src = 'art/tug-sprite.png';
let engine, state, paused = true, accumulator = 0, lastTime = 0, trail = [];
const keys = new Set(), pointers = new Map();
const controlButtons = [...document.querySelectorAll('[data-control]')];
const read = () => Object.fromEntries(fieldNames.map((name, i) => [name, engine._flight_value(i)]));
const pressed = name => keys.has(name) || [...pointers.values()].includes(name);
function clearControls() {
  keys.clear(); pointers.clear();
  controlButtons.forEach(button => button.setAttribute('aria-pressed', 'false'));
}
function reflectControls() {
  controlButtons.forEach(button => button.setAttribute('aria-pressed', String(pressed(button.dataset.control))));
}
function pause(value) {
  paused = value; accumulator = 0; clearControls();
  $('pause').textContent = state?.status ? 'Complete' : paused ? 'Resume' : 'Pause';
  $('pause').disabled = Boolean(state?.status);
  $('pause').setAttribute('aria-pressed', String(paused));
  update();
}
function reset(mode, start = true) {
  engine._flight_reset(mode); state = read(); trail = [[state.x, state.y]];
  $('rate').value = mode ? '4' : '1';
  controlButtons.forEach(button => button.disabled = Boolean(mode));
  pause(!start); draw();
}
function update() {
  if (!state) return;
  for (const [id, value, decimals] of [
    ['radius',state.radius,3],['speed',state.speed,3],['energy',state.energy,4],
    ['distance',state.distance,3],['relative',state.relative,3],['elapsed',state.time,2],['delta-v',state.dv,4]]) {
    $(id).textContent = value.toFixed(decimals).replace('-', '−');
  }
  const demo = Boolean(state.mode), phase = state.phase;
  $('mode-label').textContent = demo ? 'DEMONSTRATION' : 'MANUAL';
  $('mission-title').textContent = demo ? 'Two burns. A new orbit.' : 'Meet the moon.';
  $('mission-copy').textContent = demo
    ? 'Watch a star-only transfer from radius 1 to 2. The moon is a position reference; its gravity is off in this demonstration.'
    : 'Close to within 0.15 L and match its velocity to within 0.25 L/T. Turning changes where the engine points, not where you drift.';
  $('step-label').textContent = demo ? ['01 / PARKING ORBIT', '02 / TRANSFER ELLIPSE', '03 / CIRCULARIZE'][phase] : 'MANUAL FLIGHT';
  $('guidance').textContent = demo ? [
    'Coast to the departure point. The first impulse will stretch the orbit.',
    'Engine off. Follow the ellipse to its farthest point, at radius 2.',
    'The second impulse matches circular speed. Your new orbit holds at radius 2.'
  ][phase] : 'Point the nose, then burn. Your momentum does the rest.';
  const burning = !paused && (demo ?
    ((phase === 1 && state.time - state.burn1 < 0.28) || (phase === 2 && state.time - state.burn2 < 0.28)) : pressed('burn'));
  $('status').dataset.state = state.status ? 'stop' : burning ? 'burn' : 'coast';
  const statusText = state.status ? ['','STAR IMPACT / RESET TO RETRY','RENDEZVOUS ACHIEVED','TRANSFER COMPLETE / r ≈ 2'][state.status]
    : paused ? 'PAUSED / READY TO FLY' : burning ? (demo ? 'IMPULSE APPLIED' : 'ENGINE BURN / THRUST ON') : demo ? 'DEMO / COASTING' : 'MANUAL / COASTING';
  if ($('status').textContent !== statusText) $('status').textContent = statusText;
}
function line(x1, y1, x2, y2, colour, width = 1) {
  ctx.strokeStyle = colour; ctx.lineWidth = width; ctx.beginPath();
  ctx.moveTo(x1,y1); ctx.lineTo(x2,y2); ctx.stroke();
}
function draw() {
  if (!state) return;
  const box = canvas.getBoundingClientRect(), dpr = Math.min(devicePixelRatio || 1, 2);
  const width = Math.round(box.width*dpr), height = Math.round(box.height*dpr);
  if (canvas.width !== width || canvas.height !== height) { canvas.width = width; canvas.height = height; }
  ctx.setTransform(dpr,0,0,dpr,0,0);
  const w = box.width, h = box.height;
  ctx.clearRect(0,0,w,h);
  const extent = Math.max(3.7, Math.ceil(state.radius * 1.15));
  const scale = Math.min(w-60,h-60)/(extent*2), cx = w/2, cy = h/2;
  const point = (x,y) => [cx+x*scale,cy-y*scale];
  const grid = Math.max(1,Math.ceil(extent/6));
  $('plot-note').textContent = `${extent > 3.7 ? 'Auto fit' : 'Fixed scale'} · 1 grid interval = ${grid} L`;
  for(let i=-Math.floor(extent/grid); i<=Math.floor(extent/grid); i++) {
    const offset=i*grid*scale;
    line(cx+offset,0,cx+offset,h,'#8fb6c20d'); line(0,cy+offset,w,cy+offset,'#8fb6c20d');
  }
  ctx.font='10px ui-monospace,monospace'; ctx.fillStyle='#91a5b5';
  ctx.fillText('+Y',cx+7,20); ctx.fillText('+X',w-27,cy-7);
  line(cx,25,cx,h-25,'#7797a32c'); line(25,cy,w-25,cy,'#7797a32c');
  [1,2,3].forEach(r => {
    ctx.setLineDash(r===3?[2,5]:[5,6]); ctx.strokeStyle=r===3?'#b8b3e34a':'#92b3c13d';
    ctx.lineWidth=1;ctx.beginPath();ctx.arc(cx,cy,r*scale,0,Math.PI*2);ctx.stroke();
    ctx.setLineDash([]);ctx.fillStyle='#9bb0bd';
    ctx.fillText(`r = ${r}`,cx+r*scale*.707+5,cy-r*scale*.707-4);
  });
  if(trail.length>1) {
    ctx.beginPath();trail.forEach(([x,y],i)=>{const p=point(x,y);if(i)ctx.lineTo(...p);else ctx.moveTo(...p)});
    ctx.strokeStyle='#efbd84';ctx.lineWidth=1.8;ctx.stroke();
  }
  const glow=ctx.createRadialGradient(cx,cy,0,cx,cy,22);
  glow.addColorStop(0,'#e8ba7566');glow.addColorStop(1,'#e8ba7500');
  ctx.fillStyle=glow;ctx.fillRect(cx-22,cy-22,44,44);
  ctx.beginPath();ctx.arc(cx,cy,5,0,Math.PI*2);ctx.fillStyle='#f5d19e';ctx.fill();
  ctx.fillStyle='#eac994';ctx.fillText('STAR',cx-34,cy+22);
  const [mx,my]=point(state.mx,state.my);
  ctx.beginPath();ctx.arc(mx,my,6,0,Math.PI*2);ctx.strokeStyle='#d5d1f6';ctx.lineWidth=2;ctx.stroke();
  ctx.fillStyle='#d5d1f6';
  const labelX=Math.max(8,Math.min(w-150,mx+12));
  ctx.fillText(state.mode?'MOON / REFERENCE':'MOON / RENDEZVOUS',labelX,Math.max(35,my-10));
  const [sx,sy]=point(state.x,state.y), rotation=Math.PI/2-state.heading;
  // Velocity is independent of the texture's nose heading. Vector = v * 0.45 T.
  const dx=state.vx*scale*.45,dy=-state.vy*scale*.45,angle=Math.atan2(dy,dx);
  line(sx,sy,sx+dx,sy+dy,'#8ddbe3',1.7);
  line(sx+dx,sy+dy,sx+dx-6*Math.cos(angle-.5),sy+dy-6*Math.sin(angle-.5),'#8ddbe3',1.7);
  line(sx+dx,sy+dy,sx+dx-6*Math.cos(angle+.5),sy+dy-6*Math.sin(angle+.5),'#8ddbe3',1.7);
  ctx.save();ctx.translate(sx,sy);ctx.rotate(rotation);
  if(sprite.complete && sprite.naturalWidth) {
    // Aspect ratio is retained; the sprite is an illustration, not a physical size.
    const size=38,ratio=sprite.naturalWidth/sprite.naturalHeight;
    ctx.drawImage(sprite,-size*ratio/2,-size/2,size*ratio,size);
  } else {
    ctx.strokeStyle='#eee9df';ctx.lineWidth=2;ctx.strokeRect(-4,-4,8,8);
  }
  if(!paused&&!state.mode&&pressed('burn')) {
    line(-3,15,0,25,'#efbd84',3);line(3,15,0,25,'#8ddbe3',1);
  }
  ctx.restore();
  ctx.fillStyle='#eee9df';ctx.fillText('TUG',sx+20,sy-12);
}
function frame(now) {
  if (!lastTime) lastTime=now;
  if(engine && state && !paused && !state.status) {
    accumulator+=Math.min((now-lastTime)/1000,.1)*Number($('rate').value);
    while(accumulator>=.04&&!state.status) {
      const turn=Number(pressed('left'))-Number(pressed('right'));
      engine._flight_tick(1,turn,Number(pressed('burn')));state=read();
      trail.push([state.x,state.y]);if(trail.length>2000)trail.shift();accumulator-=.04;
    }
    if(state.status) pause(true);
  }
  lastTime=now;update();draw();requestAnimationFrame(frame);
}
const keyMap={a:'left',ArrowLeft:'left',d:'right',ArrowRight:'right',w:'burn',ArrowUp:'burn'};
document.addEventListener('keydown',event=>{
  if(!engine || /^(SELECT|INPUT)$/.test(event.target.tagName)) return;
  const key=event.key.length===1?event.key.toLowerCase():event.key;
  if(keyMap[key]&&!state.mode) {event.preventDefault();keys.add(keyMap[key]);reflectControls();}
  if(key==='p'&&!event.repeat&&!state.status) pause(!paused);
});
document.addEventListener('keyup',event=>{
  const key=event.key.length===1?event.key.toLowerCase():event.key;
  keys.delete(keyMap[key]);reflectControls();
});
controlButtons.forEach(button=>{
  button.setAttribute('aria-pressed','false');
  button.addEventListener('pointerdown',event=>{
    if(!engine||state.mode||paused||state.status) return;
    event.preventDefault();button.setPointerCapture(event.pointerId);
    pointers.set(event.pointerId,button.dataset.control);reflectControls();
  });
  ['pointerup','pointercancel','lostpointercapture'].forEach(type=>button.addEventListener(type,event=>{
    pointers.delete(event.pointerId);reflectControls();
  }));
});
window.addEventListener('blur',()=>{if(engine)pause(true)});
document.addEventListener('visibilitychange',()=>{if(document.hidden&&engine)pause(true)});
$('pilot').onclick=()=>reset(0);
$('demo').onclick=()=>reset(1);
$('pause').onclick=()=>{if(!state.status)pause(!paused)};
$('reset').onclick=()=>reset(state.mode,false);
$('rate').onchange=()=>{accumulator=0};
// Read-only observations for automated verification and scientific inspection.
Object.defineProperty(window,'orbitView',{value:Object.freeze({get snapshot(){return state?{...state,paused,artReady:sprite.complete&&sprite.naturalWidth>0}:null}})});
Promise.resolve().then(()=>createOrbit()).then(module=>{
  engine=module;reset(0,false);
  ['pilot','demo','pause','reset'].forEach(id=>$(id).disabled=false);
  requestAnimationFrame(frame);
}).catch(()=>{
  $('status').dataset.state='stop';
  $('status').textContent='Could not load the orbital engine. Reload to retry.';
});
