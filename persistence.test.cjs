const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const KEY = 'dama15.autosave.v1';
const launchOptions = () => ({headless:true, ...(process.env.DAMA_CHROMIUM_PATH ? {executablePath:process.env.DAMA_CHROMIUM_PATH} : {})});
const ready = page => page.waitForFunction(() => !restoringSavedGame && board.length === BOARD_SIZE &&
    (DAMA_SAVE_ENGINE !== 'wasm' || !loadingMoves));
const state = page => page.evaluate(() => ({board, currentPlayer, moveCount, isEditing, selectedEditorValue,
    currentPreset, currentRotation, currentLang, difficultyLevel, gameMode, startingPlayer,
    p1:document.getElementById('picker-p1').value, p2:document.getElementById('picker-p2').value,
    time:document.getElementById('ai-time')?.value, customPieceColors}));

async function runPersistenceTests(browser, target) {
    const context = await browser.newContext();
    const errors = [];
    let page = await context.newPage();
    const watch = p => p.on('pageerror', error => errors.push(error.message));
    watch(page);
    try {
        await page.goto(target); await ready(page);
        const kind = await page.evaluate(() => DAMA_SAVE_ENGINE);
        await page.evaluate(async () => {
            setMode('2p'); applyBoardPreset(4); rotateBoard(90); setLang('fr'); setDiff(3);
            document.getElementById('picker-p1').value = '#aabbcc';
            document.getElementById('picker-p2').value = '#153759'; updatePieceColors();
            const time = document.getElementById('ai-time');
            if (time) { time.value = '1000'; time.dispatchEvent(new Event('change')); }
            toggleEditorMode(); clearEntireBoard();
            selectEditorPiece(2); handleSquareClick(6,0);
            selectEditorPiece(-2); handleSquareClick(0,6);
        });
        const edited = await state(page);
        await page.reload(); await ready(page);
        assert.deepEqual(await state(page), edited, 'settings and unfinished editor survive reload');
        assert.equal(await page.locator('#editor-toolbar').evaluate(el => el.classList.contains('active')), true);
        assert.equal(await page.locator('#board').evaluate(el => el.style.transform), 'rotate(90deg)');
        assert.equal(await page.locator('#diff-3').evaluate(el => el.classList.contains('active')), true);
        await page.evaluate(() => finalizeEdit(P1));
        await page.waitForFunction(() => !isEditing && (DAMA_SAVE_ENGINE !== 'wasm' || !loadingMoves));
        const beforeMove = await state(page);
        await page.evaluate(() => executeMoveStepByStep(legalMoves[0]));
        await page.waitForFunction(() => moveCount === 1 && !isAnimating && (DAMA_SAVE_ENGINE !== 'wasm' || !loadingMoves));
        const afterMove = await state(page);
        await page.close(); page = await context.newPage(); watch(page);
        await page.goto(target); await ready(page);
        assert.deepEqual(await state(page), afterMove, 'game survives closing and reopening the page');
        if (kind === 'native') {
            await page.locator('#ui-btn-undo').click();
            assert.deepEqual(await state(page), beforeMove, 'restored Undo returns to the previous full position');
        }
        await page.locator('#ui-btn-new').click();
        await page.waitForFunction(() => moveCount === 0 && !isEditing && (DAMA_SAVE_ENGINE !== 'wasm' || !loadingMoves));
        assert.equal(await page.evaluate(() => board.flat().filter(p => p === 1).length),12);
        const fresh = await state(page);
        for (const key of ['currentPreset','currentRotation','currentLang','difficultyLevel','gameMode','p1','p2','customPieceColors','time'])
            assert.deepEqual(fresh[key],edited[key], 'new game preserves '+key);

        // Closing during multi-jump animation must not commit the half-capture.
        await page.evaluate(async () => {
            applyBoardPreset(1);
            if (DAMA_SAVE_ENGINE === 'wasm') await new Promise(resolve => setTimeout(resolve,150));
            toggleEditorMode(); clearEntireBoard();
            for (const [r,c,p] of [[6,1,1],[5,2,-1],[3,4,-1],[0,9,-2]]) { selectEditorPiece(p); handleSquareClick(r,c); }
            await finalizeEdit(P1);
        });
        await page.waitForFunction(() => legalMoves.length && !isEditing && (DAMA_SAVE_ENGINE !== 'wasm' || !loadingMoves));
        const beforeCapture = await state(page);
        await page.evaluate(() => { executeMoveStepByStep(legalMoves[0]); saveUserGame(); });
        assert.equal(await page.evaluate(() => isAnimating), true);
        await page.reload(); await ready(page);
        assert.deepEqual(await state(page),beforeCapture,'interrupted capture restores the previous stable board');
        await page.evaluate(key => localStorage.setItem(key, '{broken-json'), KEY);
        await page.reload(); await ready(page);
        assert.equal(await page.evaluate(() => BOARD_SIZE),10);
        assert.equal(await page.evaluate(() => board.flat().filter(p => p===1).length),15);
        await page.evaluate(key => localStorage.setItem(key,JSON.stringify({version:1,settings:{preset:2,lang:'fr'},game:{board:[[99]],currentPlayer:1,moveCount:0}})),KEY);
        await page.reload(); await ready(page);
        assert.equal(await page.evaluate(() => BOARD_SIZE),8);
        assert.equal(await page.evaluate(() => board.flat().filter(p => p===1).length),12);
        assert.equal(await page.evaluate(() => currentLang),'fr');
        assert.deepEqual(errors,[]);
        console.log(kind+': reload, editor, completed move, Undo, new game, interrupted capture and corrupt save passed');
    } finally { await context.close(); }

    const blocked = await browser.newContext();
    try {
        await blocked.addInitScript(() => Object.defineProperty(Storage.prototype,'setItem',{value(){throw new DOMException('Storage blocked','QuotaExceededError');}}));
        const page = await blocked.newPage(); await page.goto(target); await ready(page);
        assert.equal(await page.evaluate(() => validSavedBoard(board)),true);
        assert.equal(await page.evaluate(() => saveUnavailable),true);
        assert.ok((await page.locator('#save-status').textContent()).length>0);
    } finally { await blocked.close(); }
}

async function runPersistentProfileTest(type, target) {
    const profile = fs.mkdtempSync(path.join(os.tmpdir(),'dama-save-profile-'));
    let context;
    try {
        context = await type.launchPersistentContext(profile,launchOptions());
        let page = context.pages()[0] || await context.newPage();
        await page.goto(target); await ready(page);
        await page.evaluate(() => {setMode('2p');setLang('fr');setDiff(2);rotateBoard(180);});
        const expected = await state(page);
        await context.close(); context = null;
        context = await type.launchPersistentContext(profile,launchOptions());
        page = context.pages()[0] || await context.newPage();
        await page.goto(target); await ready(page);
        assert.deepEqual(await state(page),expected,'disk-backed state survives full browser shutdown/restart');
        console.log('Disk profile: full browser shutdown/restart passed');
    } finally {
        if (context) await context.close();
        fs.rmSync(profile,{recursive:true,force:true});
    }
}
module.exports = {runPersistenceTests, runPersistentProfileTest};
