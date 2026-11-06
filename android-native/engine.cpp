#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
using namespace std;
using Clock=chrono::steady_clock;
struct Board { int n=10; array<int8_t,100> a{}; uint64_t occLo=0,occHi=0; void bits(){occLo=occHi=0;for(int i=0;i<n*n;i++)if(a[i]){if(i<64)occLo|=1ULL<<i;else occHi|=1ULL<<(i-64);}}};
struct Move {int from=0,to=0; vector<int> caps; vector<int> path;};
static bool inside(int r,int c,int n){return r>=0&&c>=0&&r<n&&c<n;}
static bool enemy(int a,int b){return a&&b&&((a>0)!=(b>0));}
static const int DR[4]={-1,-1,1,1},DC[4]={-1,1,-1,1};
static void captures(Board &b,int origin,int pos,int piece,vector<int>&taken,vector<int>&path,vector<Move>&out){
 if(taken.size()>=15){out.push_back({origin,pos,taken,path});return;}
 int r=pos/b.n,c=pos%b.n;bool king=abs(piece)==2;bool found=false;
 for(int d=0;d<4;d++){
  if(!king && DR[d]!=(piece>0?-1:1))continue;
  int rr=r+DR[d],cc=c+DC[d];
  if(king)while(inside(rr,cc,b.n)&&!b.a[rr*b.n+cc]){rr+=DR[d];cc+=DC[d];}
  if(!inside(rr,cc,b.n))continue;
  int mid=rr*b.n+cc;
  if(!enemy(piece,b.a[mid])||find(taken.begin(),taken.end(),mid)!=taken.end())continue;
  rr+=DR[d];cc+=DC[d];
  while(inside(rr,cc,b.n)&&!b.a[rr*b.n+cc]){
   int dst=rr*b.n+cc;found=true;
   b.a[pos]=0;b.a[dst]=piece;taken.push_back(mid);path.push_back(dst);
   captures(b,origin,dst,piece,taken,path,out);
   path.pop_back();taken.pop_back();b.a[dst]=0;b.a[pos]=piece;
   if(!king)break;
   rr+=DR[d];cc+=DC[d];
  }
 }
 if(!found&&!taken.empty())out.push_back({origin,pos,taken,path});
}
static vector<Move> moves(Board b,int side){
 vector<Move> cap,quiet;
 for(int p=0;p<b.n*b.n;p++){
  int v=b.a[p];if(!v||(v>0?1:-1)!=side)continue;
  vector<int> t,path;captures(b,p,p,v,t,path,cap);
 }
 if(!cap.empty()){
  size_t mx=0;for(auto &m:cap)mx=max(mx,m.caps.size());
  vector<Move> filtered;for(auto &m:cap)if(m.caps.size()==mx)filtered.push_back(m);return filtered;
 }
 for(int p=0;p<b.n*b.n;p++){
  int v=b.a[p];if(!v||(v>0?1:-1)!=side)continue;
  int r=p/b.n,c=p%b.n;
  for(int d=0;d<4;d++){
   if(abs(v)==1&&DR[d]!=(side>0?-1:1))continue;
   int rr=r+DR[d],cc=c+DC[d];
   while(inside(rr,cc,b.n)&&!b.a[rr*b.n+cc]){
    quiet.push_back({p,rr*b.n+cc,{},{rr*b.n+cc}});
    if(abs(v)==1)break;
    rr+=DR[d];cc+=DC[d];
   }
  }
 }
 return quiet;
}
static Board applyMove(Board b,const Move&m){int v=b.a[m.from];b.a[m.from]=0;for(int x:m.caps)b.a[x]=0;if(v==1&&m.to/b.n==0)v=2;if(v==-1&&m.to/b.n==b.n-1)v=-2;b.a[m.to]=v;b.bits();return b;}
static uint64_t hashBoard(const Board&b,int side){uint64_t h=1469598103934665603ULL;for(int i=0;i<b.n*b.n;i++){h^=(uint8_t)(b.a[i]+3)+(uint64_t)i*1315423911ULL;h*=1099511628211ULL;}return h^(side==1?0x9e3779b97f4a7c15ULL:0x243f6a8885a308d3ULL);}
struct Entry{uint64_t key=0;int depth=-1,score=0,flag=0,from=-1,to=-1;};
struct Search{
 vector<Entry> tt;atomic<bool> stop{false};Clock::time_point deadline;atomic<uint64_t> nodes{0};
 Search():tt(1<<19){}
 bool expired(){if(stop.load(memory_order_relaxed))return true;if((nodes.fetch_add(1,memory_order_relaxed)&1023)==0&&Clock::now()>=deadline)stop.store(true);return stop.load();}
 int eval(const Board&b,int side){int s=0;for(int i=0;i<b.n*b.n;i++){int v=b.a[i];if(!v)continue;int val=abs(v)==2?330:100+(v>0?(b.n-1-i/b.n):i/b.n)*2;s+=(v>0?val:-val);}return s*side;}
 int negamax(const Board&b,int side,int depth,int alpha,int beta,int ply){
  if(expired())return eval(b,side);
  uint64_t key=hashBoard(b,side);Entry &e=tt[key&(tt.size()-1)];int orig=alpha;
  if(e.key==key&&e.depth>=depth){if(e.flag==0)return e.score;if(e.flag==1)alpha=max(alpha,e.score);else beta=min(beta,e.score);if(alpha>=beta)return e.score;}
  auto ms=moves(b,side);if(ms.empty())return -100000+ply;
  if(depth<=0 && ms[0].caps.empty())return eval(b,side);
  if(depth<=-8)return eval(b,side);
  if(e.key==key)stable_sort(ms.begin(),ms.end(),[&](const Move&a,const Move&b){return (a.from==e.from&&a.to==e.to)>(b.from==e.from&&b.to==e.to);});
  int best=-10000000,bf=-1,bt=-1;bool first=true;
  for(auto &m:ms){Board next=applyMove(b,m);int val;
   if(first)val=-negamax(next,-side,depth-1,-beta,-alpha,ply+1);
   else{val=-negamax(next,-side,depth-1,-alpha-1,-alpha,ply+1);if(val>alpha&&val<beta)val=-negamax(next,-side,depth-1,-beta,-alpha,ply+1);}
   first=false;if(stop.load())return eval(b,side);
   if(val>best){best=val;bf=m.from;bt=m.to;}alpha=max(alpha,val);if(alpha>=beta)break;
  }
  e={key,depth,best,best<=orig?2:(best>=beta?1:0),bf,bt};return best;
 }
};
struct Result{Move best;int depth=0,score=0;uint64_t nodes=0;};
static Result choose(Board b,int side,int milliseconds=2900,int threads=4){
 auto legal=moves(b,side);if(legal.empty())return {};
 if(legal.size()==1)return {legal[0],1,0,0};
 auto deadline=Clock::now()+chrono::milliseconds(max(50,min(milliseconds,2900)));
 Result result{legal[0],0,0,0};
 // Parallel root splitting: each worker owns its TT; completed iterations only are committed.
 for(int depth=1;depth<=64&&Clock::now()<deadline;depth++){
  atomic<size_t> index{0};atomic<bool> abort{false};
  vector<int> scores(legal.size(),-10000000);vector<uint64_t> counts(max(1,threads),0);
  vector<thread> pool;
  int workers=min((int)legal.size(),max(1,min(threads,4)));
  for(int t=0;t<workers;t++)pool.emplace_back([&,t]{Search s;s.deadline=deadline;while(!abort.load()){
   size_t k=index.fetch_add(1);if(k>=legal.size())break;
   if(Clock::now()>=deadline){abort=true;break;}
   int v=-s.negamax(applyMove(b,legal[k]),-side,depth-1,-10000000,10000000,1);
   if(s.stop.load()){abort=true;break;}scores[k]=v;
  }counts[t]=s.nodes.load();});
  for(auto &th:pool)th.join();
  if(abort.load()||Clock::now()>=deadline)break;
  size_t best=0;for(size_t i=1;i<scores.size();i++)if(scores[i]>scores[best])best=i;
  result.best=legal[best];result.score=scores[best];result.depth=depth;
  for(auto n:counts)result.nodes+=n;
  rotate(legal.begin(),legal.begin()+best,legal.begin()+best+1);
 }
 return result;
}

namespace TrapMaster {
constexpr int INF=1000000, MATE=100000, MAXPLY=96;
static uint64_t splitmix(uint64_t &x){x+=0x9e3779b97f4a7c15ULL;uint64_t z=x;z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;z=(z^(z>>27))*0x94d049bb133111ebULL;return z^(z>>31);}
static uint64_t positionKey(const Board&b,int side){
 static const auto random=[](){array<array<uint64_t,5>,100> a{};uint64_t seed=0xDADA1506;
 for(auto &sq:a)for(auto &v:sq)v=splitmix(seed);return a;}();
 uint64_t h=(side==1?0x8a77793fb57a8985ULL:0xdfff1812ee3828deULL)^uint64_t(b.n);
 for(int i=0;i<b.n*b.n;i++)if(b.a[i])h^=random[i][b.a[i]+2];return h;
}
static uint64_t moveKey(const Move&m){
 uint64_t h=uint64_t(m.from+1)*101+uint64_t(m.to+1);
 for(int c:m.caps)h=(h^uint64_t(c+1))*1099511628211ULL;
 for(int p:m.path)h=(h^uint64_t(p+1))*1099511628211ULL;return h;
}
static int evaluate(const Board&b,int side){
 int score=0;
 for(int i=0;i<b.n*b.n;i++){
  const int p=b.a[i];if(!p)continue;const int r=i/b.n,c=i%b.n,sg=p>0?1:-1;
  const int centre=min(c,b.n-1-c)+min(r,b.n-1-r);
  int v=abs(p)==2?380:100;
  if(abs(p)==1){
   const int advance=sg>0?b.n-1-r:r;
   v+=advance*3+centre*2;
   if(advance==0)v+=8; // Preserve the back rank until development is useful.
   if(advance==b.n-2)v+=12;
  } else {
   v+=centre*4;
   int freedom=0;
   for(int d=0;d<4;d++){int rr=r+DR[d],cc=c+DC[d];
    while(inside(rr,cc,b.n)&&!b.a[rr*b.n+cc]){freedom++;rr+=DR[d];cc+=DC[d];}
   }
   v+=min(freedom,18)*2;
  }
  int friends=0;
  for(int d=0;d<4;d++){int rr=r+DR[d],cc=c+DC[d];
   if(inside(rr,cc,b.n)&&b.a[rr*b.n+cc]&&(b.a[rr*b.n+cc]>0)==(p>0))friends++;
  }
  v+=min(friends,2)*4;score+=sg*v;
 }
 return score*side;
}
struct TableEntry {uint64_t key=0,move=0;int score=0;int16_t depth=-100;uint8_t flag=0;};
struct TacticalSearch {
 vector<TableEntry> table;
 array<array<uint64_t,2>,MAXPLY> killers{};
 int history[2][100][100]{};
 array<uint64_t,MAXPLY+1> line{};
 Clock::time_point deadline;uint64_t nodes=0;bool stopped=false;
 TacticalSearch(Clock::time_point end):table(1<<18),deadline(end){}
 bool timeUp(){++nodes;if(stopped)return true;if((nodes&127)==0&&Clock::now()>=deadline)stopped=true;return stopped;}
 static int storeScore(int s,int ply){return s>MATE-MAXPLY?s+ply:(s<-MATE+MAXPLY?s-ply:s);}
 static int loadScore(int s,int ply){return s>MATE-MAXPLY?s-ply:(s<-MATE+MAXPLY?s+ply:s);}
 int orderValue(const Board&b,const Move&m,int side,uint64_t ttmove,int ply){
  const auto key=moveKey(m);if(key==ttmove)return 2000000;
  int v=int(m.caps.size())*10000;
  for(int c:m.caps)if(abs(b.a[c])==2)v+=4000;
  if(abs(b.a[m.from])==1&&(m.to/b.n==0||m.to/b.n==b.n-1))v+=5000;
  if(m.caps.empty()){
   if(killers[ply][0]==key)v+=3500;else if(killers[ply][1]==key)v+=3000;
   v+=history[side>0?0:1][m.from][m.to];
   v+=min(m.to%b.n,b.n-1-m.to%b.n)*3;
  }
  return v;
 }
 int search(const Board&b,int side,int depth,int alpha,int beta,int ply,int extensions,bool previousQuiet){
  if(timeUp())return evaluate(b,side);
  if(ply>=MAXPLY-1)return evaluate(b,side);
  const uint64_t key=positionKey(b,side);
  int occurrences=0;for(int i=0;i<ply;i++)if(line[i]==key)occurrences++;
  if(occurrences>=2)return 0; // Repetition inside this search line only.
  line[ply]=key;
  auto legal=moves(b,side);
  if(legal.empty())return -MATE+ply;
  const bool forced=!legal[0].caps.empty();
  // Follow sacrifices into the forced reply and counterattack, with a bounded extension budget.
  if(forced&&previousQuiet&&depth>0&&extensions>0){++depth;--extensions;}
  if(depth<=0&&!forced)return evaluate(b,side);
  // Forced captures continue through quiescence until a quiet position.
  const uint64_t ttkey=key^(uint64_t(extensions)*0x94114ff0e9535d6dULL);
  auto &slot=table[ttkey&(table.size()-1)];
  const TableEntry cached=slot;
  const int originalAlpha=alpha;
  uint64_t ttmove=cached.key==ttkey?cached.move:0;
  const int requiredDepth=max(depth,0);
  if(!occurrences&&cached.key==ttkey&&cached.depth>=requiredDepth){
   const int val=loadScore(cached.score,ply);
   if(cached.flag==0)return val;
   if(cached.flag==1&&val>=beta)return val;
   if(cached.flag==2&&val<=alpha)return val;
  }
  vector<pair<int,size_t>> order;order.reserve(legal.size());
  for(size_t i=0;i<legal.size();i++)order.push_back({orderValue(b,legal[i],side,ttmove,ply),i});
  stable_sort(order.begin(),order.end(),[](auto a,auto b){return a.first>b.first;});
  int best=-INF;uint64_t bestMove=0;bool first=true;
  for(auto [priority,idx]:order){
   const auto &m=legal[idx];const auto next=applyMove(b,m);int val;
   if(first)val=-search(next,-side,depth-1,-beta,-alpha,ply+1,extensions,m.caps.empty());
   else{
    val=-search(next,-side,depth-1,-alpha-1,-alpha,ply+1,extensions,m.caps.empty());
    if(!stopped&&val>alpha&&val<beta)val=-search(next,-side,depth-1,-beta,-alpha,ply+1,extensions,m.caps.empty());
   }
   if(stopped)return 0;first=false;
   if(val>best){best=val;bestMove=moveKey(m);}alpha=max(alpha,val);
   if(alpha>=beta){
    if(m.caps.empty()){
     const auto mk=moveKey(m);if(killers[ply][0]!=mk){killers[ply][1]=killers[ply][0];killers[ply][0]=mk;}
     auto &h=history[side>0?0:1][m.from][m.to];h=min(2000,h+max(depth,1)*max(depth,1));
    }break;
   }
  }
  if(!occurrences)slot={ttkey,bestMove,storeScore(best,ply),(int16_t)requiredDepth,(uint8_t)(best<=originalAlpha?2:(best>=beta?1:0))};
  return best;
 }
};
static int offeredPieces(const Board&b,int side,const Move&m){
 if(!m.caps.empty())return 0;
 auto replies=moves(applyMove(b,m),-side);
 if(replies.empty()||replies[0].caps.empty())return 0;
 auto before=moves(b,-side);
 if(!before.empty()&&before[0].caps.size()>=replies[0].caps.size())return 0;
 return int(replies[0].caps.size());
}
static Result choose(const Board&b,int side,int milliseconds){
 const auto deadline=Clock::now()+chrono::milliseconds(clamp(milliseconds,50,2900));
 auto legal=moves(b,side);if(legal.empty())return {};
 TacticalSearch search(deadline);
 vector<int> bait;for(const auto &m:legal)bait.push_back(offeredPieces(b,side,m));
 vector<size_t> order;for(size_t i=0;i<legal.size();i++)order.push_back(i);
 // Search offers early; they still have to survive minimax, and receive no artificial score bonus.
 stable_sort(order.begin(),order.end(),[&](size_t a,size_t c){return bait[a]>bait[c];});
 Result completed{legal[0],0,evaluate(b,side),0};
 for(int depth=1;depth<=64&&Clock::now()<deadline;depth++){
  search.line[0]=positionKey(b,side);
  int best=-INF;size_t bestIdx=order[0];int alpha=-INF;
  for(size_t idx:order){
   if(Clock::now()>=deadline){search.stopped=true;break;}
   const auto &m=legal[idx];
   const int score=-search.search(applyMove(b,m),-side,depth-1,-INF,-alpha,1,2,m.caps.empty());
   if(search.stopped)break;
   if(score>best){best=score;bestIdx=idx;}alpha=max(alpha,score);
  }
  if(search.stopped)break;
  completed={legal[bestIdx],depth,best,search.nodes};
  auto it=find(order.begin(),order.end(),bestIdx);rotate(order.begin(),it,it+1);
  if(best>MATE-MAXPLY||best<-MATE+MAXPLY)break;
 }
 completed.nodes=search.nodes;
 return completed;
}
} // namespace TrapMaster

static string solve(const string&input){
 istringstream ss(input);Board b;int side=0,ms=2900,threads=4;
 if(!(ss>>b.n>>side>>ms>>threads)||!(b.n==8||b.n==10)||abs(side)!=1)return "ERROR invalid header";
 for(int i=0;i<b.n*b.n;i++){int v;if(!(ss>>v)||v<-2||v>2)return "ERROR invalid board";b.a[i]=v;}
 b.bits();if(moves(b,side).empty())return "ERROR no legal moves";auto r=threads==6?TrapMaster::choose(b,side,ms):choose(b,side,ms,threads);
 ostringstream out;out<<r.best.from<<' '<<r.best.to<<' '<<r.best.caps.size();for(int x:r.best.caps)out<<' '<<x;
 out<<' '<<r.best.path.size();for(int x:r.best.path)out<<' '<<x;
 out<<' '<<r.depth<<' '<<r.score<<' '<<r.nodes;
 int offered=threads==6?TrapMaster::offeredPieces(b,side,r.best):0;
 // A searched sacrifice is a tactical estimate, never a claim of exhaustive proof.
 out<<' '<<(offered?(r.score>0?1:2):0)<<' '<<offered;
 return out.str();
}
#ifndef DAMA_JNI
int main(){string input((istreambuf_iterator<char>(cin)),istreambuf_iterator<char>());cout<<solve(input)<<'\n';return 0;}
#else
#include <jni.h>
extern "C" JNIEXPORT jstring JNICALL Java_com_maro_dama15_MainActivity_nativeSolve(JNIEnv*env,jobject,jstring str){const char*p=env->GetStringUTFChars(str,nullptr);string result=solve(p);env->ReleaseStringUTFChars(str,p);return env->NewStringUTF(result.c_str());}
#endif
