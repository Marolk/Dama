#define main dama_cli_entrypoint
#include "engine.cpp"
#undef main
#include <cassert>

static bool same(const Move&a,const Move&b){return a.from==b.from&&a.to==b.to&&a.caps==b.caps&&a.path==b.path;}
static bool isLegal(const Board&b,int side,const Move&m){for(auto&x:moves(b,side))if(same(x,m))return true;return false;}
// Independent bounded AND/OR oracle: no heuristic, pruning, TT or extensions.
// A true result proves a win against EVERY reply within this horizon.
static bool forcedWin(const Board& b,int side,int winner,int plies) {
 auto ms=moves(b,side);if(ms.empty())return side!=winner;
 if(plies==0)return false;
 if(side==winner){for(const auto&m:ms)if(forcedWin(applyMove(b,m),-side,winner,plies-1))return true;return false;}
 for(const auto&m:ms)if(!forcedWin(applyMove(b,m),-side,winner,plies-1))return false;
 return true;
}
static void bestScore(const Remontada::Analysis&a) {
 assert(a.result.depth>0&&!a.candidates.empty());
 bool found=false;
 for(auto&c:a.candidates){assert(c.score<=a.result.score);if(same(c.move,a.result.best)){found=true;assert(c.score==a.result.score);}}
 assert(found);
}
int main() {
 Board b;b.n=8;for(int p:{3,19,23,26,30,49})b.a[p]=-1;for(int p:{17,60})b.a[p]=1;b.a[24]=2;b.bits();
 auto a=Remontada::choose(b,1,500);bestScore(a);
 assert(a.result.best.from==17&&a.result.best.to==10&&a.result.score>0&&a.extensions>0);
 for(auto&r:moves(applyMove(b,a.result.best),-1)) {
  assert(r.caps.size()==1);auto after=applyMove(applyMove(b,a.result.best),r);
  for(auto&c:moves(after,1))assert(c.caps.size()==3);
 }
 std::cout<<"PASS level7: 3-vs-6 offer; every forced reply permits triple countercapture (exchange proof, not full-game proof)\n";
 Board multi;multi.n=8;
 for(auto [i,v]:vector<pair<int,int>>{{3,-1},{8,-1},{10,-1},{17,1},{21,-1},{23,2},{39,1},{42,1},{49,-1}})multi.a[i]=v;
 multi.bits();auto sacrifice=Remontada::choose(multi,1,1500,6);bestScore(sacrifice);
 assert(sacrifice.result.depth==6&&sacrifice.result.best.from==42&&sacrifice.result.best.to==35);
 auto offered=applyMove(multi,sacrifice.result.best);
 for(auto&r:moves(offered,-1))assert(r.caps.size()==2);
 assert(sacrifice.result.score>0);
 std::cout<<"PASS level7: a two-piece sacrifice survives adversarial depth-six analysis\n";
 Board danger;danger.n=8;danger.a[42]=2;danger.a[19]=-1;danger.bits();
 auto safe=Remontada::choose(danger,1,150);bestScore(safe);
 assert(safe.result.best.to!=28);assert(forcedWin(applyMove(danger,safe.result.best),-1,1,4));
 for(auto&c:safe.candidates)if(c.move.to==28)assert(c.score<-99000);
 std::cout<<"PASS refuted king sacrifice rejected; selected line independently proves a win against all replies\n";
 for(int pieces=1;pieces<=4;pieces++) {
  Board end;end.n=10;end.a[81]=2;for(int i=0;i<pieces-1;i++)end.a[90+2*i]=1;
  for(int p:{63,36,18})end.a[p]=-1;end.bits();
  auto win=Remontada::choose(end,1,100);bestScore(win);
  assert(forcedWin(applyMove(end,win.result.best),-1,1,2));
  assert(win.result.score>99000&&win.result.best.caps.size()==3);
  assert(Remontada::countPieces(applyMove(end,win.result.best),1)==pieces);
 }
 std::cout<<"PASS terminal wins retaining 1, 2, 3 or 4 pieces, independently checked\n";
 Board promotion;promotion.n=8;promotion.a[17]=1;promotion.a[10]=-1;promotion.a[53]=-1;promotion.bits();
 auto crowned=Remontada::choose(promotion,1,150);assert(isLegal(promotion,1,crowned.result.best));
 assert(applyMove(promotion,crowned.result.best).a[crowned.result.best.to]==2);
 std::cout<<"PASS promotion and search extensions\n";
 Board style;style.n=8;
 for(auto [i,v]:vector<pair<int,int>>{{1,-1},{12,-1},{17,1},{23,-1},{24,-1},{26,1},{28,-1},{46,1},{55,2}})style.a[i]=v;
 style.bits();
 Remontada::Session neutral(Clock::now()+chrono::seconds(3));auto normal=neutral.run(style,1,4,false);
 Remontada::Session comeback(Clock::now()+chrono::seconds(3));auto different=comeback.run(style,1,4,true);
 bestScore(normal);bestScore(different);
 assert(normal.result.depth==4&&different.result.depth==4);
 assert(normal.result.score==different.result.score&&different.result.score==331);
 assert(normal.result.best.from==46&&normal.result.best.to==39);
 assert(different.result.best.from==46&&different.result.best.to==37);
 assert(TrapMaster::offeredPieces(style,1,different.result.best)>0);
 std::cout<<"PASS behavior ablation: same depth and score, REMONTADA selects 46->37 sacrifice instead of quiet 46->39\n";
 for(int n:{8,10}) {
  Board start;start.n=n;for(int r=0;r<n;r++)for(int c=0;c<n;c++)if((r+c)%2){if(r<3)start.a[r*n+c]=-1;if(r>=n-3)start.a[r*n+c]=1;}start.bits();
  for(int ms:{50,500,2900}) {
   const auto begin=Clock::now();auto r=Remontada::choose(start,1,ms);
   const auto elapsed=chrono::duration_cast<chrono::milliseconds>(Clock::now()-begin).count();
   assert(isLegal(start,1,r.result.best));bestScore(r);assert(elapsed<ms+700);
   assert(r.candidates.size()==moves(start,1).size());
   std::cout<<"PASS complete root comparison "<<n<<"x"<<n<<" budget="<<ms<<" elapsed="<<elapsed<<" depth="<<r.result.depth<<"\n";
  }
 }
 std::ostringstream request;request<<8<<" 1 500 7";for(int i=0;i<64;i++)request<<' '<<int(b.a[i]);
 const auto response=solve(request.str());assert(response.rfind(" 7")==response.size()-2);
 assert(response.starts_with("17 10 0 "));
 std::cout<<"PASS native protocol dispatches difficulty 7 to REMONTADA\n";
}
