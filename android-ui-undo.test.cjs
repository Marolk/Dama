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
   window.lastNativePayload=envelope;
   const i=envelope.indexOf('|');
   const answer=await window.nativeTestSolve(envelope.slice(i+1));
   window.damaNativeResult(envelope.slice(0,i)+'|'+answer);
  }};
  setMode('2p');
 });
 const state=()=>page.evaluate(()=>({board:board.map(r=>r.slice()),currentPlayer,moveCount,lastMove,isGameOver,captureTrail}));
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
   cancelPendingAnimation(); moveHistory=[];captureTrail=null;board=Array.from({length:10},()=>Array(10).fill(0));
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
 // Flying king: trace every landing, preserve during a normal reply, rotate, undo.
 await fixture([[8,1,2],[6,3,-1],[3,6,-1],[1,8,-1],[9,8,-2]]);
 const kingBefore=await state();
 const kingMove=await page.evaluate(()=>legalMoves[0]);
 assert.equal(kingMove.captures.length,3);
 const colors=await page.evaluate(()=>[
   getComputedStyle(document.querySelector('.king.p1-style')).backgroundImage,
   getComputedStyle(document.querySelector('.king.p2-style')).backgroundImage
 ]);
 assert.notEqual(colors[0],colors[1],'Kings must have distinct colors');
 await play();
 assert.equal(await page.locator('#capture-badges text').allTextContents().then(x=>x.join(',')),'0,1,2,3');
 const expected=[{r:kingMove.fromRow,c:kingMove.fromCol},...kingMove.steps.map(s=>({r:s.landR,c:s.landC}))];
 assert.deepEqual(await page.evaluate(()=>captureTrail.points),expected);
 const coords = (rtl)=>expected.map(p=>((rtl?10-p.c-.5:p.c+.5)*100)+','+((p.r+.5)*100)).join(' ');
 assert.equal(await page.locator('.capture-path').getAttribute('points'),coords(true));
 await page.evaluate(()=>setLang('fr'));
 assert.equal(await page.locator('.capture-path').getAttribute('points'),coords(false));
 await page.evaluate(()=>rotateBoard(180));
 assert.equal(await page.locator('#capture-badges text').count(),4);
 await page.evaluate(()=>rotateBoard(180));
 await page.locator('#board-wrapper').screenshot({path:'dama-capture-preview.png'});
 const traceAfterCapture=await page.evaluate(()=>captureTrail);
 await play(); // A normal opponent reply must not hide the last capture route.
 assert.deepEqual(await page.evaluate(()=>captureTrail),traceAfterCapture);
 await page.locator('#ui-btn-undo').click();
 assert.deepEqual(await page.evaluate(()=>captureTrail),traceAfterCapture);
 await page.locator('#ui-btn-undo').click();
 assert.deepEqual(await state(),kingBefore);
 assert.equal(await page.locator('#capture-route').count(),0);

 // The trail disappears three seconds after completion, independently of later moves.
 await fixture([[6,1,1],[5,2,-1],[3,4,-1],[0,9,-2]]);
 await play();
 const afterTimedCapture=await state();
 await play(); // Normal reply must not restart the countdown.
 await page.waitForTimeout(2200);
 assert.equal(await page.locator('#capture-route').count(),1,'Trail should still be visible before expiry');
 await page.evaluate(()=>rotateBoard(180)); // A redraw must not restart the countdown.
 await page.waitForTimeout(1000);
 assert.equal(await page.locator('#capture-route').count(),0,'Trail must disappear without another capture');
 assert.equal(await page.locator('#capture-badges').count(),0);
 await page.locator('#ui-btn-undo').click();
 const restored=await state();
 assert.deepEqual(restored.board,afterTimedCapture.board);
 assert.equal(restored.currentPlayer,afterTimedCapture.currentPlayer);
 assert.equal(restored.captureTrail,null,'Undo must not resurrect an expired trail');
 await page.evaluate(()=>rotateBoard(180));
 // A later capture gets its own three seconds; the old timer cannot erase it.
 await fixture([[6,1,1],[5,2,-1],[3,4,-1],[0,7,-2]]);
 await play();
 await page.waitForTimeout(1800);
 await play();
 assert.equal(await page.evaluate(()=>captureTrail.player),-1);
 await page.waitForTimeout(1300);
 assert.equal(await page.locator('#capture-route').count(),1,'Old countdown must not erase the new capture');
 await page.waitForTimeout(1900);
 assert.equal(await page.locator('#capture-route').count(),0,'New capture must expire on its own countdown');
 console.log('PASS 3-second capture trail expiry; no reset on quiet moves/rotation; expired Undo; fresh timer on a new capture.');

 await page.evaluate(()=>{setMode('ai');document.getElementById('ai-time').value='500';});
 const aiBefore=await state(); await play();
 await page.waitForFunction(()=>moveHistory.length===2&&!isAnimating&&currentPlayer===1,{},{timeout:10000});
 await page.locator('#ui-btn-undo').click();assert.deepEqual(await state(),aiBefore);
 assert.equal(await page.evaluate(()=>moveHistory.length),0);

 // Level 6 uses the actual C++ tactical engine: 3 pieces vs 6, bait then forced countercapture.
 await page.evaluate(()=>{
  setDiff(6);BOARD_SIZE=8;INITIAL_PIECES=12;initGame();
  board=Array.from({length:8},()=>Array(8).fill(0));
  const original={3:-1,17:1,19:-1,23:-1,24:2,26:-1,30:-1,49:-1,60:1};
  for(const [pos,val] of Object.entries(original)){const dst=63-Number(pos);board[Math.floor(dst/8)][dst%8]=-val;}
  currentPlayer=P2;gameMode='ai';isAiThinking=true;isGameOver=false;
  document.getElementById('ai-time').value='500';
  updateLegalMoves();renderBoard();updateStatus();makeAIMove();
 });
 await page.waitForFunction(()=>moveHistory.length===1&&!isAnimating&&currentPlayer===P1,{},{timeout:10000});
 assert.equal(await page.locator('#diff-6').getAttribute('aria-pressed'),'true');
 assert.equal(await page.locator('#impossible-note').isVisible(),true);
 assert.equal(await page.evaluate(()=>window.lastNativePayload.split('|')[1].trim().split(/\s+/)[3]),'6');
 assert.equal(await page.evaluate(()=>board[6][5]===P2&&board[5][6]===0),true,'Native level 6 must play the bait 46->53');
 const afterBait=await state();
 assert.equal(await page.evaluate(()=>legalMoves[0].captures.length),1);
 await page.evaluate(()=>executeMoveStepByStep(legalMoves[0]));
 await page.waitForFunction(()=>moveHistory.length===3&&!isAnimating&&currentPlayer===P1,{},{timeout:10000});
 assert.equal(await page.evaluate(()=>lastMove.captures.length),3);
 assert.equal(await page.locator('#capture-badges text').count(),4);
 await page.locator('#ui-btn-undo').click();assert.deepEqual(await state(),afterBait);
 console.log('PASS Impossible level 6: actual native bait, forced human capture and triple countercapture; Undo restores the bait position.');

 assert.deepEqual(errors,[]);
 console.log('PASS: visible Undo; repeated 2-player undo; double capture; promotion; actual C++ AI reply + two-ply undo; Offline/Online labels; 3-second default.');
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
