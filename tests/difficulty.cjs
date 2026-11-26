const assert=require('node:assert/strict');
exports.runDifficultyTests=async(browser,url)=>{
 const page=await browser.newPage();const errors=[];
 page.on('pageerror',e=>errors.push(e.message));
 try {
  await page.goto(url);await page.waitForFunction(()=>!restoringSavedGame&&!isAiThinking&&!isAnimating);
  await page.evaluate(()=>setMode('2p'));
  for(const level of [2,3,4,5,6,7]) {
   await page.locator('#diff-'+level).click();
   assert.equal(await page.evaluate(()=>difficultyLevel),level);
   assert.equal(await page.locator('#diff-group [aria-pressed="true"]').count(),1);
   await page.reload();await page.waitForFunction(()=>!restoringSavedGame);
   assert.equal(await page.evaluate(()=>difficultyLevel),level);
   assert.equal(await page.locator('#diff-'+level).getAttribute('aria-pressed'),'true');
  }
  assert.equal(await page.locator('#diff-1').count(),0);
  await page.evaluate(()=>setLang('ar'));
  assert.deepEqual(await page.locator('#diff-group button').allTextContents(),['متوسط','صعب','ديمون','إعجازي','المستحيل — فخاخ وتضحيات','😈 الشيطان الأعظم — REMONTADA']);
  await page.evaluate(()=>setLang('fr'));
  assert.ok((await page.locator('#diff-7').textContent()).includes('😈'));
  assert.equal(await page.locator('#remontada-note').isVisible(),true);
  // Existing Easy saves stay valid; no removed DOM element is dereferenced.
  await page.evaluate(()=>{const s=JSON.parse(localStorage.getItem(DAMA_SAVE_KEY));s.settings.difficulty=1;localStorage.setItem(DAMA_SAVE_KEY,JSON.stringify(s));});
  await page.reload();await page.waitForFunction(()=>!restoringSavedGame);
  assert.equal(await page.evaluate(()=>difficultyLevel),2);
  assert.deepEqual(errors,[]);
  console.log('PASS six levels, exact Arabic order, exclusive selection, reload persistence, removed-Easy migration and languages');
 } finally {await page.close();}
};
