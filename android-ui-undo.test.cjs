const { chromium } = require('playwright');
const { execFile } = require('node:child_process');
const assert = require('node:assert/strict');
const path = require('node:path');
(async () => {
 const browser = await chromium.launch({headless:true});
 try {
 const page = await browser.newPage({viewport:{width:412,height:915}});
 const errors=[]; page.on('pageerror',e=>errors.push(e.message));
 await page.exposeFunction('nativeTestSolve', input => new Promise((resolve,reject)=>{
  const child=execFile(process.argv[3], {timeout:6000}, (err,out)=>err?reject(err):resolve(out.trim()));
  child.stdin.end(input);
 }));
 await page.goto('file://'+path.resolve(process.argv[2]));
 await page.evaluate(()=>{
  window.DamaNative={solve:async envelope=>{
   const i=envelope.indexOf('|');
   const answer=await window.nativeTestSolve(envelope.slice(i+1));
   window.damaNativeResult(envelope.slice(0,i)+'|'+answer);
  }};
  setMode('2p');
 });
 const state=()=>page.evaluate(()=>({board:board.map(r=>r.slice()),currentPlayer,moveCount,lastMove,isGameOver}));
 const play=async()=>{
  const m=await page.evaluate(()=>legalMoves[0]);
  assert(m,'legal move exists');
  await page.locator('#board .square').nth(m.fromRow*10+m.fromCol).click();
  await page.locator('#board .square').nth(m.toRow*10+m.toCol).click();
  await page.waitForFunction(()=>!isAnimating);
 };
 assert.equal(await page.locator('#engine-online').isDisabled(),true);
 assert.equal(await page.locator('#ai-time').inputValue(),'3000');
 const initial=await state(); await play(); const afterOne=await state(); await play();
 await page.locator('#ui-btn-undo').click(); assert.deepEqual(await state(),afterOne);
 await page.locator('#ui-btn-undo').click(); assert.deepEqual(await state(),initial);
 // Install legal fixture, then use the visible board and undo button.
 const fixture=async(pieces)=>{
  await page.evaluate(pieces=>{
   cancelPendingAnimation(); moveHistory=[];board=Array.from({length:10},()=>Array(10).fill(0));
   for(const [r,c,p] of pieces)board[r][c]=p;
   currentPlayer=1;moveCount=0;lastMove=null;isGameOver=false;isAnimating=false;isAiThinking=false;
   updateLegalMoves();renderBoard();updateStatus();
  },pieces);
 };
 await fixture([[6,1,1],[5,2,-1],[3,4,-1],[0,9,-2]]);
 const captureBefore=await state();
 assert.equal(await page.evaluate(()=>legalMoves[0].captures.length),2);
 await play(); await page.locator('#ui-btn-undo').click();assert.deepEqual(await state(),captureBefore);
 await fixture([[1,2,1],[8,1,-1]]);
 const promotionBefore=await state();await play();
 assert.equal(await page.evaluate(()=>board[0].some(p=>p===2)),true);
 await page.locator('#ui-btn-undo').click();assert.deepEqual(await state(),promotionBefore);
 await page.evaluate(()=>{setMode('ai');document.getElementById('ai-time').value='500';});
 const aiBefore=await state(); await play();
 await page.waitForFunction(()=>moveHistory.length===2&&!isAnimating&&currentPlayer===1,{},{timeout:10000});
 await page.locator('#ui-btn-undo').click();assert.deepEqual(await state(),aiBefore);
 assert.equal(await page.evaluate(()=>moveHistory.length),0);
 assert.deepEqual(errors,[]);
 console.log('PASS: visible Undo; repeated 2-player undo; double capture; promotion; actual C++ AI reply + two-ply undo; Offline/Online labels; 3-second default.');
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
