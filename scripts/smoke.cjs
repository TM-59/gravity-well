/* Local only: exercise the real WASM with keyboard/pointer input and capture it. */
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const root = path.resolve('dist');
const mime = {'.html':'text/html','.js':'text/javascript','.wasm':'application/wasm',
  '.css':'text/css','.png':'image/png','.jpg':'image/jpeg'};
const server = http.createServer((req,res)=>{
  const file = path.resolve(root,'.'+decodeURIComponent(new URL(req.url,'http://localhost').pathname));
  const target = file===root?path.join(root,'index.html'):file;
  if(!target.startsWith(root+path.sep)) {res.writeHead(403).end();return;}
  fs.readFile(target,(error,data)=>{
    if(error)res.writeHead(404).end();
    else {res.setHeader('Content-Type',mime[path.extname(target)]||'application/octet-stream');res.end(data);}
  });
});
let browser, checks=0;
const check=(condition,message)=>{assert.ok(condition,message);checks++};
const observe=page=>page.evaluate(()=>window.orbitView.snapshot);
async function ready(page,url) {
  await page.goto(url);
  await page.waitForFunction(()=>window.orbitView?.snapshot?.artReady);
  await page.clock.install();
}
async function capture(page,name){await page.screenshot({path:`docs/images/${name}.jpg`,type:'jpeg',quality:88,fullPage:true});}
async function run(){
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const origin=`http://127.0.0.1:${server.address().port}`;
  browser=await chromium.launch({headless:true,args:['--no-sandbox']});
  const errors=[],external=[];
  const context=await browser.newContext({viewport:{width:1440,height:1000},deviceScaleFactor:1});
  context.on('page',page=>{
    page.on('pageerror',error=>errors.push(error.message));
    page.on('console',message=>{if(message.type()==='error')errors.push(message.text())});
    page.on('request',req=>{if(!req.url().startsWith(origin)&&!req.url().startsWith('data:'))external.push(req.url())});
  });
  const page=await context.newPage();
  await ready(page,origin+'/');
  check((await observe(page)).paused,'starts paused');
  await capture(page,'observatory');
  await page.getByRole('button',{name:'Take control'}).click();
  await page.keyboard.down('a');await page.clock.runFor(400);await page.keyboard.up('a');
  const turned=await observe(page);
  check(Math.abs(turned.heading-Math.atan2(turned.vy,turned.vx))>.3,'heading is independent of velocity');
  await page.keyboard.down('w');await page.clock.runFor(1200);await page.keyboard.up('w');
  const manual=await observe(page);
  check(manual.dv>.02,'keyboard thrust reaches the C engine');
  check(await page.getByLabel('Hold to burn').getAttribute('aria-pressed')==='false','key release clears thrust');
  await page.keyboard.press('p');
  const paused=await observe(page);await page.clock.runFor(500);
  check((await observe(page)).time===paused.time,'P freezes time');
  await capture(page,'manual-flight');
  await page.getByRole('button',{name:'Watch a Hohmann transfer'}).click();
  await page.clock.runFor(1100);
  check((await observe(page)).phase===1,'first impulse reached through real UI');
  await capture(page,'transfer');
  await page.clock.runFor(5000);
  const final=await observe(page);
  check(final.status===3,'demo completes');
  check(Math.abs(final.radius-2)<.002,'WASM transfer reaches target radius');
  check(final.dv>.28&&final.dv<.29,'two impulses applied');
  await capture(page,'circularized');
  await page.getByRole('button',{name:'Reset',exact:true}).click();
  const reset=await observe(page);
  check(reset.time===0&&reset.dv===0&&reset.paused,'reset clears demo and pauses');
  check(await page.locator('body').evaluate(el=>el.scrollWidth<=innerWidth),'desktop has no overflow');
  // Narrow viewport + touch-generated Pointer Events. A cancelled touch must not stick.
  const mobile=await context.newPage();
  await mobile.setViewportSize({width:390,height:844});
  await ready(mobile,origin+'/');
  await mobile.getByRole('button',{name:'Take control'}).click();
  const button=mobile.getByLabel('Hold to burn');
  await button.scrollIntoViewIfNeeded();const box=await button.boundingBox();
  const client=await context.newCDPSession(mobile);
  await client.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x:box.x+box.width/2,y:box.y+box.height/2}]});
  await mobile.clock.runFor(600);
  check((await observe(mobile)).dv>.005,'touch thrust reaches the C engine');
  await client.send('Input.dispatchTouchEvent',{type:'touchCancel',touchPoints:[]});
  check(await button.getAttribute('aria-pressed')==='false','cancelled touch clears thrust');
  const dv=(await observe(mobile)).dv;await mobile.clock.runFor(300);
  check((await observe(mobile)).dv===dv,'no thrust after touch cancellation');
  await mobile.getByRole('button',{name:'Pause',exact:true}).click();
  check(await mobile.locator('body').evaluate(el=>el.scrollWidth<=innerWidth),'mobile has no horizontal overflow');
  for(const el of await mobile.locator('.controls button').all()) {
    const rect=await el.boundingBox();check(rect.width>=44&&rect.height>=44,'touch target at least 44 px');
  }
  await mobile.evaluate(()=>scrollTo(0,0));await capture(mobile,'mobile');
  await mobile.close();
  // Artwork is optional for simulation. Block both assets; then engine failures are explicit.
  const fallback=await context.newPage();
  await fallback.route('**/art/**',route=>route.fulfill({status:200,body:''}));
  await fallback.goto(origin+'/');
  await fallback.waitForFunction(()=>window.orbitView?.snapshot);
  await fallback.clock.install();
  await fallback.getByRole('button',{name:'Take control'}).click();await fallback.clock.runFor(200);
  check((await observe(fallback)).time>0,'missing artwork keeps simulation usable');
  await fallback.close();
  check(errors.length===0,`browser errors: ${errors.join('; ')}`);
  check(external.length===0,`external requests: ${external.length}`);
  const result={checks,errors:errors.length,externalRequests:external.length,
    manual:{time:manual.time,deltaV:manual.dv,heading:turned.heading,velocityAngle:Math.atan2(turned.vy,turned.vx)},
    transfer:{radius:final.radius,deltaV:final.dv,time:final.time,status:final.status},
    screenshots:fs.readdirSync('docs/images').filter(x=>/\.(jpg|png)$/.test(x)).sort()};
  fs.writeFileSync('docs/validation.json',JSON.stringify(result,null,2)+'\n');
  console.log(JSON.stringify(result,null,2));
}
run().catch(error=>{console.error(error);process.exitCode=1}).finally(async()=>{
  if(browser)await browser.close();server.close();
});
