const assert=require('node:assert/strict');
const {spawnSync}=require('node:child_process');
const fs=require('node:fs');
const oracle=require('./reference-rules.js');
const create=require('../dist/engine.js');
const initial=n=>Array.from({length:n},(_,r)=>Array.from({length:n},(_,c)=>(r+c)%2?(r<3?-1:r>=n-3?1:0):0));
const wire=(b,s,op=0,ms=100,d=5,h=[])=>[1,op,b.length,s,ms,d,...b.flat(),h.length,...h].join(' ');
const canonical=m=>JSON.stringify([m.fromRow,m.fromCol,m.toRow,m.toCol,m.captures,m.steps||[]]);
const moveset=m=>m.map(canonical).sort();
let seed=74891;const random=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed/2**32;};
(async()=>{
 const wasm=await create({wasmBinary:fs.readFileSync('dist/engine.wasm')});
 const ask=input=>JSON.parse(wasm.ccall('dama_request','string',['string'],[input]));
 let cases=[];
 for(const n of [8,10])for(const parity of [0,1]){
  cases.push(wire(initial(n),1));
  for(let i=0;i<100;i++){let b=Array.from({length:n},()=>Array(n).fill(0));for(let r=0;r<n;r++)for(let c=0;c<n;c++)if((r+c)%2===parity&&random()<.2)b[r][c]=[1,-1,2,-2][Math.floor(random()*4)];let side=i%2?1:-1;const input=wire(b,side);cases.push(input);assert.deepEqual(moveset(ask(input).moves),moveset(oracle(b,side)));}
 }
 const native=spawnSync('./build/engine-cli',{input:cases.join('\n')+'\n',encoding:'utf8',maxBuffer:30e6});assert.equal(native.status,0,native.stderr);
 native.stdout.trim().split('\n').forEach((line,i)=>assert.deepEqual(JSON.parse(line),ask(cases[i])));
 for(const n of [8,10]){const b=initial(n);const r=ask(wire(b,1,1,150,30));assert.ok(r.elapsedMs<800);assert.ok(moveset(r.moves).includes(canonical(r.best)));console.log(`WASM ${n}x${n}: ${r.elapsedMs} ms, depth ${r.depth}, ${r.nodes} nodes`);}
 const tactical=Array.from({length:8},()=>Array(8).fill(0));
 for(const [i,v] of [[12,-1],[24,-1],[26,1],[28,1],[37,-1],[46,1]])tactical[Math.floor(i/8)][i%8]=v;
 const trap=ask(wire(tactical,1,1,1000,6));assert.equal(trap.best.fromRow*8+trap.best.fromCol,28);assert.equal(trap.best.toRow*8+trap.best.toCol,19);
 // Both native strategies are compiled into WASM; never map REMONTADA to level 5.
 const rem=Array.from({length:8},()=>Array(8).fill(0));
 for(const [i,v] of [[3,-1],[17,1],[19,-1],[23,-1],[24,2],[26,-1],[30,-1],[49,-1],[60,1]])rem[Math.floor(i/8)][i%8]=v;
 for(const strategy of [6,7]) {
  const r=ask(wire(rem,1,1,1000,30)+' '+strategy);
  assert.equal(r.strategy,strategy);assert.equal(r.best.fromRow*8+r.best.fromCol,17);assert.equal(r.best.toRow*8+r.best.toCol,10);
  const h=ask(wire(rem,1)).hash;assert.equal(ask(wire(rem,1,1,100,30,[h,h])+' '+strategy).draw,true);
 }
 console.log('WASM strategies 6/7, real tactical sacrifice, and root repetition passed');
 const full=ask(wire(initial(10),-1,1,2600,30));assert.ok(full.elapsedMs<3500);console.log(`WASM full budget: ${full.elapsedMs} ms, depth ${full.depth}, nodes ${full.nodes}`);
 const hash=ask(wire(initial(8),1)).hash;assert.equal(ask(wire(initial(8),1,1,100,5,[hash,hash])).draw,true);
 assert.ok(ask('bad').error);
 console.log(`${cases.length} native/WASM comparisons, 400 original-rule comparisons passed`);
})();
