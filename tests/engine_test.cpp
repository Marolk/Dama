#include "../src/engine.cpp"
#include <cassert>
#include <iostream>
using namespace dama;
Position initial(int n){Position p;p.n=n;for(int r=0;r<n;r++)for(int c=0;c<n;c++)if((r+c)%2){if(r<3)p.b[r*n+c]=-1;if(r>=n-3)p.b[r*n+c]=1;}return p;}
int main(){
 for(int n:{8,10}){auto p=initial(n);assert(std::count(p.b.begin(),p.b.end(),1)==(n==8?12:15));assert(legal(p).size()==size_t(n-1));}
 Position p;p.n=8;p.b[5*8+2]=1;assert(legal(p).size()==2);
 p.b[4*8+3]=-1;auto m=legal(p);assert(m.size()==1&&m[0].caps.size()==1&&m[0].to==3*8+4);
 p.b[2*8+5]=-1;m=legal(p);assert(m.size()==1&&m[0].caps.size()==2&&m[0].to==1*8+6);
 p.b[5*8+6]=1;p.b[4*8+7]=-1;for(auto&x:legal(p))assert(x.caps.size()==2);
 p={};p.n=8;p.b[3*8+2]=1;p.b[4*8+3]=-1;for(auto&x:legal(p))assert(x.caps.empty());
 p={};p.n=8;p.b[5*8+2]=2;p.b[3*8+4]=-1;m=legal(p);assert(m.size()==3);for(auto&x:m)assert(x.caps.size()==1);
 // The captured enemy blocks reversing through it to the other enemy.
 p={};p.n=8;p.b[3*8+3]=2;p.b[2*8+2]=-1;p.b[4*8+4]=-1;for(auto&x:legal(p))assert(x.caps.size()==1);
 p={};p.n=8;p.b[2*8+1]=1;p.b[1*8+2]=-1;p.b[1*8+4]=-1;m=legal(p);assert(m.size()==1&&m[0].caps.size()==1);assert(apply(p,m[0]).b[3]==2);
 p={};p.n=8;assert(legal(p).empty());p.b[0]=1;assert(legal(p).empty());
 p=initial(10);Search repetition;auto draw=repetition.run(p,50,3,{hash(p),hash(p)});assert(draw.draw);
 Search s;auto r=s.run(p,100,30);assert(r.ms<600);auto lm=legal(p);assert(std::find(lm.begin(),lm.end(),r.best)!=lm.end());assert(r.depth>0);
 // Exhaustively check every forced reply in this three-ply sacrifice fixture.
 p={};p.n=8;p.b[12]=-1;p.b[24]=-1;p.b[26]=1;p.b[28]=1;p.b[37]=-1;p.b[46]=1;
 Move sacrifice;for(auto&m:legal(p))if(m.from==28&&m.to==19)sacrifice=m;
 assert(sacrifice.from==28);auto offered=apply(p,sacrifice);auto replies=legal(offered);assert(!replies.empty());
 for(auto&reply:replies){assert(!reply.caps.empty());auto after=apply(offered,reply);int best=-10000;for(auto&counter:legal(after))best=std::max(best,material(apply(after,counter)));assert(best>material(p));}
 Search tactic;auto tactical=tactic.run(p,1000,6);assert(tactical.best==sacrifice);
 std::cout<<"Sacrifice fixture: all immediate forced replies allow a favorable counter; not a full-game proof.\n";
 assert(std::string(dama_request("garbage")).find("error")!=std::string::npos);
 std::cout<<"Native rules / repetition / budget passed. 100 ms search: "<<r.ms<<" ms, depth "<<r.depth<<", nodes "<<r.nodes<<'\n';
}
