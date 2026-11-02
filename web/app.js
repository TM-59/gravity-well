'use strict';
/* Only presentation lives here. Every state value and integration step is C. */
const $ = id => document.getElementById(id);
const fieldNames = ['x','y','vx','vy','time','heading','radius','speed','energy','mx','my',
  'distance','relative','status','phase','dv','mode','burn1','burn2','end'];
// The Ice Run (src/voyage.c): same order as enum VoyageValue.
const voyageFields = ['x','y','vx','vy','time','heading','radius','speed','fuel','red','leg','scene','banner','tows',
  'from','target','apo','peri','apoRead','periRead','matchError','route','turned','count','wobble','ending','loud',
  'heavy','command','a','e','periAngle','radial','mx','my','redLine','held','holdNeed','boil','drift','cue'];
const canvas = $('map'), ctx = canvas.getContext('2d');
const sprite = new Image();
sprite.src = 'art/tug-sprite.png';
let engine, state, paused = true, accumulator = 0, lastTime = 0, trail = [], voyage = false, shownScene = -2;
const keys = new Set(), pointers = new Map(), heavyLog = [];
const controlButtons = [...document.querySelectorAll('[data-control]')];
const voyageButtons = [...document.querySelectorAll('[data-voyage]')];
const read = () => voyage
  ? Object.fromEntries(voyageFields.map((name, i) => [name, engine._vw_value(i)]))
  : Object.fromEntries(fieldNames.map((name, i) => [name, engine._flight_value(i)]));
const pressed = name => keys.has(name) || [...pointers.values()].includes(name);

// ------------------------------------------------------------------ The Ice Run: words
// Scenes open where the C voyage says (src/voyage.c); the words live here. Index = VS_* in src/voyage.h.
const SCENES = [
  {id: 'dock', title: 'Inner Dock, last shift', lines: [
    ['Ilse', "That's the last of the ice in your hold. Lamp Quay drinks it or nobody does."],
    ['Ilse', "I've put in what the dock can spare. It isn't much. Two burns, not three. Every extra burn is water you didn't deliver."],
    ['Ilse', "The reserve pod stays here with me. Somebody keeps the pumps turning until you're clear."],
    ['Ilse', "I'll call your burns while you're in range. Go on, Wren. The ring at one and a half."]],
    note: "Hold RAISE until the dashed orbit touches the green ring, coast to it, then hold MATCH until the orbit is round.",
    options: ['Cast off']},
  {id: 'route', title: 'Two buoys still lit', lines: [
    ['Pell', "Wren, Lamp Quay. Pell on the board. Our tanks dropped below the marks this morning. The kids are on half cups."],
    ['Pell', "Two buoys are still lit on your way out. Lark is close, but the evacuation ships drained it. Wick is way up the well, and its keeper says he'll fire his spare fuel down to you if you come his way."],
    ['Ilse', "Close is quick. Far is fuel. Pick one and stop looking at the other."]],
    options: ['Make for Lark, the near buoy (short climb)', 'Climb to Wick, the far buoy (long climb, fuel sent down)']},
  {id: 'beacon', title: 'A lamp on the Quay', lines: [
    ['Pell', "I see you! You're on the board, Wren. A real blip, not a guess!"],
    ['Pell', "I'm lighting the first lamp on the Quay. One for every tank that comes home. Two more hops and yours is the second."],
    ['Ilse', "Take what's in the buoy's cache. Don't thank it."]],
    options: ['Take the cache aboard']},
  {id: 'flare', title: 'Three weeks early', lines: [
    ['Ilse', 'Wren. Look sunward.'],
    ['Pell', "That isn't a sunrise. They said three weeks. They told us three weeks!"],
    ['Ilse', "Shouting won't cool it. Drop down to the ring at one point eight while the front is still coming. When it reaches us your tank will boil off and the readout will jump. Don't trust a number that won't sit still."]],
    options: ['Drop below the front']},
  {id: 'ilse', title: "The dock's last call", heavy: true, lines: [
    ['Ilse', "Wren. The dock is inside the flare. The hull is past the red line and climbing."],
    ['Ilse', "No. I'm not coming. The pumps need a hand on them or the reserve boils away, and I'm the hand."],
    ['Ilse', "I've sent the reserve pod down to the ring at one point six. Fetch it or don't. You'll be later to the Quay if you do, and I'll be on this channel a little longer. That's all it buys you."],
    ['Pell', "Ilse, there's room here! There's room, we can make room, we can—"],
    ['Ilse', 'Pell. Count the cups.']],
    options: ['Turn back for her pod (a detour, then a wait while it pumps)', 'Hold the heading for Lamp Quay']},
  {id: 'silence', title: 'Static', heavy: true, lines: [
    ['Ilse', "Pod's in your hands. Two burns now. Not three. It's warm in here, Wren. Tell Pell to light it anyway."],
    ['', 'The channel hisses. Nobody calls the next burn.'],
    ['You', 'Two burns. Not three.']],
    heldLines: [
    ['Ilse', "Wren, the pod's gone past you, never mind it. Two burns. Not th—"],
    ['', 'The channel hisses. Nobody calls the next burn.'],
    ['You', 'Two burns. Not three.']],
    note: 'From here, nobody steadies your readout. If the tank runs dry, only Lamp Quay can reach you, and its tug drags you back to the start of the leg.',
    options: ['Fly on']},
  {id: 'quay', title: 'Lamp Quay', endings: [
    ["Lamp Quay takes your hold of ice. Pell meets you at the lock with a lamp in each hand.",
     "\"One for the tank,\" Pell says, \"and one for the dock.\" The second lamp goes in the window that faces the star.",
     "Ilse's empty pod is still clamped to your hull. You can repeat every word she said on the way down to it."],
    ["Lamp Quay takes your hold of ice. You came in early, and the kids get full cups tonight.",
     "Pell lights the second lamp and doesn't say anything for a long time.",
     "Your log has her last call cut off in the middle of a word, somewhere behind you in the static. You count your own burns now."]],
    options: ['Start again']}
];

// ------------------------------------------------------------------ shared controls
function clearControls() {
  keys.clear(); pointers.clear();
  [...controlButtons, ...voyageButtons].forEach(button => button.setAttribute('aria-pressed', 'false'));
}
function reflectControls() {
  controlButtons.forEach(button => button.setAttribute('aria-pressed', String(pressed(button.dataset.control))));
  voyageButtons.forEach(button => button.setAttribute('aria-pressed', String(pressed(button.dataset.voyage))));
}
const finished = () => voyage ? state && state.leg >= 5 : Boolean(state?.status);
function pause(value) {
  paused = value; accumulator = 0; clearControls();
  $('pause').textContent = finished() ? 'Complete' : paused ? 'Resume' : 'Pause';
  $('pause').disabled = finished();
  $('pause').setAttribute('aria-pressed', String(paused));
  update();
}
function setMode(isVoyage) {
  voyage = isVoyage;
  document.body.dataset.mode = voyage ? 'voyage' : 'flight';
  $('flight-data').hidden = voyage; $('voyage-data').hidden = !voyage;
  $('flight-controls').hidden = voyage; $('voyage-controls').hidden = !voyage;
  $('legend-moon').textContent = voyage ? 'Moon (reference, no pull)' : 'Moon';
}
function reset(mode, start = true) {
  setMode(mode === 2);
  if (voyage) engine._vw_reset(); else engine._flight_reset(mode);
  state = read(); trail = [[state.x, state.y]]; shownScene = -2; heavyLog.length = 0;
  $('rate').value = mode === 1 ? '4' : '1';
  controlButtons.forEach(button => button.disabled = mode === 1);
  pause(!start); showScene(); draw();
}

// ------------------------------------------------------------------ scenes
function showScene() {
  const box = $('scene');
  const index = voyage && state ? state.scene : -1;
  if (index === shownScene) return;
  shownScene = index;
  if (index < 0) { box.hidden = true; return; }
  const s = SCENES[index];
  clearControls();
  box.hidden = false;
  box.className = s.heavy ? 'heavy' : s.endings ? 'ending' : '';
  box.dataset.scene = s.id;
  if (s.heavy) heavyLog.push({id: s.id, banner: state.banner, loud: state.loud, bannerShown: !$('banner').hidden});
  $('scene-title').textContent = s.title;
  const text = $('scene-text');
  text.replaceChildren();
  const lines = s.endings ? s.endings[state.ending === 0 ? 0 : 1].map(line => ['', line])
    : s.heldLines && state.turned === 1 ? s.heldLines : s.lines;
  for (const [who, words] of lines) {
    const p = document.createElement('p');
    if (who) { const b = document.createElement('b'); b.textContent = who; p.append(b, ' '); }
    else p.className = 'narration';
    p.append(words);
    text.append(p);
  }
  if (s.note) { const p = document.createElement('p'); p.className = 'note'; p.textContent = s.note; text.append(p); }
  const options = $('scene-options');
  options.replaceChildren();
  s.options.forEach((label, option) => {
    const button = document.createElement('button');
    button.textContent = label;
    if (!option) button.className = 'primary';
    button.onclick = () => answer(option);
    options.append(button);
  });
  $('banner').hidden = true;
  box.scrollTop = 0;
  options.firstChild.focus({preventScroll: true});
}
function answer(option) {
  if (!voyage || state.scene < 0) return;
  if (state.scene === 6) { reset(2, false); return; }
  engine._vw_answer(option); state = read();
  showScene(); pause(state.scene >= 0); update(); draw();
}

// ------------------------------------------------------------------ readouts
function fmt(id, value, decimals) { $(id).textContent = value.toFixed(decimals).replace('-', '−'); }
function update() {
  if (!state) return;
  if (voyage) return updateVoyage();
  for (const [id, value, decimals] of [
    ['radius',state.radius,3],['speed',state.speed,3],['energy',state.energy,4],
    ['distance',state.distance,3],['relative',state.relative,3],['elapsed',state.time,2],['delta-v',state.dv,4]]) fmt(id, value, decimals);
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
  $('banner').hidden = true;
}
const LEG_NAMES = ['LEG 1 / 5 · LEAVING THE DOCK', 'LEG 2 / 5 · THE CLIMB TO THE BUOY', 'LEG 3 / 5 · BELOW THE FLARE FRONT',
  'LEG 4 / 5 · THE FLARE', 'LEG 5 / 5 · THE LAST LEG'];
function updateVoyage() {
  const leg = Math.min(state.leg, 4), red = state.fuel < state.redLine;
  fmt('v-fuel', state.fuel, 3); fmt('v-red', state.red, 1); $('v-tows').textContent = String(state.tows);
  fmt('v-target', state.target, 2); fmt('v-apo', state.apoRead, 3); fmt('v-peri', state.periRead, 3);
  fmt('v-clock', state.time, 1); fmt('v-match', state.matchError, 3);
  $('fuel-row').dataset.red = String(red);
  $('fuel-bar').style.width = `${Math.max(0, Math.min(100, state.fuel / 0.8 * 100))}%`;
  $('mode-label').textContent = 'THE ICE RUN';
  $('mission-title').textContent = state.leg >= 5 ? 'Lamp Quay.' : 'Carry the ice out.';
  $('mission-copy').textContent = 'Five legs from the inner dock to Lamp Quay, around the star only: the moon is drawn as a reference and does not pull. ' +
    'Fuel is the ice you were meant to deliver. Two burns per leg is the rule.';
  $('step-label').textContent = state.leg >= 5 ? 'ARRIVED' : LEG_NAMES[leg];
  const pod = state.holdNeed > 0;
  $('guidance').textContent = state.leg >= 5 ? 'The run is over.'
    : state.drift > 0 ? 'Engine dead. A drone from the dock is on its way with fuel.'
    : pod && state.held > 0 ? `Grappled. The pod is pumping across: ${state.held.toFixed(1)} of ${state.holdNeed.toFixed(0)} T.`
    : pod ? "Round off on the pod's ring, then cut the engine. It only reaches for a quiet tug."
    : 'Raise or Lower until the dashed orbit touches the green ring, coast to the ring, then Match.';
  const burning = !paused && state.command > 0 && state.drift <= 0;
  $('status').dataset.state = state.leg >= 5 ? 'stop' : burning ? 'burn' : red ? 'stop' : 'coast';
  const statusText = state.leg >= 5 ? 'RUN COMPLETE' : state.scene >= 0 ? 'ON THE CHANNEL'
    : paused ? 'PAUSED / READY TO FLY' : state.drift > 0 ? 'TANK DRY / DRIFTING'
    : burning ? ['', 'RAISE / PROGRADE BURN', 'LOWER / RETROGRADE BURN', 'MATCH / ROUNDING THE ORBIT'][state.command]
    : red ? 'COASTING / TANK IN THE RED' : 'COASTING';
  if ($('status').textContent !== statusText) $('status').textContent = statusText;
  // The C voyage clears the banner when a heavy scene opens; a scene on screen always hides it.
  const banner = $('banner');
  banner.hidden = !state.banner || state.scene >= 0;
  banner.textContent = state.banner === 1 ? 'LEG CLEARED' : state.banner === 3 ? 'GRAPPLE MISSED / THE POD COMES ROUND AGAIN'
    : state.count ? 'TANK DRY / DRONE ON ITS WAY' : 'TANK DRY / TOWED BACK BY LAMP QUAY';
  banner.dataset.kind = state.banner === 1 ? 'clear' : 'fail';
}

// ------------------------------------------------------------------ drawing
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
  const reach = voyage ? Math.max(state.radius, state.target, Math.min(state.apoRead, 4)) : state.radius;
  const extent = Math.max(3.7, Math.ceil(reach * 1.15));
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
  if (voyage && state.leg < 5) drawVoyageRings(cx, cy, scale, point);
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
  ctx.fillText(voyage||state.mode?'MOON / REFERENCE':'MOON / RENDEZVOUS',labelX,Math.max(35,my-10));
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
  const flame = voyage ? !paused && state.command > 0 && state.drift <= 0 && state.fuel > 0 : !paused&&!state.mode&&pressed('burn');
  if(flame) {
    line(-3,15,0,25,'#efbd84',3);line(3,15,0,25,'#8ddbe3',1);
  }
  ctx.restore();
  ctx.fillStyle='#eee9df';ctx.fillText('TUG',sx+20,sy-12);
}
function drawVoyageRings(cx, cy, scale, point) {
  // The target ring, and the orbit the gauges read (apoapsis/periapsis readouts, wobbling when nobody steadies them).
  ctx.setLineDash([7,5]);ctx.strokeStyle='#9fe0a8';ctx.lineWidth=1.6;
  ctx.beginPath();ctx.arc(cx,cy,state.target*scale,0,Math.PI*2);ctx.stroke();ctx.setLineDash([]);
  ctx.fillStyle='#9fe0a8';
  const label = state.holdNeed > 0 ? "ILSE'S POD" : state.leg === 4 ? 'LAMP QUAY' : state.leg === 1 ? 'BUOY' : 'TARGET';
  ctx.fillText(`${label} r = ${state.target.toFixed(2)}`,cx-state.target*scale*.707-4,cy+state.target*scale*.707+14);
  const apo = state.apoRead, peri = Math.max(0.05, state.periRead);
  if (!(apo > 0 && apo < 50)) return;
  const a = (apo + peri) / 2, e = (apo - peri) / (apo + peri), p = a * (1 - e * e), w0 = state.periAngle;
  ctx.beginPath();
  for (let k = 0; k <= 160; k++) {
    const th = k / 160 * Math.PI * 2, r = p / (1 + e * Math.cos(th - w0));
    const [x, y] = point(r * Math.cos(th), r * Math.sin(th));
    if (k) ctx.lineTo(x, y); else ctx.moveTo(x, y);
  }
  ctx.setLineDash([3,4]);ctx.strokeStyle='#8ddbe3aa';ctx.lineWidth=1.2;ctx.stroke();ctx.setLineDash([]);
}

// ------------------------------------------------------------------ loop and input
function frame(now) {
  if (!lastTime) lastTime=now;
  if(engine && state && !paused && !finished() && !(voyage && state.scene >= 0)) {
    accumulator+=Math.min((now-lastTime)/1000,.1)*Number($('rate').value);
    while(accumulator>=.04&&!finished()&&!(voyage && state.scene >= 0)) {
      if (voyage) {
        const command = pressed('raise') ? 1 : pressed('lower') ? 2 : pressed('match') ? 3 : 0;
        const tows = state.tows;
        engine._vw_tick(1, command); state = read();
        if (state.tows !== tows) trail.length = 0;
      } else {
        const turn=Number(pressed('left'))-Number(pressed('right'));
        engine._flight_tick(1,turn,Number(pressed('burn')));state=read();
      }
      trail.push([state.x,state.y]);if(trail.length>2000)trail.shift();accumulator-=.04;
    }
    if (voyage && state.scene >= 0) { showScene(); pause(true); }
    else if(finished()) pause(true);
  }
  lastTime=now;update();draw();requestAnimationFrame(frame);
}
const keyMap={a:'left',ArrowLeft:'left',d:'right',ArrowRight:'right',w:'burn',ArrowUp:'burn'};
const voyageKeys={w:'raise',ArrowUp:'raise',s:'lower',ArrowDown:'lower',m:'match',' ':'match'};
document.addEventListener('keydown',event=>{
  if(!engine || /^(SELECT|INPUT|BUTTON)$/.test(event.target.tagName) && event.key===' ') return;
  if(!engine || /^(SELECT|INPUT)$/.test(event.target.tagName)) return;
  const key=event.key.length===1?event.key.toLowerCase():event.key;
  if (voyage) {
    if (state.scene >= 0) return;
    if (voyageKeys[key]) { event.preventDefault(); keys.add(voyageKeys[key]); reflectControls(); }
  } else if(keyMap[key]&&!state.mode) {event.preventDefault();keys.add(keyMap[key]);reflectControls();}
  if(key==='p'&&!event.repeat&&!finished()&&!(voyage&&state.scene>=0)) pause(!paused);
});
document.addEventListener('keyup',event=>{
  const key=event.key.length===1?event.key.toLowerCase():event.key;
  keys.delete(keyMap[key]);keys.delete(voyageKeys[key]);reflectControls();
});
function holdButton(button, name) {
  button.setAttribute('aria-pressed','false');
  button.addEventListener('pointerdown',event=>{
    if(!engine||paused||finished()||(voyage?state.scene>=0:state.mode)) return;
    event.preventDefault();button.setPointerCapture(event.pointerId);
    pointers.set(event.pointerId,name);reflectControls();
  });
  ['pointerup','pointercancel','lostpointercapture'].forEach(type=>button.addEventListener(type,event=>{
    pointers.delete(event.pointerId);reflectControls();
  }));
}
controlButtons.forEach(button=>holdButton(button, button.dataset.control));
voyageButtons.forEach(button=>holdButton(button, button.dataset.voyage));
window.addEventListener('blur',()=>{if(engine)pause(true)});
document.addEventListener('visibilitychange',()=>{if(document.hidden&&engine)pause(true)});
$('voyage').onclick=()=>reset(2);
$('pilot').onclick=()=>reset(0);
$('demo').onclick=()=>reset(1);
$('pause').onclick=()=>{if(!finished()&&!(voyage&&state.scene>=0))pause(!paused)};
$('reset').onclick=()=>reset(voyage?2:state.mode,false);
$('voyage-reset').onclick=()=>reset(2,false);
$('rate').onchange=()=>{accumulator=0};
// Read-only observations for automated verification and scientific inspection.
Object.defineProperty(window,'orbitView',{value:Object.freeze({get snapshot(){
  if (!state) return null;
  const scene = voyage && state.scene >= 0 ? SCENES[state.scene].id : null;
  return {...state,paused,voyage,scene,artReady:sprite.complete&&sprite.naturalWidth>0,
    bannerShown:!$('banner').hidden,bannerText:$('banner').textContent,heavyLog:heavyLog.map(h=>({...h})),
    sceneText:$('scene').hidden?'':$('scene-text').textContent};
}})});
Promise.resolve().then(()=>createOrbit()).then(module=>{
  engine=module;reset(0,false);
  ['voyage','pilot','demo','pause','reset','voyage-reset'].forEach(id=>$(id).disabled=false);
  requestAnimationFrame(frame);
}).catch(()=>{
  $('status').dataset.state='stop';
  $('status').textContent='Could not load the orbital engine. Reload to retry.';
});
