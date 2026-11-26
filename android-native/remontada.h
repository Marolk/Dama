// Included after the existing board/rule engine, also shared by the WASM build.
// No material handicap: style NEVER changes the minimax evaluation.
namespace Remontada {
constexpr int INF = TrapMaster::INF, MATE = TrapMaster::MATE, MAXPLY = 96;
struct Timeout {};
struct Style {
 int offered = 0, counter = 0, smallArmy = 0, promotion = 0;
 bool operator<(const Style& b) const {
  if (offered != b.offered) return offered < b.offered;
  if (counter != b.counter) return counter < b.counter;
  if (smallArmy != b.smallArmy) return smallArmy < b.smallArmy;
  return promotion < b.promotion;
 }
};
static int countPieces(const Board& b, int side) {
 int n=0; for(int i=0;i<b.n*b.n;i++) n += b.a[i]*side>0; return n;
}
struct Candidate { Move move; int score; Style style; };
struct Analysis { Result result; vector<Candidate> candidates; uint64_t extensions=0; };
struct OrderEntry { uint64_t key=0, move=0; };
class Session {
 vector<OrderEntry> orderTable = vector<OrderEntry>(1<<17);
 array<uint64_t,MAXPLY> line{};
 array<array<uint64_t,2>,MAXPLY> killers{};
 int history[2][100][100]{};
 Clock::time_point deadline;
public:
 uint64_t nodes=0, extensions=0;
 explicit Session(Clock::time_point end):deadline(end){}
 void check() { if(Clock::now()>=deadline) throw Timeout{}; }
 int evaluate(const Board& b,int side) const {return TrapMaster::evaluate(b,side);}
 int ordering(const Board& b,const Move& m,int side,int ply,uint64_t preferred) {
  const auto key=TrapMaster::moveKey(m);
  if(key==preferred)return 2000000;
  int score=int(m.caps.size())*10000;
  for(int c:m.caps)if(abs(b.a[c])==2)score+=4000;
  if(abs(b.a[m.from])==1&&(m.to/b.n==0||m.to/b.n==b.n-1))score+=6000;
  if(key==killers[ply][0])score+=3500;
  if(key==killers[ply][1])score+=3000;
  return score+history[side>0][m.from][m.to];
 }
 int search(const Board& b,int side,int depth,int alpha,int beta,int ply,
            int extra,bool previousQuiet,bool promoted) {
  if((++nodes&63)==0)check();
  const auto key=TrapMaster::positionKey(b,side);
  int repeats=0;for(int i=0;i<ply;i++)repeats+=line[i]==key;
  if(repeats>=2)return 0;
  auto legal=moves(b,side);
  if(legal.empty())return -MATE+ply;
  if(ply>=MAXPLY-1)return evaluate(b,side);
  line[ply]=key;
  const bool forced=!legal[0].caps.empty();
  // Additional depth after offers/promotions, bounded along each branch.
  if(extra>0 && depth>0 && ((forced&&previousQuiet)||promoted)) {
   ++depth;--extra;++extensions;
  }
  // No stand-pat during compulsory captures. No null-move, futility or LMR.
  if(depth<=0&&!forced)return evaluate(b,side);
  auto& slot=orderTable[key&(orderTable.size()-1)];
  const auto preferred=slot.key==key?slot.move:0;
  vector<pair<int,size_t>> ordered;ordered.reserve(legal.size());
  for(size_t i=0;i<legal.size();i++)ordered.push_back({ordering(b,legal[i],side,ply,preferred),i});
  stable_sort(ordered.begin(),ordered.end(),[](auto a,auto c){return a.first>c.first;});
  int best=-INF;uint64_t bestMove=0;bool first=true;
  for(auto [priority,index]:ordered) {
   const auto& m=legal[index];const Board child=applyMove(b,m);
   const bool crown=abs(b.a[m.from])==1&&abs(child.a[m.to])==2;
   int score;
   if(first)score=-search(child,-side,depth-1,-beta,-alpha,ply+1,extra,m.caps.empty(),crown);
   else {
    score=-search(child,-side,depth-1,-alpha-1,-alpha,ply+1,extra,m.caps.empty(),crown);
    if(score>alpha&&score<beta)score=-search(child,-side,depth-1,-beta,-alpha,ply+1,extra,m.caps.empty(),crown);
   }
   first=false;
   if(score>best){best=score;bestMove=TrapMaster::moveKey(m);}
   alpha=max(alpha,score);
   if(alpha>=beta) {
    if(m.caps.empty()) {
     if(killers[ply][0]!=bestMove){killers[ply][1]=killers[ply][0];killers[ply][0]=bestMove;}
     auto& h=history[side>0][m.from][m.to];h=min(4000,h+max(1,depth*depth));
    }
    break;
   }
  }
  // Ordering only: never reuse scores across different repetition histories.
  slot={key,bestMove};return best;
 }
 Style style(const Board& b,int side,const Move& m) {
  check();Style s;const auto child=applyMove(b,m);
  s.promotion=abs(b.a[m.from])==1&&abs(child.a[m.to])==2;
  const auto replies=moves(child,-side);
  if(!m.caps.empty()||replies.empty()||replies[0].caps.empty())return s;
  const auto oldReplies=moves(b,-side);
  const int oldOffers=oldReplies.empty()?0:int(oldReplies[0].caps.size());
  s.offered=max(0,int(replies[0].caps.size())-oldOffers);
  if(!s.offered)return s;
  int minCounter=100,minSmall=1;
  // The tie-break uses the WORST reply, not the most cooperative opponent.
  for(const auto& r:replies) {
   check();const auto after=applyMove(child,r);const int left=countPieces(after,side);
   minSmall=min(minSmall,int(left>=1&&left<=4));
   const auto counters=moves(after,side);
   minCounter=min(minCounter,counters.empty()?0:int(counters[0].caps.size()));
  }
  s.counter=minCounter;s.smallArmy=minSmall;return s;
 }
 Analysis run(const Board& b,int side,int maxDepth=64,bool preferStyle=true) {
  Analysis answer;auto legal=moves(b,side);if(legal.empty())return answer;
  answer.result={legal[0],0,evaluate(b,side),0};
  vector<Style> styles(legal.size());
  // Extra endgame work is earned by the actual position, never by throwing pieces away.
  const int extra=countPieces(b,side)<=4?4:3;
  try {
   for(size_t i=0;i<legal.size();i++)styles[i]=style(b,side,legal[i]);
   for(int depth=1;depth<=maxDepth;depth++) {
    check();line[0]=TrapMaster::positionKey(b,side);
    vector<Candidate> iteration;iteration.reserve(legal.size());
    int best=-INF;size_t bestIndex=0;
    for(size_t i=0;i<legal.size();i++) {
     check();const auto& m=legal[i];const auto child=applyMove(b,m);
     const bool crown=abs(b.a[m.from])==1&&abs(child.a[m.to])==2;
     // Full window for EVERY root: a fail-low bound cannot qualify for a style tie.
     const int score=-search(child,-side,depth-1,-INF,INF,1,extra,m.caps.empty(),crown);
     iteration.push_back({m,score,styles[i]});
     const bool equalSafe=preferStyle&&score==best&&score>=0&&depth>=4;
     if(score>best||(equalSafe&&styles[bestIndex]<styles[i])) {best=score;bestIndex=i;}
    }
    check(); // Commit only a complete, equal-depth comparison of all legal roots.
    answer.result={legal[bestIndex],depth,best,nodes};answer.candidates=std::move(iteration);
    std::rotate(legal.begin(),legal.begin()+bestIndex,legal.begin()+bestIndex+1);
    std::rotate(styles.begin(),styles.begin()+bestIndex,styles.begin()+bestIndex+1);
    if(abs(best)>MATE-MAXPLY)break;
   }
  }catch(const Timeout&) { /* Keep the last fully verified iteration. */ }
  answer.result.nodes=nodes;answer.extensions=extensions;return answer;
 }
};
static Analysis choose(const Board& b,int side,int milliseconds,int maxDepth=64) {
 Session s(Clock::now()+chrono::milliseconds(clamp(milliseconds,50,2900)));
 return s.run(b,side,maxDepth);
}
} // namespace Remontada
