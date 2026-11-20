// Original UI rule generator, retained ONLY as regression oracle.
let BOARD_SIZE=10;
function getAllLegalMoves(b, player, isSearch = false) {
    let captureMoves = [], normalMoves = [];
    for (let r = 0; r < BOARD_SIZE; r++) {
        for (let c = 0; c < BOARD_SIZE; c++) {
            const p = b[r][c];
            if (p === 0 || (p > 0 ? 1 : -1) !== player) continue;
            const caps = getCapturesForPiece(b, r, c, p, [], [], isSearch);
            if (caps.length > 0) captureMoves.push(...caps);
            else if (captureMoves.length === 0) normalMoves.push(...getNormalMovesForPiece(b, r, c, p));
        }
    }
    if (captureMoves.length > 0) {
        let maxCapCount = 0;
        for (let i = 0; i < captureMoves.length; i++) {
            if (captureMoves[i].captures.length > maxCapCount) {
                maxCapCount = captureMoves[i].captures.length;
            }
        }
        return captureMoves.filter(m => m.captures.length === maxCapCount);
    }
    return normalMoves;
}

function getNormalMovesForPiece(b, r, c, piece) {
    const moves = [], isKing = Math.abs(piece) === 2, forwardDir = piece > 0 ? -1 : 1;
    if (!isKing) {
        const nr = r + forwardDir;
        if (nr >= 0 && nr < BOARD_SIZE) {
            if (c > 0 && b[nr][c - 1] === 0) moves.push({ fromRow: r, fromCol: c, toRow: nr, toCol: c - 1, captures: [], steps: null });
            if (c < BOARD_SIZE - 1 && b[nr][c + 1] === 0) moves.push({ fromRow: r, fromCol: c, toRow: nr, toCol: c + 1, captures: [], steps: null });
        }
    } else {
        for (const [dr, dc] of [[-1,-1], [-1,1], [1,-1], [1,1]]) {
            let nr = r + dr, nc = c + dc;
            while (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE && b[nr][nc] === 0) {
                moves.push({ fromRow: r, fromCol: c, toRow: nr, toCol: nc, captures: [], steps: null });
                nr += dr; nc += dc;
            }
        }
    }
    return moves;
}

function getCapturesForPiece(b, r, c, piece, alreadyCaptured, stepsSoFar, isSearch = false) {
    if (alreadyCaptured.length >= 12) return [];
    const results = [], isKing = Math.abs(piece) === 2, player = piece > 0 ? 1 : -1, forwardDir = player > 0 ? -1 : 1;
    
    if (!isKing) {
        const midR = r + forwardDir, landR = r + forwardDir * 2;
        if (landR >= 0 && landR < BOARD_SIZE) {
            for (const dc of [-1, 1]) {
                const midC = c + dc, landC = c + dc * 2;
                if (landC >= 0 && landC < BOARD_SIZE && b[landR][landC] === 0) {
                    const midPiece = b[midR][midC];
                    if (midPiece !== 0 && (midPiece > 0 ? 1 : -1) === -player && !isCaptured(alreadyCaptured, midR, midC)) {
                        const newCap = [...alreadyCaptured, { r: midR, c: midC }];
                        const newSteps = isSearch ? null : [...stepsSoFar, { capR: midR, capC: midC, landR, landC }];
                        const savedOrig = b[r][c], savedLand = b[landR][landC];
                        b[r][c] = 0; b[landR][landC] = piece;
                        const further = getCapturesForPiece(b, landR, landC, piece, newCap, newSteps, isSearch);
                        b[r][c] = savedOrig; b[landR][landC] = savedLand;
                        if (further.length > 0) {
                            for (let i = 0; i < further.length; i++) {
                                const fc = further[i];
                                results.push({ fromRow: r, fromCol: c, toRow: fc.toRow, toCol: fc.toCol, captures: fc.captures, steps: fc.steps });
                            }
                        } else {
                            results.push({ fromRow: r, fromCol: c, toRow: landR, toCol: landC, captures: newCap, steps: newSteps });
                        }
                    }
                }
            }
        }
    } else {
        for (const [dr, dc] of [[-1,-1], [-1,1], [1,-1], [1,1]]) {
            let curR = r + dr, curC = c + dc;
            while (curR >= 0 && curR < BOARD_SIZE && curC >= 0 && curC < BOARD_SIZE && b[curR][curC] === 0) { curR += dr; curC += dc; }
            if (curR >= 0 && curR < BOARD_SIZE && curC >= 0 && curC < BOARD_SIZE) {
                const midPiece = b[curR][curC];
                if (midPiece !== 0 && (midPiece > 0 ? 1 : -1) === -player && !isCaptured(alreadyCaptured, curR, curC)) {
                    const capR = curR, capC = curC;
                    let landR = capR + dr, landC = capC + dc;
                    let continuationsFound = false;
                    const quietLandings = [];

                    while (landR >= 0 && landR < BOARD_SIZE && landC >= 0 && landC < BOARD_SIZE && b[landR][landC] === 0) {
                        const newCap = [...alreadyCaptured, { r: capR, c: capC }];
                        const newSteps = isSearch ? null : [...stepsSoFar, { capR, capC, landR, landC }];
                        const savedOrig = b[r][c], savedLand = b[landR][landC];
                        b[r][c] = 0; b[landR][landC] = piece;
                        const further = getCapturesForPiece(b, landR, landC, piece, newCap, newSteps, isSearch);
                        b[r][c] = savedOrig; b[landR][landC] = savedLand;

                        if (further.length > 0) {
                            continuationsFound = true;
                            for (let i = 0; i < further.length; i++) {
                                const fc = further[i];
                                results.push({ fromRow: r, fromCol: c, toRow: fc.toRow, toCol: fc.toCol, captures: fc.captures, steps: fc.steps });
                            }
                        } else {
                            quietLandings.push({ fromRow: r, fromCol: c, toRow: landR, toCol: landC, captures: newCap, steps: newSteps });
                        }
                        landR += dr; landC += dc;
                    }
                    for (let i = 0; i < quietLandings.length; i++) {
                        if (!isSearch || !continuationsFound || i === 0 || i === quietLandings.length - 1) {
                            results.push(quietLandings[i]);
                        }
                    }
                }
            }
        }
    }
    return results;
}

function isCaptured(list, r, c) {
    for (let i = 0; i < list.length; i++) if (list[i].r === r && list[i].c === c) return true;
    return false;
}


module.exports=(board,side)=>{BOARD_SIZE=board.length;return getAllLegalMoves(board,side,false);};
