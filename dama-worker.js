/* Transport only: all move generation and search execute in C++/WASM. */
let enginePromise;
function engine() {
  if (!enginePromise) enginePromise = (async () => {
    importScripts('engine.js');
    return createDamaEngine({locateFile: name => new URL(name, self.location.href).href});
  })();
  return enginePromise;
}
self.onmessage = async ({data}) => {
  try {
    const module = await engine();
    const {id, board, player, history = [], difficulty = 5, op = 0} = data;
    if (![8,10].includes(board?.length) || board.some(row => row.length !== board.length)) throw Error('Invalid board dimensions');
    const budgets = [0,100,250,600,1400,2600], depths = [0,2,4,7,14,30];
    const level = Math.max(1,Math.min(5, difficulty|0));
    const input = [1,op,board.length,player,budgets[level],depths[level],...board.flat(),history.length,...history].join(' ');
    const result = JSON.parse(module.ccall('dama_request','string',['string'],[input]));
    if(result.error) throw Error(result.error);
    self.postMessage({id,result});
  } catch(error) { self.postMessage({id:data.id,error:String(error.message || error)}); }
};
