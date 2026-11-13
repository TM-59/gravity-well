'use strict';
// Only drawing, controls and timing. All states, potentials, roots and contour
// edge intersections come from the same C functions as the terminal output.
const $=id=>document.getElementById(id);
const names=['x','y','vx','vy','time','c','c0','error','mu','level','segments','status','preset'];
let engine,state,paused=true,last=0,elapsed=0,preset=0,level=0,trail=[],history=[],segments=[],mask=[];
const fields=()=>Object.fromEntries(names.map((name,i)=>[name,engine._lab_value(i)]));
function reset(start=false){
  engine._lab_reset(preset,level);state=fields();paused=!start;elapsed=0;
  trail=[[state.x,state.y]];history=[[0,0]];
  segments=Array.from({length:state.segments},(_,i)=>Array.from({length:4},(_,k)=>engine._lab_segment(i,k)));
  mask=Array.from({length:80*80},(_,i)=>engine._lab_allowed(-1.7+(i%80+.5)*3.4/80,-1.7+(Math.floor(i/80)+.5)*3.4/80)<0);
  $('points').replaceChildren();
  for(let i=1;i<=5;i++){
    const row=document.createElement('tr');
    for(const text of [`L${i}`,...[0,1,2].map(k=>engine._lab_point(i,k).toFixed(6))]){
      const cell=document.createElement('td');cell.textContent=text;row.append(cell);
    }
    $('points').append(row);
  }
  update();draw();
}
function update(){
  $('l4').setAttribute('aria-pressed',String(preset===0));$('l1').setAttribute('aria-pressed',String(preset===1));
  $('pause').textContent=state.status?'Stopped':paused?(state.time?'Resume':'Start'):'Pause';
  $('pause').disabled=Boolean(state.status);
  $('experiment').textContent=preset?'Through the L1 gateway.':'A little nudge at L4.';
  $('guidance').textContent=preset?'The traveller starts to the left of L1, moving right. Watch it pass the gateway; the primaries stay fixed in this rotating view.':'The displaced traveller loops near L4. Let it run, then compare the open and closed boundaries around L1.';
  $('run-status').textContent=state.status===1?'Stopped before a close encounter exceeds the accuracy budget. Reset to try again.':state.status===2?'60 T observation complete. Reset to start again.':paused?'Ready / time paused':'Coasting / no thrust';
  $('time').textContent=state.time.toFixed(2);$('c').textContent=state.c.toFixed(9);
  $('boundary-c').textContent=state.level.toFixed(9);$('max-error').textContent=state.error.toExponential(2);$('mu').textContent=state.mu.toFixed(9);
  $('error').textContent=`max |ΔC| ${state.error.toExponential(2)}`;
  $('comparison').textContent=level?'Comparison only: grey regions use the selected boundary C. The amber traveller keeps its own C, shown in the data panel.':'The boundary uses the traveller’s initial C; it cannot enter the grey regions while C is conserved.';
}
function context(id){
  const canvas=$(id),box=canvas.getBoundingClientRect(),dpr=Math.min(devicePixelRatio||1,2);
  canvas.width=Math.round(box.width*dpr);canvas.height=Math.round(box.height*dpr);
  const ctx=canvas.getContext('2d');ctx.setTransform(dpr,0,0,dpr,0,0);ctx.font='14px ui-monospace,monospace';
  return {ctx,w:box.width,h:box.height};
}
function draw(){
  if(!state)return;
  const {ctx,w,h}=context('map'),scale=Math.min(w-48,h-48)/3.4;
  const point=(x,y)=>[w/2+x*scale,h/2-y*scale];
  for(let i=0;i<mask.length;i++)if(mask[i]){
    const [x,y]=point(-1.7+(i%80)*3.4/80,-1.7+(Math.floor(i/80)+1)*3.4/80);
    ctx.fillStyle='#626b874c';ctx.fillRect(x,y,3.4/80*scale+1,3.4/80*scale+1);
  }
  ctx.strokeStyle='#354b5b';ctx.lineWidth=1;ctx.beginPath();ctx.moveTo(12,h/2);ctx.lineTo(w-12,h/2);ctx.moveTo(w/2,12);ctx.lineTo(w/2,h-12);ctx.stroke();
  ctx.fillStyle='#b3c1cc';ctx.fillText('x / L',w-52,h/2+18);ctx.fillText('y / L',w/2+8,18);
  ctx.strokeStyle='#8ddbe3';ctx.beginPath();
  for(const [x1,y1,x2,y2]of segments){ctx.moveTo(...point(x1,y1));ctx.lineTo(...point(x2,y2));}ctx.stroke();
  ctx.strokeStyle='#efbd84';ctx.lineWidth=2;ctx.beginPath();
  trail.forEach(([x,y],i)=>{if(i)ctx.lineTo(...point(x,y));else ctx.moveTo(...point(x,y));});ctx.stroke();
  for(let i=1;i<=5;i++){
    const [x,y]=point(engine._lab_point(i,0),engine._lab_point(i,1));
    ctx.strokeStyle='#eee9df';ctx.lineWidth=1;ctx.beginPath();ctx.moveTo(x-4,y);ctx.lineTo(x+4,y);ctx.moveTo(x,y-4);ctx.lineTo(x,y+4);ctx.stroke();
    ctx.fillStyle='#eee9df';ctx.fillText(`L${i}`,x+5,y-8);
  }
  for(const [x,r,color]of [[-state.mu,7,'#efbd84'],[1-state.mu,5,'#b5a2ee']]){
    const p=point(x,0);ctx.fillStyle=color;ctx.beginPath();ctx.arc(...p,r,0,2*Math.PI);ctx.fill();
  }
  ctx.fillStyle='#efbd84';ctx.beginPath();ctx.arc(...point(state.x,state.y),4,0,2*Math.PI);ctx.fill();
  const chart=context('jacobi'),c=chart.ctx,W=chart.w,H=chart.h;
  const xmax=Math.max(1,state.time),yrange=Math.max(1e-14,state.error*1.2);
  c.fillStyle='#b3c1cc';c.fillText(`ΔC / (L²/T²) · ±${yrange.toExponential(1)}`,12,20);
  c.fillText('0',12,H/2+4);c.fillText(`t: 0 → ${state.time.toFixed(2)} T`,12,H-10);
  c.strokeStyle='#354b5b';c.beginPath();c.moveTo(40,H/2);c.lineTo(W-12,H/2);c.stroke();
  c.strokeStyle='#8ddbe3';c.beginPath();
  history.forEach(([t,d],i)=>{const x=40+t/xmax*(W-56),y=H/2-d/yrange*(H/2-32);if(i)c.lineTo(x,y);else c.moveTo(x,y);});c.stroke();
}
function frame(now){
  if(engine&&!paused&&!state.status){
    elapsed+=Math.min((now-last)/1000,.1);
    while(elapsed>=.04&&!state.status){
      engine._lab_tick(1);state=fields();elapsed-=.04;
      trail.push([state.x,state.y]);history.push([state.time,state.c-state.c0]);
    }
    if(state.status)paused=true;
    update();draw();
  }
  last=now;requestAnimationFrame(frame);
}
function pause(){if(!engine)return;paused=true;elapsed=0;update();}
$('l4').onclick=()=>{preset=0;reset(true);};$('l1').onclick=()=>{preset=1;reset(true);};
$('pause').onclick=()=>{paused=!paused;elapsed=0;update();};$('reset').onclick=()=>reset(false);
$('level').onchange=()=>{level=Number($('level').value);reset(false);};
document.addEventListener('visibilitychange',()=>{if(document.hidden)pause();});
window.addEventListener('blur',pause);window.addEventListener('resize',draw);
Object.defineProperty(window,'lagrangeView',{value:Object.freeze({get snapshot(){return state?{...state,paused,levelIndex:level,points:Array.from({length:5},(_,i)=>[engine._lab_point(i+1,0),engine._lab_point(i+1,1)]),trailLength:trail.length}:null;}})});
Promise.resolve().then(()=>createLagrange()).then(module=>{
  engine=module;reset();for(const id of ['l4','l1','pause','reset','level'])$(id).disabled=false;
  requestAnimationFrame(frame);
}).catch(()=>{$('run-status').textContent='Could not load the C engine. Reload to retry.';});
