/* Local browser and native/WASM integration checks; no downloads or services. */
const {env}=process;
const {chromium}=require(env.PLAYWRIGHT_MODULE||'playwright');
const assert=require('node:assert/strict');
const fs=require('node:fs'),http=require('node:http'),path=require('node:path');
const {execFileSync}=require('node:child_process');
const create=require('../dist/cr3bp.js');
const root=path.resolve('dist');
const mime={'.html':'text/html','.js':'text/javascript','.wasm':'application/wasm','.css':'text/css'};
const server=http.createServer((req,res)=>{
  const file=path.resolve(root,'.'+new URL(req.url,'http://localhost').pathname);
  const target=file===root?path.join(root,'index.html'):file;
  if(!target.startsWith(root+path.sep)){res.writeHead(403).end();return;}
  fs.readFile(target,(e,data)=>{if(e)res.writeHead(404).end();else{res.setHeader('Content-Type',mime[path.extname(target)]||'application/octet-stream');res.end(data);}});
});
let browser,checks=0;
const check=(condition,message)=>{assert.ok(condition,message);checks++;};
const snap=page=>page.evaluate(()=>window.lagrangeView.snapshot);
async function run(){
  fs.mkdirSync('test-results',{recursive:true});
  const wasm=await create({wasmBinary:fs.readFileSync('dist/cr3bp.wasm')});
  const native=execFileSync('./test_cr3bp',['--samples'],{encoding:'utf8'}).trim().split('\n').map(row=>row.split(',').map(Number));
  let row=0,maxDifference=0;
  for(let p=0;p<2;p++){
    wasm._lab_reset(p,0);
    for(let i=0;i<50;i++){
      for(let f=0;f<13;f++){
        const a=wasm._lab_value(f),b=native[row][f],error=Math.abs(a-b)/Math.max(1,Math.abs(b));
        maxDifference=Math.max(maxDifference,error);check(error<1e-10,`native/WASM row ${row} field ${f}`);
      }
      row++;wasm._lab_tick(1);
    }
  }
  await new Promise(resolve=>server.listen(0,'localhost',resolve));
  const origin=`http://localhost:${server.address().port}`;
  browser=await chromium.launch({headless:true,args:['--no-sandbox']});
  const errors=[],external=[],screens=[];
  for(const [width,height]of [[1440,1000],[390,844],[844,390]]){
    const context=await browser.newContext({viewport:{width,height},hasTouch:width!==1440});
    const page=await context.newPage();
    const activate=locator=>width===1440?locator.click():locator.tap();
    page.on('pageerror',e=>errors.push(e.message));
    page.on('console',m=>{if(m.type()==='error')errors.push(m.text());});
    page.on('request',r=>{if(!r.url().startsWith(origin)&&!r.url().startsWith('data:'))external.push(r.url());});
    await page.addInitScript(()=>{
      window.canvasFonts=[];
      const fill=CanvasRenderingContext2D.prototype.fillText;
      CanvasRenderingContext2D.prototype.fillText=function(...args){window.canvasFonts.push(parseFloat(this.font));return fill.apply(this,args);};
    });
    await page.goto(origin+'/');
    await activate(page.getByRole('link',{name:'Explore Lagrange points'}));
    await page.waitForFunction(()=>window.lagrangeView?.snapshot);
    await page.clock.install();
    check((await snap(page)).paused,'starts paused');
    check(await page.locator('#points tr').count()===5,'five points in the real DOM');
    await activate(page.getByRole('button',{name:'L4 libration',exact:true}));
    await page.clock.runFor(1500);
    const moved=await snap(page);
    check(moved.time>1 && moved.error<1e-6,'L4 trajectory advances and conserves C');
    await activate(page.getByRole('button',{name:'Pause',exact:true}));
    const stopped=await snap(page);await page.clock.runFor(400);
    check((await snap(page)).time===stopped.time,'pause freezes C time');
    await page.selectOption('#level','1');
    const closed=await snap(page);
    check(closed.level>closed.c && closed.paused&&closed.time===0,'closed comparison resets and is labelled separately');
    check((await page.locator('#comparison').textContent()).startsWith('Comparison only'),'comparison is not claimed to be the particle C');
    await page.selectOption('#level','2');
    const open=await snap(page);
    check(open.level<closed.level && open.segments!==closed.segments,'open comparison changes the C-generated contour');
    await page.selectOption('#level','0');
    await activate(page.getByRole('button',{name:'L1 passage',exact:true}));
    await page.clock.runFor(1000);
    const transit=await snap(page);
    check(transit.x>transit.points[0][0]&&transit.error<1e-6,'actual L1 transit through UI');
    await activate(page.getByRole('button',{name:'Reset',exact:true}));
    const reset=await snap(page);
    check(reset.time===0&&reset.error===0&&reset.paused,'reset is clean');
    await activate(page.getByRole('button',{name:'L4 libration',exact:true}));
    await page.clock.runFor(400);
    await page.evaluate(()=>window.dispatchEvent(new Event('blur')));
    const blurred=await snap(page);await page.clock.runFor(400);
    check(blurred.paused&&(await snap(page)).time===blurred.time,'blur pauses without stale time');
    check(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'no horizontal overflow');
    const metrics=await page.evaluate(()=>{
      const els=[...document.querySelectorAll('body *')].filter(e=>e.getClientRects().length&&[...e.childNodes].some(n=>n.nodeType===Node.TEXT_NODE&&n.textContent.trim()));
      const fonts=els.map(e=>parseFloat(getComputedStyle(e).fontSize));
      const targets=[...document.querySelectorAll('button,select,a')].map(e=>{const r=e.getBoundingClientRect();return[r.width,r.height];});
      return {minFont:Math.min(...fonts,...window.canvasFonts),minTarget:Math.min(...targets.flat())};
    });
    check(metrics.minFont>=14,'all visible DOM and canvas text >=14 CSS px');
    check(metrics.minTarget>=44,'all buttons, links and select >=44 CSS px');
    check(await page.locator('.table-wrap').evaluate(el=>el.scrollWidth<=el.clientWidth),'point coordinates fit without clipping');
    await activate(page.getByRole('button',{name:'L1 passage',exact:true}));
    await page.clock.runFor(800);
    await activate(page.getByRole('button',{name:'Pause',exact:true}));
    const shot=`docs/images/lagrange-${width}x${height}.png`;
    await page.screenshot({path:shot,fullPage:true});screens.push({path:shot,...metrics});
    await activate(page.getByRole('link',{name:'Back to free flight and The Ice Run'}));
    await page.waitForFunction(()=>window.orbitView?.snapshot);
    check(await page.locator('#welcome').isVisible(),'return to original observatory');
    await context.close();
  }
  // An unavailable WASM must produce a readable recovery message, not an empty lab.
  const failure=await browser.newPage();
  await failure.route('**/cr3bp.wasm',r=>r.fulfill({status:503,body:'unavailable'}));
  await failure.goto(origin+'/lagrange.html');
  await failure.getByRole('status').filter({hasText:'Could not load the C engine'}).waitFor();
  check(await failure.locator('#l4').isDisabled(),'engine failure keeps controls disabled');
  await failure.close();
  check(errors.length===0,`browser errors: ${errors.join(';')}`);check(external.length===0,'no external requests');
  const result={checks,errors:errors.length,externalRequests:external.length,nativeWasm:{rows:100,columns:13,maxRelativeDifference:maxDifference},screens};
  fs.writeFileSync('test-results/lagrange-smoke.json',JSON.stringify(result,null,2)+'\n');
  console.log(JSON.stringify(result,null,2));
}
run().catch(e=>{console.error(e);process.exitCode=1;}).finally(async()=>{if(browser)await browser.close();server.close();});
