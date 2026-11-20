#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
namespace dama {
using Clock=std::chrono::steady_clock;
struct Move { int from=0,to=0; std::vector<int> caps,path; bool operator==(const Move&) const=default; };
struct Position { int n=10,side=1; std::array<int,100> b{}; };
struct Timeout {};
struct Guard { bool active=false; Clock::time_point end; uint64_t ticks=0; void check(){if(active && (++ticks%64==0) && Clock::now()>=end) throw Timeout{};} };
thread_local Guard guard;
bool inside(int r,int c,int n){return r>=0&&c>=0&&r<n&&c<n;}
void captures(Position& p,int at,int piece,Move& m,std::array<bool,100>& taken,std::vector<Move>& out){
 guard.check(); bool extended=false; int r=at/p.n,c=at%p.n;
 for(int dr:{-1,1}) for(int dc:{-1,1}) {
  if(std::abs(piece)==1&&dr!=(piece>0?-1:1))continue;
  int rr=r+dr,cc=c+dc;
  if(std::abs(piece)==2)while(inside(rr,cc,p.n)&&!p.b[rr*p.n+cc]){rr+=dr;cc+=dc;}
  if(!inside(rr,cc,p.n))continue; int victim=rr*p.n+cc;
  if(p.b[victim]*piece>=0||taken[victim])continue;
  rr+=dr;cc+=dc;
  while(inside(rr,cc,p.n)&&!p.b[rr*p.n+cc]){
   extended=true; int land=rr*p.n+cc; p.b[at]=0;p.b[land]=piece;taken[victim]=true;
   m.caps.push_back(victim);m.path.push_back(land);m.to=land;
   captures(p,land,piece,m,taken,out);
   m.path.pop_back();m.caps.pop_back();taken[victim]=false;p.b[land]=0;p.b[at]=piece;m.to=at;
   if(std::abs(piece)==1)break; rr+=dr;cc+=dc;
  }
 }
 if(!extended&&!m.caps.empty())out.push_back(m);
}
std::vector<Move> legal(Position p){
 std::vector<Move> out;std::array<bool,100> taken{};
 for(int i=0;i<p.n*p.n;i++)if(p.b[i]*p.side>0){Move m;m.from=m.to=i;captures(p,i,p.b[i],m,taken,out);}
 if(!out.empty()){size_t mx=0;for(auto&m:out)mx=std::max(mx,m.caps.size());std::erase_if(out,[&](auto&m){return m.caps.size()!=mx;});return out;}
 for(int i=0;i<p.n*p.n;i++)if(p.b[i]*p.side>0)for(int dr:{-1,1})for(int dc:{-1,1}){
  if(std::abs(p.b[i])==1&&dr!=(p.side>0?-1:1))continue;
  int r=i/p.n+dr,c=i%p.n+dc;
  while(inside(r,c,p.n)&&!p.b[r*p.n+c]){Move m;m.from=i;m.to=r*p.n+c;m.path.push_back(m.to);out.push_back(m);if(std::abs(p.b[i])==1)break;r+=dr;c+=dc;}
 }return out;
}
Position apply(Position p,const Move&m){int piece=p.b[m.from];p.b[m.from]=0;for(int c:m.caps)p.b[c]=0;if(piece==1&&m.to/p.n==0)piece=2;if(piece==-1&&m.to/p.n==p.n-1)piece=-2;p.b[m.to]=piece;p.side=-p.side;return p;}
uint64_t mix(uint64_t x){x+=0x9e3779b97f4a7c15ULL;x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;x=(x^(x>>27))*0x94d049bb133111ebULL;return x^(x>>31);}
uint64_t hash(const Position&p){uint64_t h=mix(p.n*3+p.side);for(int i=0;i<p.n*p.n;i++)if(p.b[i])h^=mix(1000+i*5+p.b[i]+2);return h;}
int material(const Position&p){int s=0;for(int x:p.b)s+=(x>0?1:-1)*(std::abs(x)==2?330:std::abs(x)==1?100:0);return s;}
int evaluate(const Position&p){int s=material(p);for(int i=0;i<p.n*p.n;i++)if(p.b[i]){int sign=p.b[i]>0?1:-1;int advance=sign>0?p.n-1-i/p.n:i/p.n;s+=sign*((std::abs(p.b[i])==1?advance*4:0)+int(p.n-std::abs(2*(i%p.n)-p.n+1)));}return s*p.side;}
struct Entry {uint64_t key=0;int depth=-1,score=0,flag=0;Move move;};
struct Result {Move best;std::vector<std::pair<int,Move>> plans;int depth=0,score=0;uint64_t nodes=0;double ms=0;bool draw=false;};
struct Search {
 std::vector<Entry> tt=std::vector<Entry>(1<<17);std::array<std::array<Move,2>,128> killers{};int history[2][100][100]{};std::vector<uint64_t> past;uint64_t nodes=0;
 bool repetition(uint64_t h){return std::count(past.begin(),past.end(),h)>=2;}
 int ordering(const Move&m,int ply,const Move&best,int side){int k=(m==best?1000000:0)+int(m.caps.size())*10000;if(ply<128){if(m==killers[ply][0])k+=9000;if(m==killers[ply][1])k+=8000;}return k+history[side>0][m.from][m.to];}
 int search(Position p,int depth,int a,int b,int ply,int q=20){
  guard.check();nodes++;uint64_t h=hash(p);if(repetition(h))return 0;
  auto moves=legal(p);if(moves.empty())return -30000+ply;
  if(ply>=100||(depth<=0&&(moves[0].caps.empty()||q<=0)))return evaluate(p);
  // Repetition depends on path: TT is ordering-only, never a context-free cutoff.
  auto &entry=tt[h%tt.size()];Move preferred=entry.key==h?entry.move:Move{};
  std::stable_sort(moves.begin(),moves.end(),[&](auto&x,auto&y){return ordering(x,ply,preferred,p.side)>ordering(y,ply,preferred,p.side);});
  past.push_back(h);int best=-32000,original=a;Move bm=moves[0];
  try{for(size_t i=0;i<moves.size();i++){auto&m=moves[i];int score;
   if(i==0)score=-search(apply(p,m),depth-1,-b,-a,ply+1,q-(depth<=0));
   else {score=-search(apply(p,m),depth-1,-a-1,-a,ply+1,q-(depth<=0));if(score>a&&score<b)score=-search(apply(p,m),depth-1,-b,-a,ply+1,q-(depth<=0));}
   if(score>best){best=score;bm=m;}a=std::max(a,score);
   if(a>=b){if(m.caps.empty()&&ply<128){killers[ply][1]=killers[ply][0];killers[ply][0]=m;auto&v=history[p.side>0][m.from][m.to];v=std::min(7000,v+std::max(1,depth*depth));}break;}
  }}catch(...){past.pop_back();throw;}past.pop_back();entry={h,depth,best,best<=original?-1:best>=b?1:0,bm};return best;
 }
 Result run(Position p,int milliseconds,int maxDepth,std::vector<uint64_t> previous={}){
  Result result;past=std::move(previous);auto start=Clock::now();guard={false,start,0};auto moves=legal(p);result.draw=repetition(hash(p));if(moves.empty()||result.draw)return result;result.best=moves[0];guard={true,start+std::chrono::milliseconds(std::clamp(milliseconds,1,2800)),0};
  for(int d=1;d<=maxDepth;d++){std::vector<std::pair<int,Move>> scores;try{past.push_back(hash(p));for(auto&m:moves){int s=-search(apply(p,m),d-1,-32000,32000,1);scores.push_back({s,m});}past.pop_back();}catch(Timeout&){if(!past.empty()&&past.back()==hash(p))past.pop_back();break;}
   std::stable_sort(scores.begin(),scores.end(),[](auto&a,auto&b){return a.first>b.first;});result.best=scores[0].second;result.score=scores[0].first;result.depth=d;result.plans=scores;for(size_t i=0;i<moves.size();i++)moves[i]=scores[i].second;
   if(Clock::now()>=guard.end)break;
  }guard.active=false;result.nodes=nodes;result.ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();return result;
 }
};
std::string moveJSON(const Move&m,int n){std::ostringstream o;o<<"{\"fromRow\":"<<m.from/n<<",\"fromCol\":"<<m.from%n<<",\"toRow\":"<<m.to/n<<",\"toCol\":"<<m.to%n<<",\"captures\":[";for(size_t i=0;i<m.caps.size();i++){if(i)o<<',';o<<"{\"r\":"<<m.caps[i]/n<<",\"c\":"<<m.caps[i]%n<<'}';}o<<"],\"steps\":[";for(size_t i=0;i<m.caps.size();i++){if(i)o<<',';o<<"{\"capR\":"<<m.caps[i]/n<<",\"capC\":"<<m.caps[i]%n<<",\"landR\":"<<m.path[i]/n<<",\"landC\":"<<m.path[i]%n<<'}';}o<<"],\"path\":[";for(size_t i=0;i<m.path.size();i++){if(i)o<<',';o<<m.path[i];}o<<"]}";return o.str();}
}
extern "C" const char* dama_request(const char* input){
 static std::string output;using namespace dama;std::istringstream in(input?input:"");Position p;int version,op,ms,depth,count;
 if(!(in>>version>>op>>p.n>>p.side>>ms>>depth)||version!=1||(op!=0&&op!=1)||(p.n!=8&&p.n!=10)||std::abs(p.side)!=1)return "{\"error\":\"Invalid protocol header\"}";
 for(int i=0;i<p.n*p.n;i++)if(!(in>>p.b[i])||std::abs(p.b[i])>2)return "{\"error\":\"Invalid board\"}";
 std::vector<uint64_t> previous;if(!(in>>count)||count<0||count>10000)return "{\"error\":\"Invalid history\"}";for(int i=0;i<count;i++){uint64_t h;if(!(in>>h))return "{\"error\":\"Invalid history hash\"}";previous.push_back(h);}
 guard.active=false;auto moves=legal(p);std::ostringstream o;o<<"{\"version\":1,\"hash\":\""<<hash(p)<<"\",\"moves\":[";for(size_t i=0;i<moves.size();i++){if(i)o<<',';o<<moveJSON(moves[i],p.n);}o<<']';
 if(op==1){Search s;auto r=s.run(p,ms,std::clamp(depth,1,30),previous);o<<",\"best\":"<<(moves.empty()||r.draw?"null":moveJSON(r.best,p.n))<<",\"draw\":"<<(r.draw?"true":"false")<<",\"depth\":"<<r.depth<<",\"nodes\":"<<r.nodes<<",\"elapsedMs\":"<<r.ms<<",\"score\":"<<r.score<<",\"proof\":\"evaluation-only\",\"plans\":[";for(size_t i=0;i<std::min(size_t(4),r.plans.size());i++){if(i)o<<',';o<<"{\"score\":"<<r.plans[i].first<<",\"move\":"<<moveJSON(r.plans[i].second,p.n)<<'}';}o<<']';}o<<'}';output=o.str();return output.c_str();
}
#ifdef DAMA_CLI
#include <iostream>
int main(){std::string line;while(std::getline(std::cin,line))std::cout<<dama_request(line.c_str())<<'\n';}
#endif
