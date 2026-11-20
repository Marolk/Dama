const {chromium,firefox,webkit}=require('playwright');
const http=require('node:http'),fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const server=http.createServer((req,res)=>{let file=path.join(process.cwd(),'dist',decodeURI(req.url.split('?')[0] === '/' ? '/index.html':req.url.split('?')[0]));if(!fs.existsSync(file)){res.writeHead(404);return res.end();}res.setHeader('Content-Type',file.endsWith('.wasm')?'application/wasm':file.endsWith('.js')?'text/javascript':'text/html');res.end(fs.readFileSync(file));});
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));const target=process.env.DAMA_URL||`http://127.0.0.1:${server.address().port}/`;
 try{for(const [name,type] of Object.entries({chromium,firefox,webkit}).filter(([name])=>!process.env.DAMA_BROWSERS||process.env.DAMA_BROWSERS.split(',').includes(name))){
  const browser=await type.launch({headless:true});const page=await browser.newPage({viewport:{width:390,height:844},hasTouch:true});let errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.goto(target);await page.waitForFunction(()=>!loadingMoves&&legalMoves.length>0);assert.equal(await page.evaluate(()=>engineFailure),'');
  assert.equal(await page.locator('.square').count(),100);
  await page.evaluate(()=>setDiff(1));
  let m=await page.evaluate(()=>legalMoves[0]);
  await page.locator('.square').nth(m.fromRow*10+m.fromCol).tap();
  await page.locator('.square').nth(m.toRow*10+m.toCol).tap();
  await page.waitForFunction(()=>moveCount>=2&&!loadingMoves&&!isAnimating&&!isAiThinking);
  assert.ok((await page.locator('#engine-analysis').textContent()).includes('WASM'));
  await page.evaluate(()=>rotateBoard(90));assert.equal(await page.evaluate(()=>currentRotation),90);
  await page.evaluate(()=>applyBoardPreset(2));await page.waitForFunction(()=>!loadingMoves&&legalMoves.length>0);assert.equal(await page.locator('.square').count(),64);
  await page.evaluate(()=>{toggleEditorMode();clearEntireBoard();});await page.evaluate(()=>finalizeEdit(1));await page.waitForFunction(()=>!loadingMoves);assert.equal(await page.evaluate(()=>isGameOver),true);
  await page.evaluate(()=>initGame());await page.waitForFunction(()=>!loadingMoves&&legalMoves.length>0);
  // Cancel search/animation by starting a new game; old worker responses must not mutate it.
  await page.evaluate(()=>{setStartingPlayer(-1);initGame();setStartingPlayer(1);initGame();});await page.waitForFunction(()=>!loadingMoves&&legalMoves.length>0);await page.waitForTimeout(600);assert.equal(await page.evaluate(()=>moveCount),0);
  assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'horizontal overflow');assert.deepEqual(errors,[]);
  console.log(`${name}: WASM Worker, touch move + AI reply, presets, rotate, edit, restart passed`);
  await require('./persistence.cjs').runPersistenceTests(browser,target);
  await require('./persistence.cjs').runPersistentProfileTest(type,target);
  await browser.close();
 }}finally{server.close();}
})().catch(e=>{console.error(e);process.exit(1);});
