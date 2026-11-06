#define main dama_cli_entrypoint
#include "engine.cpp"
#undef main
#include <cassert>
static bool sameMove(const Move&a,const Move&b){return a.from==b.from&&a.to==b.to&&a.caps==b.caps&&a.path==b.path;}
static bool legalMove(const Board&b,int side,const Move&m){for(const auto &x:moves(b,side))if(sameMove(x,m))return true;return false;}
static Board sacrificeFixture(){Board b;b.n=8;for(int p:{3,19,23,26,30,49})b.a[p]=-1;for(int p:{17,60})b.a[p]=1;b.a[24]=2;b.bits();return b;}
static void forcedCountercapture(const Board&b,const Move&m,int side){
 assert(m.caps.empty());auto after=applyMove(b,m);auto replies=moves(after,-side);assert(!replies.empty());
 for(const auto &r:replies){assert(r.caps.size()==1);auto counter=moves(applyMove(after,r),side);assert(!counter.empty());for(const auto &c:counter)assert(c.caps.size()==3);}
}
int main(){
 auto b=sacrificeFixture();assert(moves(b,1)[0].caps.empty());
 auto r=TrapMaster::choose(b,1,500);assert(legalMove(b,1,r.best));assert(r.best.from==17&&r.best.to==10);
 assert(r.score>0);assert(TrapMaster::offeredPieces(b,1,r.best)==1);forcedCountercapture(b,r.best,1);
 std::cout<<"PASS 3 vs 6: sacrifice 17->10; every legal opponent capture loses three men to the countercapture. This proves only this short exchange, not the whole game.\n";
 Board mirrored;mirrored.n=8;for(int i=0;i<64;i++)mirrored.a[63-i]=-b.a[i];mirrored.bits();
 auto mr=TrapMaster::choose(mirrored,-1,500);assert(legalMove(mirrored,-1,mr.best));assert(mr.best.from==46&&mr.best.to==53);forcedCountercapture(mirrored,mr.best,-1);
 Board danger;danger.n=8;danger.a[42]=2;danger.a[19]=-1;danger.bits();
 Move poison;for(const auto &m:moves(danger,1))if(m.to==28)poison=m;
 assert(TrapMaster::offeredPieces(danger,1,poison)==1);
 auto safe=TrapMaster::choose(danger,1,150);assert(legalMove(danger,1,safe.best));
 for(const auto &reply:moves(applyMove(danger,safe.best),-1))assert(!moves(applyMove(applyMove(danger,safe.best),reply),1).empty());
 std::cout<<"PASS dangerous king sacrifice rejected\n";
 for(int n:{8,10}){Board start;start.n=n;for(int row=0;row<n;row++)for(int col=0;col<n;col++)if((row+col)%2){if(row<3)start.a[row*n+col]=-1;else if(row>=n-3)start.a[row*n+col]=1;}start.bits();
  for(int ms:{50,250}){auto tick=Clock::now();auto result=TrapMaster::choose(start,1,ms);auto elapsed=chrono::duration_cast<chrono::milliseconds>(Clock::now()-tick).count();assert(legalMove(start,1,result.best));assert(elapsed<ms+1000);
  std::cout<<"PASS level6 "<<n<<"x"<<n<<" budget="<<ms<<"ms elapsed="<<elapsed<<"ms depth="<<result.depth<<" nodes="<<result.nodes<<"\n";}
  auto tick=Clock::now();auto result=TrapMaster::choose(start,1,2900);auto elapsed=chrono::duration_cast<chrono::milliseconds>(Clock::now()-tick).count();assert(legalMove(start,1,result.best));assert(elapsed<3900);
  std::cout<<"PASS level6 "<<n<<"x"<<n<<" budget=2900ms elapsed="<<elapsed<<"ms depth="<<result.depth<<" nodes="<<result.nodes<<"\n";
  auto legacy=choose(start,1,250,4);
  std::cout<<"COMPARISON "<<n<<"x"<<n<<" legacy5 250ms depth="<<legacy.depth<<" nodes="<<legacy.nodes<<"\n";
 }
 std::ostringstream in;in<<8<<" "<<1<<" "<<500<<" "<<6;for(int i=0;i<64;i++)in<<" "<<int(b.a[i]);
 std::istringstream out(solve(in.str()));int from,to,count;out>>from>>to>>count;assert(from==17&&to==10&&count==0);int plen;out>>plen;for(int i=0,x;i<plen;i++)out>>x;int depth,score;uint64_t nodes;int trap,offered;out>>depth>>score>>nodes>>trap>>offered;assert(out&&trap==1&&offered==1);
 std::cout<<"PASS native protocol level6 + probable-sacrifice metadata\n";
}