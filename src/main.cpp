#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>

namespace aquila {
using U64 = std::uint64_t;
using Score = int;
constexpr Score INF = 32000;
constexpr Score MATE = 30000;
constexpr int MAX_PLY = 128;
constexpr Score MATE_THRESHOLD = MATE - MAX_PLY;

inline bool is_mate_score(Score s){ return s > MATE_THRESHOLD || s < -MATE_THRESHOLD; }
inline Score score_to_tt(Score s,int ply){
    if(s>MATE_THRESHOLD)return s+ply;
    if(s<-MATE_THRESHOLD)return s-ply;
    return s;
}
inline Score score_from_tt(Score s,int ply){
    if(s>MATE_THRESHOLD)return s-ply;
    if(s<-MATE_THRESHOLD)return s+ply;
    return s;
}
inline int mate_moves_from_score(Score s){
    if(s>MATE_THRESHOLD)return (MATE-s+1)/2;
    return -((MATE+s+1)/2);
}
inline std::string uci_score(Score s){
    return is_mate_score(s) ? "score mate "+std::to_string(mate_moves_from_score(s))
                            : "score cp "+std::to_string(s);
}
constexpr U64 BB_ALL = ~U64(0);

enum Color : int { WHITE=0, BLACK=1 };
enum PieceType : int { NONE=0, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };
enum Piece : int {
    EMPTY=0,
    WP=1, WN, WB, WR, WQ, WK,
    BP=7, BN, BB, BR, BQ, BK
};
inline Color opp(Color c){ return Color(c ^ 1); }
inline PieceType type_of(Piece p){ return p==EMPTY?NONE:PieceType(((int)p-1)%6+1); }
inline Color color_of(Piece p){ return (p>=BP)?BLACK:WHITE; }
inline int piece_index(Piece p){ return int(p); }
inline int sq(int file,int rank){ return rank*8+file; }
inline int file_of(int s){ return s&7; }
inline int rank_of(int s){ return s>>3; }
inline U64 bit(int s){ return U64(1)<<s; }
inline int poplsb(U64 &b){ int s=__builtin_ctzll(b); b&=b-1; return s; }
inline int popcount(U64 b){ return __builtin_popcountll(b); }
inline std::string square_name(int s){ std::string r; r.push_back(char('a'+file_of(s))); r.push_back(char('1'+rank_of(s))); return r; }
inline int parse_square(const std::string&s){ return (s.size()>=2 && s[0]>='a'&&s[0]<='h'&&s[1]>='1'&&s[1]<='8') ? sq(s[0]-'a',s[1]-'1') : -1; }

struct Move {
    std::uint32_t data=0;
    // bits: from 0..5, to 6..11, promo 12..14, flag 15..17
    enum Flag { QUIET=0, DOUBLE_PUSH=1, CASTLE=2, ENPASSANT=3, PROMOTION=4 };
    static Move make(int from,int to,int promo=NONE,Flag flag=QUIET){
        Move m; m.data = from | (to<<6) | (promo<<12) | (flag<<15); return m;
    }
    int from()const{return data&63;} int to()const{return (data>>6)&63;}
    int promo()const{return (data>>12)&7;} Flag flag()const{return Flag((data>>15)&7);}
    bool operator==(const Move&o)const{return data==o.data;} bool operator!=(const Move&o)const{return data!=o.data;}
};
inline std::string move_uci(const Move&m){
    std::string s=square_name(m.from())+square_name(m.to());
    if(m.promo()){ static const char* q="?pnbrqk"; s.push_back(q[m.promo()]); }
    return s;
}

struct Zobrist {
    U64 piece[12][64]{}; U64 side{}; U64 castle[16]{}; U64 ep[64]{};
    static U64 splitmix(U64&x){ x += 0x9e3779b97f4a7c15ULL; U64 z=x; z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL; z=(z^(z>>27))*0x94d049bb133111ebULL; return z^(z>>31); }
    void init(){ U64 x=0x123456789abcdef0ULL; for(auto &a:piece) for(U64 &v:a)v=splitmix(x); side=splitmix(x); for(auto &v:castle)v=splitmix(x); for(auto &v:ep)v=splitmix(x); }
} Z;

U64 KnightAtt[64], KingAtt[64], PawnAtt[2][64];
U64 FileMask[8], RankMask[8];

struct MagicTable { U64 mask=0, magic=0; int shift=64; std::vector<U64> attacks; };
MagicTable RMag[64], BMag[64];
U64 rook_attacks_slow(int s,U64 occ);
U64 bishop_attacks_slow(int s,U64 occ);
U64 rook_attacks(int s,U64 occ);
U64 bishop_attacks(int s,U64 occ);

U64 rook_mask(int s){
    U64 m=0; int f=file_of(s), r=rank_of(s);
    for(int rr=r+1;rr<=6;rr++)m|=bit(sq(f,rr));
    for(int rr=r-1;rr>=1;rr--)m|=bit(sq(f,rr));
    for(int ff=f+1;ff<=6;ff++)m|=bit(sq(ff,r));
    for(int ff=f-1;ff>=1;ff--)m|=bit(sq(ff,r));
    return m;
}
U64 bishop_mask(int s){
    U64 m=0; int f=file_of(s), r=rank_of(s);
    for(int ff=f+1,rr=r+1;ff<=6&&rr<=6;ff++,rr++)m|=bit(sq(ff,rr));
    for(int ff=f-1,rr=r+1;ff>=1&&rr<=6;ff--,rr++)m|=bit(sq(ff,rr));
    for(int ff=f+1,rr=r-1;ff<=6&&rr>=1;ff++,rr--)m|=bit(sq(ff,rr));
    for(int ff=f-1,rr=r-1;ff>=1&&rr>=1;ff--,rr--)m|=bit(sq(ff,rr));
    return m;
}
U64 subset_from_index(int index,U64 mask){ U64 out=0; for(int i=0;mask;i++){U64 b=mask&-mask;mask-=b;if(index&(1<<i))out|=b;} return out; }
void init_magics(){
    static const U64 rook_magic[64] = { 0x1080004008801020, 0x840092002c03000, 0x1900200010400900, 0x880100008000480, 0x4200100420080200, 0x8100020100080400, 0x200040110886200, 0x200008040220411, 0x404800084400220, 0x401000402000, 0x86001081220440, 0x408800800100280, 0xa001201040820, 0x8848800200840080, 0x4001000100040200, 0x442000102105084, 0x9080010020804100, 0x40404000201009, 0x808010002009, 0x2200090021d00100, 0x8008008040080, 0x4004002010040, 0x11040008015042, 0xa0001768104, 0x800080204009, 0x2010004140002001, 0x9800200280100080, 0x1000100080080080, 0x442000a00049020, 0x2100040080020080, 0x800120400900148, 0x10040a00128541, 0x2800804000800030, 0x1010002000400041, 0x4000200011004100, 0x610008410800800, 0x400802402800800, 0xc100020080800400, 0x2000802000401, 0x182085882000401, 0x220204000808000, 0x2860100040024022, 0x1002004110040, 0x99101042000a0020, 0x4080004008080, 0x10040002008080, 0x2012004881020004, 0x8300842444820011, 0x88403882010200, 0x820400080210100, 0x110910040a00300, 0x801100280080480, 0x242009008200600, 0x1002000489500200, 0x40800200010080, 0x91800041000080, 0x209300488001, 0x4c1002414824001, 0x20020000b001041, 0x7000100004200901, 0x8002002004100802, 0x30010002084c0007, 0x888221800813004, 0x4000002840840112 };
    static const U64 bishop_magic[64] = { 0xa010041108003100, 0x6082020a002900, 0x6810010619200000, 0x8281a0520000408, 0x1104001000400, 0x18901008048400, 0x40a0210245280, 0x200210808a402, 0x9140048410821200, 0x800091010820041, 0x20504804832202c0, 0x100091401081000, 0x8021011140000012, 0x810020804450400, 0x208b0542109008a2, 0x80084a08040204, 0x40e2a80811244c, 0x2505022008008108, 0x430220100420040, 0x10a040420220040, 0x1105000290400000, 0x93001200822120, 0x4000a62048043004, 0x280120048a015004, 0x6090002a020814, 0x44042000240800d0, 0x1102800040a4400, 0x1004080080220040, 0x1001011004024, 0x10044000805040, 0x914041200820100, 0x4821012821480, 0x24040500c05021, 0x88611002080200, 0x116080a00040020, 0x4000020080080080, 0x2450450140840040, 0x880201484100, 0x222020404020092, 0x8081110600002e00, 0x2842101105000801, 0x1100809008001025, 0x20202221c0400, 0x422014022009020, 0x210046102100c00, 0xc004008082029102, 0xaa461801101200, 0x404080080201108, 0x20542108c205002, 0x410544804100100, 0x40910841100000, 0x400200042021100, 0x4204850400c0, 0x200100410a42102, 0x1040020801210102, 0x805040410420000, 0x2884804130100200, 0x800c262201242000, 0x1058000194108800, 0x14221054420204, 0x104000012a02200, 0x200881003300100, 0x140400202840100, 0x402020801010201 };
    for(int s=0;s<64;s++){
        RMag[s].mask=rook_mask(s); RMag[s].magic=rook_magic[s]; RMag[s].shift=64-popcount(RMag[s].mask); int rn=1<<popcount(RMag[s].mask); RMag[s].attacks.assign(rn,0);
        for(int i=0;i<rn;i++){U64 occ=subset_from_index(i,RMag[s].mask);RMag[s].attacks[(occ*RMag[s].magic)>>RMag[s].shift]=rook_attacks_slow(s,occ);}
        BMag[s].mask=bishop_mask(s); BMag[s].magic=bishop_magic[s]; BMag[s].shift=64-popcount(BMag[s].mask); int bn=1<<popcount(BMag[s].mask); BMag[s].attacks.assign(bn,0);
        for(int i=0;i<bn;i++){U64 occ=subset_from_index(i,BMag[s].mask);BMag[s].attacks[(occ*BMag[s].magic)>>BMag[s].shift]=bishop_attacks_slow(s,occ);}
    }
    // Exhaustively validate every relevant occupancy against the reference ray generator.
    for(int s=0;s<64;s++){
        int rn=1<<popcount(RMag[s].mask); for(int i=0;i<rn;i++){U64 occ=subset_from_index(i,RMag[s].mask); if(rook_attacks(s,occ)!=rook_attacks_slow(s,occ)) throw std::runtime_error("rook magic self-test failed");}
        int bn=1<<popcount(BMag[s].mask); for(int i=0;i<bn;i++){U64 occ=subset_from_index(i,BMag[s].mask); if(bishop_attacks(s,occ)!=bishop_attacks_slow(s,occ)) throw std::runtime_error("bishop magic self-test failed");}
    }
}
inline U64 rook_attacks(int s,U64 occ){const auto&m=RMag[s];return m.attacks[((occ&m.mask)*m.magic)>>m.shift];}
inline U64 bishop_attacks(int s,U64 occ){const auto&m=BMag[s];return m.attacks[((occ&m.mask)*m.magic)>>m.shift];}

void init_attacks(){
    for(int r=0;r<8;r++) for(int f=0;f<8;f++) { FileMask[f]|=bit(sq(f,r)); RankMask[r]|=bit(sq(f,r)); }
    const int nd[8][2]={{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
    const int kd[8][2]={{1,1},{1,0},{1,-1},{0,1},{0,-1},{-1,1},{-1,0},{-1,-1}};
    for(int s=0;s<64;s++){
        int f=file_of(s),r=rank_of(s);
        for(auto&d:nd){int nf=f+d[0],nr=r+d[1];if(nf>=0&&nf<8&&nr>=0&&nr<8)KnightAtt[s]|=bit(sq(nf,nr));}
        for(auto&d:kd){int nf=f+d[0],nr=r+d[1];if(nf>=0&&nf<8&&nr>=0&&nr<8)KingAtt[s]|=bit(sq(nf,nr));}
        for(int df: {-1,1}) {int nf=f+df; if(nf>=0&&nf<8){ if(r<7)PawnAtt[WHITE][s]|=bit(sq(nf,r+1)); if(r>0)PawnAtt[BLACK][s]|=bit(sq(nf,r-1)); }}
    }
    Z.init();
    init_magics();
}

U64 rook_attacks_slow(int s,U64 occ){
    U64 a=0; int f=file_of(s),r=rank_of(s);
    for(int rr=r+1;rr<8;rr++){int x=sq(f,rr);a|=bit(x);if(occ&bit(x))break;}
    for(int rr=r-1;rr>=0;rr--){int x=sq(f,rr);a|=bit(x);if(occ&bit(x))break;}
    for(int ff=f+1;ff<8;ff++){int x=sq(ff,r);a|=bit(x);if(occ&bit(x))break;}
    for(int ff=f-1;ff>=0;ff--){int x=sq(ff,r);a|=bit(x);if(occ&bit(x))break;}
    return a;
}
U64 bishop_attacks_slow(int s,U64 occ){
    U64 a=0; int f=file_of(s),r=rank_of(s);
    for(int ff=f+1,rr=r+1;ff<8&&rr<8;ff++,rr++){int x=sq(ff,rr);a|=bit(x);if(occ&bit(x))break;}
    for(int ff=f-1,rr=r+1;ff>=0&&rr<8;ff--,rr++){int x=sq(ff,rr);a|=bit(x);if(occ&bit(x))break;}
    for(int ff=f+1,rr=r-1;ff<8&&rr>=0;ff++,rr--){int x=sq(ff,rr);a|=bit(x);if(occ&bit(x))break;}
    for(int ff=f-1,rr=r-1;ff>=0&&rr>=0;ff--,rr--){int x=sq(ff,rr);a|=bit(x);if(occ&bit(x))break;}
    return a;
}
U64 rook_attacks(int s,U64 occ);
U64 bishop_attacks(int s,U64 occ);
U64 queen_attacks(int s,U64 occ){return rook_attacks(s,occ)|bishop_attacks(s,occ);}

struct Undo { U64 key; int castle, ep, halfmove, fullmove; Piece captured; Move move; };

class Board {
public:
    Piece b[64]{}; U64 bb[12]{}; U64 occ[2]{}; U64 all=0; Color side=WHITE; int castle=15; int ep=-1; int halfmove=0; int fullmove=1; U64 key=0; std::vector<U64> history;
    static constexpr int WKCA=1,WQCA=2,BKCA=4,BQCA=8;
    Board(){ set_fen("startpos"); }
    void clear(){ std::fill(std::begin(b),std::end(b),EMPTY); std::fill(std::begin(bb),std::end(bb),0); occ[0]=occ[1]=all=0; side=WHITE; castle=0;ep=-1;halfmove=0;fullmove=1;key=0;history.clear(); }
    void put(int s,Piece p){ b[s]=p; if(p){bb[p-1]|=bit(s);occ[color_of(p)]|=bit(s);all|=bit(s);} }
    void remove(int s){ Piece p=b[s]; if(!p)return; bb[p-1]&=~bit(s);occ[color_of(p)]&=~bit(s);all&=~bit(s);b[s]=EMPTY; }
    U64 compute_key() const {U64 k=0;for(int s=0;s<64;s++)if(b[s])k^=Z.piece[b[s]-1][s]; if(side==BLACK)k^=Z.side; k^=Z.castle[castle];if(ep>=0)k^=Z.ep[ep];return k;}
    void set_fen(std::string fen){
        clear(); if(fen=="startpos") fen="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        std::istringstream is(fen); std::string placement,turn,cs,eps; if(!(is>>placement>>turn>>cs>>eps>>halfmove>>fullmove)) throw std::runtime_error("Invalid FEN");
        int r=7,f=0; for(char c:placement){if(c=='/'){--r;f=0;} else if(std::isdigit((unsigned char)c)) f+=c-'0'; else {Piece p=EMPTY;switch(c){case'P':p=WP;break;case'N':p=WN;break;case'B':p=WB;break;case'R':p=WR;break;case'Q':p=WQ;break;case'K':p=WK;break;case'p':p=BP;break;case'n':p=BN;break;case'b':p=BB;break;case'r':p=BR;break;case'q':p=BQ;break;case'k':p=BK;break;default:throw std::runtime_error("Invalid FEN piece");}put(sq(f,r),p);++f;}}
        side=(turn=="w"?WHITE:BLACK); castle=0;if(cs.find('K')!=std::string::npos)castle|=WKCA;if(cs.find('Q')!=std::string::npos)castle|=WQCA;if(cs.find('k')!=std::string::npos)castle|=BKCA;if(cs.find('q')!=std::string::npos)castle|=BQCA;ep=(eps=="-"?-1:parse_square(eps));key=compute_key();history.push_back(key);
    }
    std::string fen() const {
        std::string out;
        for(int r=7;r>=0;r--){
            int empty=0;
            for(int f=0;f<8;f++){
                Piece p=b[sq(f,r)];
                if(!p){++empty; continue;}
                if(empty){out.push_back(char('0'+empty));empty=0;}
                const char* chars=" PNBRQKpnbrqk"; out.push_back(chars[p]);
            }
            if(empty) out.push_back(char('0'+empty));
            if(r) out.push_back('/');
        }
        out += side==WHITE ? " w " : " b ";
        std::string c;
        if(castle&WKCA)c+='K';
        if(castle&WQCA)c+='Q';
        if(castle&BKCA)c+='k';
        if(castle&BQCA)c+='q';
        out += c.empty()?"-":c; out.push_back(' ');
        out += ep<0?"-":square_name(ep); out.push_back(' ');
        out += std::to_string(halfmove); out.push_back(' '); out += std::to_string(fullmove);
        return out;
    }
    int king_square(Color c) const { U64 x=bb[(c==WHITE?WK:BK)-1]; return x?__builtin_ctzll(x):-1; }
    bool attacked(int s, Color by) const {
        if(PawnAtt[opp(by)][s] & bb[(by==WHITE?WP:BP)-1]) return true;
        if(KnightAtt[s] & bb[(by==WHITE?WN:BN)-1]) return true;
        if(KingAtt[s] & bb[(by==WHITE?WK:BK)-1]) return true;
        U64 rq=bb[(by==WHITE?WR:BR)-1]|bb[(by==WHITE?WQ:BQ)-1]; if(rook_attacks(s,all)&rq)return true;
        U64 bq=bb[(by==WHITE?WB:BB)-1]|bb[(by==WHITE?WQ:BQ)-1]; if(bishop_attacks(s,all)&bq)return true;
        return false;
    }
    bool in_check(Color c) const { int k=king_square(c); return k>=0 && attacked(k,opp(c)); }
    bool make(const Move&m,Undo&u){
        u={key,castle,ep,halfmove,fullmove,EMPTY,m};
        Piece p=b[m.from()];
        if(!p || color_of(p)!=side)return false;

        const Color us=side;
        const int from=m.from(),to=m.to();
        int capture_sq=to;

        // Remove reversible state from the key before mutating it.
        key ^= Z.castle[castle];
        if(ep>=0)key ^= Z.ep[ep];

        // Move the piece out of its source square.
        key ^= Z.piece[p-1][from];
        remove(from);

        Piece captured=EMPTY;
        if(m.flag()==Move::ENPASSANT){
            capture_sq=to+(us==WHITE?-8:8);
            captured=b[capture_sq];
        }else{
            captured=b[to];
        }
        u.captured=captured;

        if(captured){
            key ^= Z.piece[captured-1][capture_sq];
            remove(capture_sq);
        }

        if(m.flag()==Move::CASTLE){
            put(to,p);
            key ^= Z.piece[p-1][to];

            int rf=-1,rt=-1;
            if(us==WHITE&&to==sq(6,0)){rf=sq(7,0);rt=sq(5,0);}
            if(us==WHITE&&to==sq(2,0)){rf=sq(0,0);rt=sq(3,0);}
            if(us==BLACK&&to==sq(6,7)){rf=sq(7,7);rt=sq(5,7);}
            if(us==BLACK&&to==sq(2,7)){rf=sq(0,7);rt=sq(3,7);}

            Piece rp=b[rf];
            key ^= Z.piece[rp-1][rf];
            remove(rf);
            put(rt,rp);
            key ^= Z.piece[rp-1][rt];
        }else if(m.flag()==Move::PROMOTION){
            Piece np=(us==WHITE?Piece(m.promo()):Piece(m.promo()+6));
            put(to,np);
            key ^= Z.piece[np-1][to];
        }else{
            put(to,p);
            key ^= Z.piece[p-1][to];
        }

        auto clear_sq=[&](int s){
            if(s==sq(0,0)) castle &= ~WQCA;
            if(s==sq(7,0)) castle &= ~WKCA;
            if(s==sq(0,7)) castle &= ~BQCA;
            if(s==sq(7,7)) castle &= ~BKCA;
        };
        if(type_of(p)==KING)castle &= (us==WHITE?~(WKCA|WQCA):~(BKCA|BQCA));
        if(type_of(p)==ROOK)clear_sq(from);
        if(captured&&type_of(captured)==ROOK)clear_sq(capture_sq);

        ep=-1;
        if(m.flag()==Move::DOUBLE_PUSH)ep=from+(us==WHITE?8:-8);
        if(type_of(p)==PAWN||captured)halfmove=0;
        else ++halfmove;
        if(us==BLACK)++fullmove;

        side=opp(us);
        key ^= Z.side;
        key ^= Z.castle[castle];
        if(ep>=0)key ^= Z.ep[ep];

        history.push_back(key);
        return true;
    }
    void undo(const Undo&u){
        side=opp(side); fullmove=u.fullmove; castle=u.castle; ep=u.ep; halfmove=u.halfmove; 
        int from=u.move.from(),to=u.move.to(); Piece p=b[to]; remove(to);
        if(u.move.flag()==Move::CASTLE){ int rf=-1,rt=-1;if(side==WHITE&&to==sq(6,0)){rf=sq(7,0);rt=sq(5,0);}if(side==WHITE&&to==sq(2,0)){rf=sq(0,0);rt=sq(3,0);}if(side==BLACK&&to==sq(6,7)){rf=sq(7,7);rt=sq(5,7);}if(side==BLACK&&to==sq(2,7)){rf=sq(0,7);rt=sq(3,7);}Piece rp=b[rt];remove(rt);put(rf,rp);p=(side==WHITE?WK:BK);}
        if(u.move.flag()==Move::PROMOTION) p=(side==WHITE?WP:BP);
        put(from,p);
        if(u.move.flag()==Move::ENPASSANT){int cs=to+(side==WHITE?-8:8);put(cs,u.captured);} else if(u.captured)put(to,u.captured); else if(p==EMPTY){} key=u.key; if(!history.empty())history.pop_back();
    }
    bool is_dead_position() const {
        if(bb[WP-1]||bb[BP-1]||bb[WR-1]||bb[BR-1]||bb[WQ-1]||bb[BQ-1])return false;
        int wn=popcount(bb[WN-1]),bn=popcount(bb[BN-1]),wb=popcount(bb[WB-1]),bbk=popcount(bb[BB-1]);
        int total=wn+bn+wb+bbk;
        if(total<=1)return true;
        if(total==2){
            if(wn==1&&bn==1)return false;
            if(wb==1&&bbk==1){
                int a=__builtin_ctzll(bb[WB-1]),bq=__builtin_ctzll(bb[BB-1]);
                return ((file_of(a)+rank_of(a))&1)==((file_of(bq)+rank_of(bq))&1);
            }
        }
        return false;
    }
    bool is_threefold_repetition() const {
        if(history.empty())return false;
        U64 k=key; int count=0;
        int limit=std::min<int>(halfmove,(int)history.size()-1);
        for(int i=(int)history.size()-1,steps=0;i>=0&&steps<=limit;i--,steps++)
            if(history[i]==k && ++count>=3)return true;
        return false;
    }
    bool is_fivefold_repetition() const {
        if(history.empty())return false;
        U64 k=key; int count=0;
        int limit=std::min<int>(halfmove,(int)history.size()-1);
        for(int i=(int)history.size()-1,steps=0;i>=0&&steps<=limit;i--,steps++)
            if(history[i]==k && ++count>=5)return true;
        return false;
    }
    bool is_fifty_move_claimable() const { return halfmove>=100; }
    bool is_seventyfive_move_draw() const { return halfmove>=150; }
    bool is_claimable_draw() const {
        return is_threefold_repetition() || is_fifty_move_claimable();
    }
    bool is_automatic_draw() const {
        return is_fivefold_repetition() || is_seventyfive_move_draw() || is_dead_position();
    }
    // Search treats a claimable draw as an immediately available draw option.
    bool is_draw_for_search() const {
        return is_claimable_draw() || is_automatic_draw();
    }

    template<class Vec>
    void pseudo(Vec&out, bool captures_only=false) const {
        Color us=side; Color them=opp(us); U64 own=occ[us], enemy=occ[them];
        auto add=[&](int f,int t,int promo=NONE,Move::Flag fl=Move::QUIET){out.push_back(Move::make(f,t,promo,fl));};
        U64 p=bb[(us==WHITE?WP:BP)-1]; while(p){int s=poplsb(p);int r=rank_of(s),f=file_of(s),dir=(us==WHITE?8:-8),start=(us==WHITE?1:6),last=(us==WHITE?7:0);int t=s+dir;if(!captures_only&&t>=0&&t<64&&b[t]==EMPTY){if(rank_of(t)==last){for(int pr:{KNIGHT,BISHOP,ROOK,QUEEN})add(s,t,pr,Move::PROMOTION);}else{add(s,t);if(r==start&&b[s+2*dir]==EMPTY)add(s,s+2*dir,NONE,Move::DOUBLE_PUSH);}} for(int df:{-1,1}){int nf=f+df;if(nf<0||nf>=8)continue;int ct=s+dir+df;if(ct>=0&&ct<64){if(b[ct]!=EMPTY&&color_of(b[ct])==them){if(rank_of(ct)==last){for(int pr:{KNIGHT,BISHOP,ROOK,QUEEN})add(s,ct,pr,Move::PROMOTION);}else add(s,ct);} if(ct==ep)add(s,ct,NONE,Move::ENPASSANT);}} }
        U64 n=bb[(us==WHITE?WN:BN)-1]; while(n){int s=poplsb(n);U64 a=KnightAtt[s]&~own;if(captures_only)a&=enemy;while(a){int t=poplsb(a);add(s,t);}}
        U64 bi=bb[(us==WHITE?WB:BB)-1]; while(bi){int s=poplsb(bi);U64 a=bishop_attacks(s,all)&~own;if(captures_only)a&=enemy;while(a){int t=poplsb(a);add(s,t);}}
        U64 ro=bb[(us==WHITE?WR:BR)-1]; while(ro){int s=poplsb(ro);U64 a=rook_attacks(s,all)&~own;if(captures_only)a&=enemy;while(a){int t=poplsb(a);add(s,t);}}
        U64 qu=bb[(us==WHITE?WQ:BQ)-1]; while(qu){int s=poplsb(qu);U64 a=queen_attacks(s,all)&~own;if(captures_only)a&=enemy;while(a){int t=poplsb(a);add(s,t);}}
        int ks=king_square(us); if(ks>=0){U64 a=KingAtt[ks]&~own;if(captures_only)a&=enemy;while(a){int t=poplsb(a);add(ks,t);} if(!captures_only&&!in_check(us)){
            if(us==WHITE && (castle&WKCA) && b[sq(5,0)]==EMPTY&&b[sq(6,0)]==EMPTY&&!attacked(sq(5,0),them)&&!attacked(sq(6,0),them))add(ks,sq(6,0),NONE,Move::CASTLE);
            if(us==WHITE && (castle&WQCA) && b[sq(1,0)]==EMPTY&&b[sq(2,0)]==EMPTY&&b[sq(3,0)]==EMPTY&&!attacked(sq(3,0),them)&&!attacked(sq(2,0),them))add(ks,sq(2,0),NONE,Move::CASTLE);
            if(us==BLACK && (castle&BKCA) && b[sq(5,7)]==EMPTY&&b[sq(6,7)]==EMPTY&&!attacked(sq(5,7),them)&&!attacked(sq(6,7),them))add(ks,sq(6,7),NONE,Move::CASTLE);
            if(us==BLACK && (castle&BQCA) && b[sq(1,7)]==EMPTY&&b[sq(2,7)]==EMPTY&&b[sq(3,7)]==EMPTY&&!attacked(sq(3,7),them)&&!attacked(sq(2,7),them))add(ks,sq(2,7),NONE,Move::CASTLE);
        }}
    }
    bool is_consistent() const {
        if((occ[WHITE]&occ[BLACK])!=0)return false;
        if(all!=(occ[WHITE]|occ[BLACK]))return false;
        if((castle&~15)!=0)return false;
        if(ep<-1||ep>=64)return false;
        if(halfmove<0||fullmove<1)return false;

        U64 calc_bb[12]{}, calc_occ[2]{};
        for(int s=0;s<64;s++){
            Piece p=b[s];
            if(!p)continue;
            int pi=p-1, ci=color_of(p);
            U64 bs=bit(s);
            calc_bb[pi]|=bs;
            calc_occ[ci]|=bs;
        }
        for(int i=0;i<12;i++)if(calc_bb[i]!=bb[i])return false;
        if(calc_occ[WHITE]!=occ[WHITE]||calc_occ[BLACK]!=occ[BLACK])return false;
        if(popcount(bb[WK-1])!=1||popcount(bb[BK-1])!=1)return false;
        if(key!=compute_key())return false;
        if(!history.empty()&&history.back()!=key)return false;
        return true;
    }

    std::vector<Move> legal(bool captures_only=false){ std::vector<Move> p; p.reserve(128); pseudo(p,captures_only); std::vector<Move> out;out.reserve(p.size()); for(const auto&m:p){Undo u;if(make(m,u)){Color moved=opp(side);bool ok=!in_check(moved);undo(u);if(ok)out.push_back(m);}} return out; }
    std::vector<Move> legal_no_mutation(bool captures_only=false) const { Board tmp=*this; return tmp.legal(captures_only); }
};

enum Bound : std::uint8_t { EXACT=1, LOWER=2, UPPER=3 };

struct TTEntry {
    U64 key=0;
    int depth=-1;
    Score score=0;
    Score eval=0;
    std::uint8_t flag=0;
    std::uint8_t generation=0;
    std::uint32_t move=0;
};

class TT {
    static constexpr int CLUSTER_SIZE=4;
    using Bucket=std::array<TTEntry,CLUSTER_SIZE>;

    std::vector<Bucket> table;
    U64 mask=0;
    std::uint8_t generation=0;

    int replacement_score(const TTEntry&e) const {
        if(e.depth<0)return -1000000;
        const int age=static_cast<std::uint8_t>(generation-e.generation);
        return e.depth-8*age;
    }

public:
    explicit TT(std::size_t mb=64){resize(mb);}

    void resize(std::size_t mb){
        std::size_t n=1;
        const std::size_t bytes=mb*1024ULL*1024ULL;
        const std::size_t max_buckets=std::max<std::size_t>(1,bytes/sizeof(Bucket));
        while(n<=max_buckets/2)n<<=1;
        table.assign(n,{});
        mask=n-1;
        generation=0;
    }

    void new_search(){++generation;}

    void clear(){
        std::fill(table.begin(),table.end(),Bucket{});
        generation=0;
    }

    std::size_t bucket_count() const{return table.size();}
    static constexpr int cluster_size(){return CLUSTER_SIZE;}

    TTEntry* probe(U64 k){
        auto&bucket=table[k&mask];
        for(auto&e:bucket)if(e.depth>=0&&e.key==k)return &e;
        return nullptr;
    }

    const TTEntry* probe(U64 k) const{
        const auto&bucket=table[k&mask];
        for(const auto&e:bucket)if(e.depth>=0&&e.key==k)return &e;
        return nullptr;
    }

    void store(U64 k,int d,Score s,Bound fl,Move m,Score ev){
        auto&bucket=table[k&mask];

        for(auto&e:bucket){
            if(e.depth>=0&&e.key==k){
                // Refresh the entry's age even when a shallower result does
                // not replace a deeper one.
                e.generation=generation;
                if(d>=e.depth)e={k,d,s,ev,static_cast<std::uint8_t>(fl),generation,m.data};
                return;
            }
        }

        TTEntry* victim=&bucket[0];
        for(auto&e:bucket){
            if(e.depth<0){victim=&e;break;}
            if(replacement_score(e)<replacement_score(*victim))victim=&e;
        }

        *victim={k,d,s,ev,static_cast<std::uint8_t>(fl),generation,m.data};
    }

    int hashfull() const{
        const std::size_t sample=std::min<std::size_t>(1000,table.size()*CLUSTER_SIZE);
        if(!sample)return 0;

        std::size_t seen=0,used=0;
        for(const auto&bucket:table){
            for(const auto&e:bucket){
                if(e.depth>=0&&e.generation==generation)++used;
                if(++seen>=sample)break;
            }
            if(seen>=sample)break;
        }
        return static_cast<int>((used*1000)/sample);
    }
};

class Searcher {
    Board &pos;
    TT tt;
    std::atomic<bool> stop{false};
    std::uint64_t nodes=0;
    int depth_limit=64;
    std::chrono::steady_clock::time_point start_time, deadline;
    bool timed=false;
    std::uint64_t node_limit=0;
    std::size_t hash_mb=64;
    std::vector<Move> restricted_root;

    Move killers[MAX_PLY][2]{};
    Move counter_moves[2][64][64]{};
    int history[2][64][64]{};

    Score piece_value[7]={0,100,320,330,500,900,0};

    const int pst[6][64] = {
      {0,5,5,0,0,5,5,0, 5,10,10,8,8,10,10,5, 3,6,8,12,12,8,6,3, 0,0,2,7,7,2,0,0, 0,0,0,5,5,0,0,0, 2,0,0,-5,-5,0,0,2, 2,4,4,-8,-8,4,4,2, 0,0,0,0,0,0,0,0},
      {-50,-40,-30,-30,-30,-30,-40,-50,-40,-20,0,0,0,0,-20,-40,-30,0,10,15,15,10,0,-30,-30,5,15,20,20,15,5,-30,-30,0,15,20,20,15,0,-30,-40,-20,0,5,5,0,-20,-40,-50,-40,-30,-30,-30,-30,-40,-50},
      {-20,-10,-10,-10,-10,-10,-10,-20,-10,0,0,0,0,0,0,-10,-10,0,5,8,8,5,0,-10,-10,5,5,10,10,5,5,-10,-10,0,10,10,10,10,0,-10,-10,5,0,0,0,0,5,-10,-20,-10,-10,-10,-10,-10,-10,-20},
      {0,0,0,5,5,0,0,0, 0,0,0,5,5,0,0,0, 0,0,5,10,10,5,0,0, 0,0,5,10,10,5,0,0, 0,0,5,10,10,5,0,0, 0,0,5,10,10,5,0,0, 0,0,0,5,5,0,0,0, 0,0,0,5,5,0,0,0},
      {-20,-10,-10,-5,-5,-10,-10,-20, -10,0,0,0,0,0,0,-10, 0,0,5,5,5,5,0,0, 0,0,5,10,10,5,0,0, 0,0,5,10,10,5,0,0, -10,0,0,0,0,0,0,-10, -20,-10,-10,-5,-5,-10,-10,-20},
      {-30,-40,-40,-50,-50,-40,-40,-30, -30,-40,-40,-50,-50,-40,-40,-30, -20,-30,-30,-40,-40,-30,-30,-20, -10,-20,-20,-20,-20,-20,-20,-10, 0,-10,-10,-10,-10,-10,-10,0, 20,20,0,0,0,0,20,20, 20,30,10,0,0,10,30,20}
    };

    static constexpr int EVAL_PHASE_MAX=24;
    static constexpr int eval_mg_value[6]={82,337,365,477,1025,0};
    static constexpr int eval_eg_value[6]={94,281,297,512,936,0};
    static constexpr int phase_value[6]={0,1,1,2,4,0};

    static int oriented_rank(Color c,int s){
        const int r=rank_of(s);
        return c==WHITE?r:7-r;
    }

    static int chebyshev_distance(int a,int b){
        const int df=file_of(a)>file_of(b)?file_of(a)-file_of(b):file_of(b)-file_of(a);
        const int dr=rank_of(a)>rank_of(b)?rank_of(a)-rank_of(b):rank_of(b)-rank_of(a);
        return std::max(df,dr);
    }

    static int center_distance(int s){
        const int df=file_of(s)>3?file_of(s)-3:3-file_of(s);
        const int dr=rank_of(s)>3?rank_of(s)-3:3-rank_of(s);
        return df+dr;
    }

    U64 forward_with_adjacent_mask(Color c,int s) const {
        U64 mask=0;
        const int f=file_of(s),r=rank_of(s);
        for(int ff=std::max(0,f-1);ff<=std::min(7,f+1);ff++){
            if(c==WHITE){
                for(int rr=r+1;rr<8;rr++)mask|=FileMask[ff]&RankMask[rr];
            }else{
                for(int rr=r-1;rr>=0;rr--)mask|=FileMask[ff]&RankMask[rr];
            }
        }
        return mask;
    }

    bool is_passed_pawn(Color c,int s,U64 enemy_pawns) const {
        return (forward_with_adjacent_mask(c,s)&enemy_pawns)==0;
    }

    bool is_pawn_supported(Color c,int s,U64 own_pawns) const {
        return (PawnAtt[opp(c)][s]&own_pawns)!=0;
    }

    bool is_defended(Color c,int s) const {
        if(PawnAtt[opp(c)][s]&pos.bb[(c==WHITE?WP:BP)-1])return true;
        if(KnightAtt[s]&pos.bb[(c==WHITE?WN:BN)-1])return true;
        if(KingAtt[s]&pos.bb[(c==WHITE?WK:BK)-1])return true;
        const U64 bishops=pos.bb[(c==WHITE?WB:BB)-1]|pos.bb[(c==WHITE?WQ:BQ)-1];
        const U64 rooks=pos.bb[(c==WHITE?WR:BR)-1]|pos.bb[(c==WHITE?WQ:BQ)-1];
        return (bishop_attacks(s,pos.all)&bishops)||(rook_attacks(s,pos.all)&rooks);
    }

    U64 control_map(Color c) const {
        U64 control=0;
        U64 pawns=pos.bb[(c==WHITE?WP:BP)-1];
        while(pawns){
            const int s=poplsb(pawns);
            control|=PawnAtt[c][s];
        }
        U64 knights=pos.bb[(c==WHITE?WN:BN)-1];
        while(knights){
            const int s=poplsb(knights);
            control|=KnightAtt[s];
        }
        U64 bishops=pos.bb[(c==WHITE?WB:BB)-1];
        while(bishops){
            const int s=poplsb(bishops);
            control|=bishop_attacks(s,pos.all);
        }
        U64 rooks=pos.bb[(c==WHITE?WR:BR)-1];
        while(rooks){
            const int s=poplsb(rooks);
            control|=rook_attacks(s,pos.all);
        }
        U64 queens=pos.bb[(c==WHITE?WQ:BQ)-1];
        while(queens){
            const int s=poplsb(queens);
            control|=queen_attacks(s,pos.all);
        }
        U64 king=pos.bb[(c==WHITE?WK:BK)-1];
        if(king)control|=KingAtt[__builtin_ctzll(king)];
        return control;
    }

    int file_pawn_count(U64 pawns,int f) const {
        return popcount(pawns&FileMask[f]);
    }

    void evaluate_side_terms(Color c,int&mg,int&eg) const {
        const Color enemy=opp(c);
        const U64 own_pawns=pos.bb[(c==WHITE?WP:BP)-1];
        const U64 enemy_pawns=pos.bb[(enemy==WHITE?WP:BP)-1];
        const int sign=(c==WHITE?1:-1);

        auto add=[&](int mgv,int egv){
            mg+=sign*mgv;
            eg+=sign*egv;
        };

        int pawn_islands=0;
        bool previous_file=false;
        for(int f=0;f<8;f++){
            const bool has=file_pawn_count(own_pawns,f)>0;
            if(has&&!previous_file)++pawn_islands;
            previous_file=has;
        }
        if(pawn_islands>1)add(-8*(pawn_islands-1),-12*(pawn_islands-1));

        for(int f=0;f<8;f++){
            const int count=file_pawn_count(own_pawns,f);
            if(count>1)add(-10*(count-1),-14*(count-1));
        }

        U64 pawns=own_pawns;
        while(pawns){
            const int s=poplsb(pawns);
            const int f=file_of(s);
            const int r=rank_of(s);
            const int orank=oriented_rank(c,s);

            U64 adjacent_files=0;
            if(f>0)adjacent_files|=FileMask[f-1];
            if(f<7)adjacent_files|=FileMask[f+1];

            const bool isolated=(own_pawns&adjacent_files)==0;
            if(isolated)add(-12,-18);

            const bool supported=is_pawn_supported(c,s,own_pawns);
            if(supported)add(5,8);

            bool connected=false;
            for(int ff=std::max(0,f-1);ff<=std::min(7,f+1);ff++){
                if(ff==f)continue;
                U64 q=own_pawns&FileMask[ff];
                while(q){
                    const int ps=poplsb(q);
                    const int dr=rank_of(ps)>r?rank_of(ps)-r:r-rank_of(ps);
                    if(dr<=1){connected=true;break;}
                }
                if(connected)break;
            }
            if(connected)add(5,7);

            const bool passed=is_passed_pawn(c,s,enemy_pawns);
            if(passed){
                static constexpr int pass_mg[8]={0,0,4,10,20,36,58,92};
                static constexpr int pass_eg[8]={0,0,12,24,42,70,112,170};
                add(pass_mg[orank],pass_eg[orank]);

                if(supported)add(12,22);

                const int own_king=pos.king_square(c);
                const int enemy_king=pos.king_square(enemy);
                if(own_king>=0&&enemy_king>=0){
                    const int race=chebyshev_distance(enemy_king,s)-chebyshev_distance(own_king,s);
                    add(std::clamp(race*2,-12,12),std::clamp(race*4,-24,24));
                }

                const Piece rook=(c==WHITE?WR:BR);
                U64 rooks=pos.bb[rook-1];
                bool rook_behind=false;
                while(rooks){
                    const int rs=poplsb(rooks);
                    if(file_of(rs)==f && ((c==WHITE&&rank_of(rs)<r)||(c==BLACK&&rank_of(rs)>r))){
                        rook_behind=true;
                        break;
                    }
                }
                if(rook_behind)add(10,20);
            }

            bool adjacent_ahead=false;
            if(f>0 && (own_pawns&FileMask[f-1])){
                U64 q=own_pawns&FileMask[f-1];
                while(q){
                    const int ps=poplsb(q);
                    if(oriented_rank(c,ps)>orank){adjacent_ahead=true;break;}
                }
            }
            if(!adjacent_ahead && f<7 && (own_pawns&FileMask[f+1])){
                U64 q=own_pawns&FileMask[f+1];
                while(q){
                    const int ps=poplsb(q);
                    if(oriented_rank(c,ps)>orank){adjacent_ahead=true;break;}
                }
            }

            const int front=s+(c==WHITE?8:-8);
            const bool enemy_controls_front=(front>=0&&front<64)&&((PawnAtt[opp(enemy)][front]&enemy_pawns)!=0);
            if(!isolated&&!passed&&adjacent_ahead&&enemy_controls_front)
                add(-10,-14);
        }

        const int bishop_count=popcount(pos.bb[(c==WHITE?WB:BB)-1]);
        if(bishop_count>=2)add(30,42);

        const U64 own_control=control_map(c);
        const U64 own_nonpawns=pos.occ[c]&~own_pawns;
        add(popcount(own_control&own_nonpawns)*3,popcount(own_control&own_nonpawns)*5);

        U64 bishops=pos.bb[(c==WHITE?WB:BB)-1];
        while(bishops){
            const int s=poplsb(bishops);
            const int mr=oriented_rank(c,s);
            const bool pawn_safe=(PawnAtt[opp(enemy)][s]&enemy_pawns)==0;
            const bool supported=is_pawn_supported(c,s,own_pawns);
            const bool outpost=mr>=3&&mr<=5&&pawn_safe&&supported;
            if(outpost)add(18,24);

            const int mobility=popcount(bishop_attacks(s,pos.all)&~pos.occ[c]);
            add(mobility*4,mobility*5);
            if(mobility<=2)add(-12,-7);
        }

        U64 knights=pos.bb[(c==WHITE?WN:BN)-1];
        while(knights){
            const int s=poplsb(knights);
            const int mr=oriented_rank(c,s);
            const bool pawn_safe=(PawnAtt[opp(enemy)][s]&enemy_pawns)==0;
            const bool supported=is_pawn_supported(c,s,own_pawns);
            const bool outpost=mr>=3&&mr<=5&&pawn_safe&&supported;
            if(outpost)add(22,30);

            const int mobility=popcount(KnightAtt[s]&~pos.occ[c]);
            add(mobility*5,mobility*4);
            if(mobility<=2)add(-14,-8);
        }

        U64 rooks=pos.bb[(c==WHITE?WR:BR)-1];
        while(rooks){
            const int s=poplsb(rooks);
            const int f=file_of(s);
            const bool own_pawn=bool(own_pawns&FileMask[f]);
            const bool enemy_pawn=bool(enemy_pawns&FileMask[f]);
            const int mobility=popcount(rook_attacks(s,pos.all)&~pos.occ[c]);
            if(!own_pawn)add(18,24);
            if(!own_pawn&&!enemy_pawn)add(12,18);
            if(oriented_rank(c,s)==6)add(18,28);
            add(mobility*2,mobility*3);
            if(mobility<=1)add(-10,-5);
        }

        U64 queens=pos.bb[(c==WHITE?WQ:BQ)-1];
        while(queens){
            const int s=poplsb(queens);
            const int mobility=popcount(queen_attacks(s,pos.all)&~pos.occ[c]);
            add(mobility,mobility*2);
        }

        const std::array<std::pair<int,int>,4> home_minor={
            c==WHITE?std::pair<int,int>{sq(1,0),WN}:std::pair<int,int>{sq(1,7),BN},
            c==WHITE?std::pair<int,int>{sq(6,0),WN}:std::pair<int,int>{sq(6,7),BN},
            c==WHITE?std::pair<int,int>{sq(2,0),WB}:std::pair<int,int>{sq(2,7),BB},
            c==WHITE?std::pair<int,int>{sq(5,0),WB}:std::pair<int,int>{sq(5,7),BB}
        };
        for(const auto&home:home_minor)
            if(pos.b[home.first]==home.second)add(-8,0);

        const int ks=pos.king_square(c);
        if(ks>=0){
            int shelter_mg=0,shelter_eg=0;
            const int kf=file_of(ks);
            const int kr=rank_of(ks);
            for(int ff=std::max(0,kf-1);ff<=std::min(7,kf+1);ff++){
                const U64 pawns_on_file=own_pawns&FileMask[ff];
                int best_gap=99;
                U64 q=pawns_on_file;
                while(q){
                    const int ps=poplsb(q);
                    const int gap=(c==WHITE?rank_of(ps)-kr:kr-rank_of(ps));
                    if(gap>=1&&gap<best_gap)best_gap=gap;
                }
                if(best_gap==1){shelter_mg+=14;shelter_eg+=4;}
                else if(best_gap==2){shelter_mg+=9;shelter_eg+=3;}
                else shelter_mg-=12;
            }
            add(shelter_mg,shelter_eg);

            U64 ring=KingAtt[ks];
            int attack_units=0;
            U64 enemy_knights=pos.bb[(enemy==WHITE?WN:BN)-1];
            while(enemy_knights){const int s=poplsb(enemy_knights);attack_units+=popcount(KnightAtt[s]&ring)*4;}
            U64 enemy_bishops=pos.bb[(enemy==WHITE?WB:BB)-1];
            while(enemy_bishops){const int s=poplsb(enemy_bishops);attack_units+=popcount(bishop_attacks(s,pos.all)&ring)*4;}
            U64 enemy_rooks=pos.bb[(enemy==WHITE?WR:BR)-1];
            while(enemy_rooks){const int s=poplsb(enemy_rooks);attack_units+=popcount(rook_attacks(s,pos.all)&ring)*5;}
            U64 enemy_queens=pos.bb[(enemy==WHITE?WQ:BQ)-1];
            while(enemy_queens){const int s=poplsb(enemy_queens);attack_units+=popcount(queen_attacks(s,pos.all)&ring)*7;}
            add(-std::min(40,attack_units),-std::min(16,attack_units/2));

            int open_near_king=0;
            for(int ff=std::max(0,kf-1);ff<=std::min(7,kf+1);ff++)
                if(!(own_pawns&FileMask[ff])&&!(enemy_pawns&FileMask[ff]))++open_near_king;
            add(-open_near_king*10,-open_near_king*3);

            int safe_king_squares=0;
            U64 king_moves=KingAtt[ks]&~pos.occ[c];
            while(king_moves){
                const int dst=poplsb(king_moves);
                if(!pos.attacked(dst,enemy))++safe_king_squares;
            }
            add(safe_king_squares*2,safe_king_squares*6);

            const int kd=center_distance(ks);
            add(-std::max(0,6-kd)*2,(7-kd)*4);
        }

        const U64 center=bit(sq(3,3))|bit(sq(4,3))|bit(sq(3,4))|bit(sq(4,4));
        const U64 space_zone=(c==WHITE)
            ?(RankMask[3]|RankMask[4]|RankMask[5])
            :(RankMask[2]|RankMask[3]|RankMask[4]);
        add(popcount(own_control&center)*3,popcount(own_control&center)*2);
        add(popcount(own_control&space_zone),0);
    }

    Score eval() const {
        int mg=0,eg=0;
        int phase=0;

        for(int sqr=0;sqr<64;sqr++){
            const Piece p=pos.b[sqr];
            if(!p)continue;
            const PieceType pt=type_of(p);
            if(pt==KING)continue;
            const int idx=pt-1;
            const Color c=color_of(p);
            const int sign=(c==WHITE?1:-1);
            const int oriented=c==WHITE?sqr:sqr^56;
            const int mg_ps=pst[idx][oriented];
            int eg_ps=0;
            const int cr=center_distance(oriented);

            switch(pt){
                case PAWN: eg_ps=oriented_rank(c,sqr)*3+(6-cr); break;
                case KNIGHT: eg_ps=16-cr*3; break;
                case BISHOP: eg_ps=10-cr*2; break;
                case ROOK: eg_ps=(oriented_rank(c,sqr)==6?18:0)-cr; break;
                case QUEEN: eg_ps=4-cr; break;
                default: break;
            }

            mg+=sign*(eval_mg_value[idx]+mg_ps);
            eg+=sign*(eval_eg_value[idx]+eg_ps);
            phase+=phase_value[idx];
        }

        evaluate_side_terms(WHITE,mg,eg);
        evaluate_side_terms(BLACK,mg,eg);

        phase=std::clamp(phase,0,EVAL_PHASE_MAX);
        Score blended=(mg*phase+eg*(EVAL_PHASE_MAX-phase))/EVAL_PHASE_MAX;
        blended+=(pos.side==WHITE?8:-8);

        if(pos.in_check(WHITE))blended-=18;
        if(pos.in_check(BLACK))blended+=18;

        return pos.side==WHITE?blended:-blended;
    }

    int see_value(PieceType pt) const {
        static constexpr int values[7]={0,100,320,330,500,900,20000};
        return values[pt];
    }

    bool is_capture(const Move&m) const {
        if(m.flag()==Move::ENPASSANT)return true;
        return pos.b[m.to()]!=EMPTY;
    }

    Piece captured_piece(const Move&m) const {
        if(m.flag()==Move::ENPASSANT)return pos.side==WHITE?BP:WP;
        return pos.b[m.to()];
    }

    int non_pawn_material(Color c) const {
        int total=0;
        for(PieceType pt:{KNIGHT,BISHOP,ROOK,QUEEN})
            total+=popcount(pos.bb[(c==WHITE?int(pt):int(pt)+6)-1])*piece_value[pt];
        return total;
    }

    void touch(){
        ++nodes;
        if((nodes&2047)==0){
            if(stop.load())return;
            if(timed&&std::chrono::steady_clock::now()>=deadline)stop.store(true);
            if(node_limit&&nodes>=node_limit)stop.store(true);
        }
    }

    bool is_quiet(const Move&m,Piece cap) const {
        return cap==EMPTY&&m.flag()!=Move::PROMOTION&&m.flag()!=Move::CASTLE&&m.flag()!=Move::ENPASSANT;
    }

    int see(const Move&m) const {
        if(!is_capture(m)&&m.flag()!=Move::PROMOTION)return 0;

        U64 pieces[12];
        std::memcpy(pieces,pos.bb,sizeof(pieces));
        U64 occupancy=pos.all;
        const Color us=pos.side;
        const int from=m.from(),to=m.to();

        Piece moving=pos.b[from];
        Piece captured=captured_piece(m);

        int capture_sq=to;
        if(m.flag()==Move::ENPASSANT)capture_sq=to+(us==WHITE?-8:8);

        for(int i=0;i<12;i++)pieces[i]=pos.bb[i];
        pieces[moving-1]&=~bit(from);
        occupancy&=~bit(from);

        if(captured){
            pieces[captured-1]&=~bit(capture_sq);
            occupancy&=~bit(capture_sq);
        }

        Piece on_target=moving;
        int initial_gain=see_value(type_of(captured));
        if(m.flag()==Move::PROMOTION){
            Piece promoted=(us==WHITE?Piece(m.promo()):Piece(m.promo()+6));
            initial_gain+=see_value(type_of(promoted))-see_value(PAWN);
            on_target=promoted;
        }

        pieces[on_target-1]|=bit(to);
        occupancy|=bit(to);

        Score gain[32]{};
        gain[0]=initial_gain;
        int depth=0;
        Color side=opp(us);

        while(depth<31){
            U64 attackers=0;
            attackers|=PawnAtt[opp(side)][to]&pieces[(side==WHITE?WP:BP)-1];
            attackers|=KnightAtt[to]&pieces[(side==WHITE?WN:BN)-1];
            U64 bishops=pieces[(side==WHITE?WB:BB)-1]|pieces[(side==WHITE?WQ:BQ)-1];
            U64 rooks=pieces[(side==WHITE?WR:BR)-1]|pieces[(side==WHITE?WQ:BQ)-1];
            attackers|=bishop_attacks(to,occupancy)&bishops;
            attackers|=rook_attacks(to,occupancy)&rooks;
            attackers|=KingAtt[to]&pieces[(side==WHITE?WK:BK)-1];

            Piece attacker=EMPTY;
            int attacker_sq=-1;
            for(PieceType pt:{PAWN,KNIGHT,BISHOP,ROOK,QUEEN,KING}){
                U64 bb_attackers=attackers&pieces[(side==WHITE?int(pt):int(pt)+6)-1];
                if(bb_attackers){
                    attacker_sq=__builtin_ctzll(bb_attackers);
                    attacker=(side==WHITE?Piece(int(pt)):Piece(int(pt)+6));
                    break;
                }
            }
            if(attacker_sq<0)break;

            ++depth;
            gain[depth]=see_value(type_of(on_target))-gain[depth-1];

            pieces[on_target-1]&=~bit(to);
            pieces[attacker-1]&=~bit(attacker_sq);
            occupancy&=~bit(attacker_sq);
            pieces[attacker-1]|=bit(to);
            on_target=attacker;
            side=opp(side);
        }

        while(depth>0){
            --depth;
            gain[depth]=-std::max(-gain[depth],gain[depth+1]);
        }
        return gain[0];
    }

    int history_value(const Move&m) const {
        return history[pos.side][m.from()][m.to()];
    }

    int score_move(const Move&m,const Move&ttm,const Move&counter_move,int ply) const {
        if(m==ttm)return 1500000;

        Piece cap=captured_piece(m);
        const bool capture=is_capture(m);

        if(capture){
            const int exchange=see(m);
            const int base=exchange>=0?900000:140000;
            return base+std::clamp(exchange*8,-50000,50000)
                  +see_value(type_of(cap))/4;
        }

        if(m.flag()==Move::PROMOTION)
            return 800000+see_value(PieceType(m.promo()))*4;

        if(m==killers[ply][0])return 700000;
        if(m==killers[ply][1])return 690000;
        if(m==counter_move)return 680000;

        return std::clamp(history_value(m),-100000,100000);
    }

    std::vector<Move> ordered(const std::vector<Move>&ms,const Move&ttm,int ply,const Move&prev_move) const {
        Move counter_move{};
        if(prev_move.data)counter_move=counter_moves[pos.side][prev_move.from()][prev_move.to()];

        std::vector<std::pair<int,Move>> scored;
        scored.reserve(ms.size());
        for(const auto&m:ms)
            scored.push_back({score_move(m,ttm,counter_move,ply),m});

        std::stable_sort(scored.begin(),scored.end(),[](const auto&a,const auto&b){return a.first>b.first;});

        std::vector<Move> out;
        out.reserve(ms.size());
        for(const auto&x:scored)out.push_back(x.second);
        return out;
    }

    int quiescence_delta(const Move&m) const {
        int gain=0;
        const Piece cap=captured_piece(m);
        if(cap!=EMPTY)gain+=see_value(type_of(cap));
        if(m.flag()==Move::PROMOTION)
            gain+=std::max(0,see_value(PieceType(m.promo()))-see_value(PAWN));
        return gain;
    }

    Score quiesce(Score alpha,Score beta,int ply,int qdepth=8){
        touch();
        if(stop.load())return 0;

        const bool chk=pos.in_check(pos.side);
        if(pos.is_draw_for_search())return 0;
        if(ply>=MAX_PLY-1)return chk?(-INF+ply):eval();

        auto legal_moves=pos.legal(false);
        if(legal_moves.empty())return chk?(-MATE+ply):0;

        // In check there is no stand-pat: every legal evasion is tactically relevant.
        if(chk){
            std::vector<Move> evasions=ordered(legal_moves,Move{},ply,Move{});
            Score best=-INF;
            for(const auto&m:evasions){
                Undo u;
                if(!pos.make(m,u))continue;
                const Score sc=-quiesce(-beta,-alpha,ply+1,std::max(0,qdepth-1));
                pos.undo(u);
                if(stop.load())return 0;
                if(sc>best)best=sc;
                if(sc>=beta)return beta;
                if(sc>alpha)alpha=sc;
            }
            return best;
        }

        // Stand-pat is valid only outside check. A score above beta is an immediate
        // cutoff; otherwise it still raises alpha before selective tactical search.
        const Score stand=eval();
        if(stand>=beta)return beta;
        if(stand>alpha)alpha=stand;

        struct QCandidate { Move move{}; int category=0; int score=0; bool gives_check=false; };
        std::vector<QCandidate> candidates;
        candidates.reserve(legal_moves.size());

        // 1) Captures, but only SEE-safe captures survive the initial filter.
        // 2) Promotions are searched even when quiet because they change material
        //    and can be missed entirely by a capture-only qsearch.
        // 3) Quiet checking moves extend the tactical horizon at controlled qdepth.
        for(const auto&m:legal_moves){
            const Piece cap=captured_piece(m);
            const bool capture=is_capture(m);
            const bool promotion=m.flag()==Move::PROMOTION;

            if(capture||promotion){
                const int exchange=see(m);
                Undo u;
                if(!pos.make(m,u))continue;
                const bool check_after=pos.in_check(pos.side);
                pos.undo(u);

                if(capture&&exchange<0&&!check_after)continue;

                int score=promotion?800000:900000;
                score+=std::clamp(exchange*8,-40000,40000);
                if(cap!=EMPTY)score+=see_value(type_of(cap));
                const int category=(promotion&&!capture)?1:0;
                candidates.push_back({m,category,score,check_after});
                continue;
            }

            if(qdepth<=0)continue;

            Undo u;
            if(!pos.make(m,u))continue;
            const bool check_after=pos.in_check(pos.side);
            pos.undo(u);
            if(check_after)candidates.push_back({m,2,700000,true});
        }

        std::stable_sort(candidates.begin(),candidates.end(),[](const QCandidate&a,const QCandidate&b){
            if(a.category!=b.category)return a.category<b.category;
            return a.score>b.score;
        });

        constexpr int DELTA_MARGIN=110;

        // SEE filtering comes before delta pruning: a capture that already loses
        // material is normally outside the tactical shell unless it gives check.
        for(const auto&candidate:candidates){
            const Move&m=candidate.move;
            const Piece cap=captured_piece(m);
            const bool capture=is_capture(m);
            const bool promotion=m.flag()==Move::PROMOTION;

            if(!candidate.gives_check){
                const int optimistic_gain=quiescence_delta(m);
                if(stand+optimistic_gain+DELTA_MARGIN<=alpha)continue;
            }

            // The search path here is deliberately make/unmake checked again so
            // the candidate classifier remains independent from legality code.
            Undo u;
            if(!pos.make(m,u))continue;
            const Score sc=-quiesce(-beta,-alpha,ply+1,std::max(0,qdepth-1));
            pos.undo(u);

            (void)cap;
            (void)capture;
            (void)promotion;
            if(stop.load())return 0;
            if(sc>=beta)return beta;
            if(sc>alpha)alpha=sc;
        }

        return alpha;
    }

    Score search(int depth,Score alpha,Score beta,int ply,bool allow_null=true,Move prev_move={}){
        touch();
        if(stop.load())return 0;
        if(ply>=MAX_PLY-1)return eval();

        const bool root=(ply==0);
        const bool pv_node=(beta-alpha)>1;
        const bool chk=pos.in_check(pos.side);

        if(depth<=0)return quiesce(alpha,beta,ply);

        auto moves=pos.legal();
        if(moves.empty())return chk?(-MATE+ply):0;
        if(pos.is_draw_for_search())return 0;

        TTEntry* te=tt.probe(pos.key);
        Move ttm{};
        if(te){
            ttm.data=te->move;
            if(!root&&te->depth>=depth){
                Score tt_score=score_from_tt(te->score,ply);
                if(te->flag==EXACT)return tt_score;
                if(te->flag==LOWER&&tt_score>=beta)return tt_score;
                if(te->flag==UPPER&&tt_score<=alpha)return tt_score;
            }
        }

        Score static_eval=eval();

        // Null move is restricted in shallow/low-material positions where
        // zugzwang is common. Deeper cutoffs are verified without null moves.
        if(!chk&&allow_null&&depth>=4&&static_eval>=beta&&
           non_pawn_material(pos.side)>=800&&
           non_pawn_material(opp(pos.side))>=800){
            const Color us=pos.side;
            const int savedEp=pos.ep;
            const int savedHalf=pos.halfmove;
            const U64 savedKey=pos.key;
            if(pos.ep>=0)pos.key^=Z.ep[pos.ep];
            pos.ep=-1;
            ++pos.halfmove;
            pos.side=opp(us);
            pos.key^=Z.side;

            const int reduction=std::min(depth-2,2+depth/4);
            Score null_score=-search(depth-1-reduction,-beta,-beta+1,ply+1,false,Move{});

            pos.side=us;
            pos.key^=Z.side;
            pos.ep=savedEp;
            pos.halfmove=savedHalf;
            pos.key=savedKey;

            if(stop.load())return 0;

            if(null_score>=beta){
                if(depth<9)return null_score;

                const Score verify=-search(std::max(0,depth-1-reduction),-beta,-beta+1,ply,false,prev_move);
                if(stop.load())return 0;
                if(verify>=beta)return null_score;
            }
        }

        Move counter_move{};
        if(prev_move.data)counter_move=counter_moves[pos.side][prev_move.from()][prev_move.to()];
        auto om=ordered(moves,ttm,ply,prev_move);

        const Score orig_alpha=alpha;
        Score best=-INF;
        Move bestm{};
        int move_no=0;

        for(const auto&m:om){
            Piece cap=captured_piece(m);
            const bool quiet=is_quiet(m,cap);
            const bool killer_move=(m==killers[ply][0]||m==killers[ply][1]);
            const bool counter_move_hit=(m==counter_move);

            // Move-level futility pruning: only at shallow non-PV nodes,
            // never for captures, promotions, killers, or counter-moves.
            const int futility_margin=90+90*depth;
            if(!pv_node&&depth<=3&&move_no>0&&quiet&&!killer_move&&!counter_move_hit&&
               static_eval+futility_margin<=alpha){
                history[pos.side][m.from()][m.to()]=
                    std::clamp(history[pos.side][m.from()][m.to()]-depth*depth,-100000,100000);
                ++move_no;
                continue;
            }

            Undo u;
            if(!pos.make(m,u))continue;

            const bool gives_check=pos.in_check(pos.side);
            const bool recapture=cap!=EMPTY&&prev_move.data&&m.to()==prev_move.to();
            const int extension=(gives_check||recapture)?1:0;

            int reduction=0;
            if(move_no>=3&&quiet&&!gives_check&&!killer_move&&!counter_move_hit){
                reduction=1;
                const int hist=history_value(m);
                if(depth>=6&&move_no>=6)++reduction;
                if(depth>=9&&move_no>=10)++reduction;
                if(depth>=12&&move_no>=16)++reduction;
                if(hist<0)++reduction;
                if(pv_node)--reduction;
                reduction=std::clamp(reduction,0,std::max(0,depth-2));
            }

            const int child_depth=depth-1+extension;
            Score sc;

            if(move_no==0){
                sc=-search(child_depth,-beta,-alpha,ply+1,true,m);
            }else{
                const int reduced_depth=std::max(0,child_depth-reduction);
                sc=-search(reduced_depth,-alpha-1,-alpha,ply+1,true,m);
                if(!stop.load()&&sc>alpha&&sc<beta)
                    sc=-search(child_depth,-beta,-alpha,ply+1,true,m);
            }

            pos.undo(u);
            if(stop.load())return 0;

            const Score old_alpha=alpha;
            if(sc>best){
                best=sc;
                bestm=m;
            }

            if(sc>alpha){
                alpha=sc;
                if(quiet)
                    history[pos.side][m.from()][m.to()]=
                        std::clamp(history[pos.side][m.from()][m.to()]+depth*depth*2,-100000,100000);
            }else if(quiet){
                history[pos.side][m.from()][m.to()]=
                    std::clamp(history[pos.side][m.from()][m.to()]-std::max(1,depth),-100000,100000);
            }

            if(alpha>=beta){
                if(quiet){
                    killers[ply][1]=killers[ply][0];
                    killers[ply][0]=m;
                    history[pos.side][m.from()][m.to()]=
                        std::clamp(history[pos.side][m.from()][m.to()]+depth*depth,-100000,100000);
                    if(prev_move.data)
                        counter_moves[pos.side][prev_move.from()][prev_move.to()]=m;
                }
                break;
            }

            (void)old_alpha;
            ++move_no;
        }

        const Bound bound=(best<=orig_alpha?UPPER:(best>=beta?LOWER:EXACT));
        tt.store(pos.key,depth,score_to_tt(best,ply),bound,bestm,static_eval);
        return best;
    }

public:
    Searcher(Board&p,std::size_t hash_mb=64):pos(p),tt(hash_mb){}

    void configure(std::uint64_t ms,std::uint64_t nodes_limit,int depth,const std::vector<Move>&root_filter={}){
        timed=ms>0;
        node_limit=nodes_limit;
        depth_limit=depth;
        stop.store(false);
        nodes=0;
        restricted_root=root_filter;
        tt.new_search();
        start_time=std::chrono::steady_clock::now();
        if(timed)deadline=start_time+std::chrono::milliseconds(ms);
    }

    void request_stop(){stop.store(true);}

    void clear(){
        tt.resize(hash_mb);
        std::memset(killers,0,sizeof(killers));
        std::memset(counter_moves,0,sizeof(counter_moves));
        std::memset(history,0,sizeof(history));
        nodes=0;
        stop.store(false);
    }

    void set_hash_mb(std::size_t mb){
        hash_mb=std::clamp<std::size_t>(mb,1,2048);
        tt.resize(hash_mb);
    }

    std::uint64_t node_count()const{return nodes;}
    int hashfull()const{return tt.hashfull();}

    void debug_reset_nodes(){nodes=0;stop.store(false);}
    TT& debug_tt(){return tt;}
    const TT& debug_tt()const{return tt;}
    Score debug_search(int depth,Score alpha=-INF,Score beta=INF,int ply=0){
        return search(depth,alpha,beta,ply,true,Move{});
    }
    Score debug_eval() const { return eval(); }
    Score debug_quiesce(Score alpha=-INF,Score beta=INF,int ply=0,int qdepth=8){
        return quiesce(alpha,beta,ply,qdepth);
    }
    int debug_see(const Move&m)const{return see(m);}
    int debug_move_score(const Move&m,const Move&ttm={},const Move&counter_move={},int ply=0)const{
        return score_move(m,ttm,counter_move,ply);
    }
    std::vector<Move> debug_ordered(const std::vector<Move>&ms,const Move&ttm={},int ply=0,const Move&prev_move={})const{
        return ordered(ms,ttm,ply,prev_move);
    }

    std::pair<Move,Score> think(){
        auto all_legal=pos.legal();
        if(all_legal.empty())return {Move{},pos.in_check(pos.side)?-MATE:0};

        std::vector<Move> legal;
        if(restricted_root.empty())legal=all_legal;
        else{
            for(const auto&m:all_legal)
                for(const auto&r:restricted_root)
                    if(m==r){legal.push_back(m);break;}
        }
        if(legal.empty())return {Move{},0};

        Move best=legal[0];
        Score bestscore=-INF;

        for(int d=1;d<=depth_limit&&!stop.load();d++){
            const Score center=(d==1||bestscore==-INF)?0:bestscore;
            Score window=(d==1||is_mate_score(bestscore))?INF:50;
            bool accepted=false;

            while(!stop.load()){
                Score alpha=(window>=INF)?-INF:std::max<Score>(-INF,center-window);
                Score beta=(window>=INF)?INF:std::min<Score>(INF,center+window);

                Move root_hint=best;
                if(TTEntry*root_entry=tt.probe(pos.key))
                    if(root_entry->depth>=d-1&&root_entry->move)root_hint.data=root_entry->move;

                const Score window_alpha=alpha;
                const Score window_beta=beta;

                auto root_moves=ordered(legal,root_hint,0,Move{});
                std::vector<std::pair<Move,Score>> roots;
                roots.reserve(root_moves.size());

                int index=0;
                for(const auto&m:root_moves){
                    Undo u;
                    if(!pos.make(m,u))continue;

                    Score sc;
                    if(index==0)
                        sc=-search(d-1,-beta,-alpha,1,true,m);
                    else{
                        sc=-search(d-1,-alpha-1,-alpha,1,true,m);
                        if(!stop.load()&&sc>alpha&&sc<beta)
                            sc=-search(d-1,-beta,-alpha,1,true,m);
                    }

                    pos.undo(u);
                    if(stop.load())break;
                    roots.push_back({m,sc});

                    if(sc>alpha)alpha=sc;
                    ++index;
                }

                if(stop.load())break;

                std::stable_sort(roots.begin(),roots.end(),[](const auto&a,const auto&b){return a.second>b.second;});
                if(roots.empty())break;

                const Score iteration_score=roots[0].second;
                if(window>=INF||(iteration_score>window_alpha&&iteration_score<window_beta)){
                    best=roots[0].first;
                    bestscore=iteration_score;
                    accepted=true;
                    break;
                }

                window*=2;
                if(window>=4000)window=INF;
            }

            if(stop.load()||!accepted)break;

            auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now()-start_time).count();
            std::uint64_t nps=nodes*1000ULL/std::max<long long>(1,ms);
            std::cout<<"info depth "<<d<<" "<<uci_score(bestscore)<<" nodes "<<nodes
                     <<" nps "<<nps<<" hashfull "<<hashfull()<<" pv "<<move_uci(best)<<"\n"<<std::flush;
        }

        return {best,bestscore};
    }

    std::uint64_t perft(int depth){
        if(depth==0)return 1;
        auto ms=pos.legal();
        std::uint64_t n=0;
        for(const auto&m:ms){
            Undo u;
            if(!pos.make(m,u))continue;
            n+=perft(depth-1);
            pos.undo(u);
        }
        return n;
    }
};
static void print_board(const Board&p){for(int r=7;r>=0;r--){std::cout<<r+1<<" ";for(int f=0;f<8;f++){Piece pc=p.b[sq(f,r)];const char* chars=" PNBRQKpnbrqk";std::cout<<chars[pc]<<' ';}std::cout<<'\n';}std::cout<<"  a b c d e f g h\n";}

struct GoLimits {int depth=0;std::uint64_t movetime=0,wtime=0,btime=0,winc=0,binc=0,nodes=0;int movestogo=0;bool infinite=false;std::vector<std::string> searchmoves;};
GoLimits parse_go(std::istringstream&is){
    GoLimits g; std::string t;
    while(is>>t){
        auto read=[&](auto&x){std::string v;if(is>>v)x=std::stoull(v);};
        if(t=="searchmoves"){while(is>>t){if(t=="depth"||t=="movetime"||t=="wtime"||t=="btime"||t=="winc"||t=="binc"||t=="nodes"||t=="movestogo"||t=="infinite"){break;}g.searchmoves.push_back(t);} if(t=="depth")read(g.depth);else if(t=="movetime")read(g.movetime);else if(t=="wtime")read(g.wtime);else if(t=="btime")read(g.btime);else if(t=="winc")read(g.winc);else if(t=="binc")read(g.binc);else if(t=="nodes")read(g.nodes);else if(t=="movestogo")read(g.movestogo);else if(t=="infinite")g.infinite=true;}
        else if(t=="depth")read(g.depth);else if(t=="movetime")read(g.movetime);else if(t=="wtime")read(g.wtime);else if(t=="btime")read(g.btime);else if(t=="winc")read(g.winc);else if(t=="binc")read(g.binc);else if(t=="nodes")read(g.nodes);else if(t=="movestogo")read(g.movestogo);else if(t=="infinite")g.infinite=true;
    }
    return g;
}

Move find_uci(Board&p,const std::string&s){if(s.size()<4)return {};int f=parse_square(s.substr(0,2)),t=parse_square(s.substr(2,2));if(f<0||t<0)return {};int promo=NONE;if(s.size()>=5){switch(std::tolower((unsigned char)s[4])){case'n':promo=KNIGHT;break;case'b':promo=BISHOP;break;case'r':promo=ROOK;break;case'q':promo=QUEEN;break;}}
    for(auto&m:p.legal()) { if(m.from()==f&&m.to()==t&&(promo==NONE||m.promo()==promo)) return m; }
    return {};
}

void uci_loop(){
    init_attacks(); Board pos; Searcher engine(pos); std::thread search_thread; std::string line;
    auto join_search=[&](){ if(search_thread.joinable()) search_thread.join(); };
    auto stop_search=[&](){ engine.request_stop(); join_search(); };
    while(std::getline(std::cin,line)){
        std::istringstream is(line); std::string cmd; is>>cmd; if(cmd.empty())continue;
        if(cmd=="uci"){
            std::cout<<"id name Aquila\nid author Obedience Adara\noption name Hash type spin default 64 min 1 max 2048\noption name Threads type spin default 1 min 1 max 1\nuciok\n"<<std::flush;
        } else if(cmd=="isready"){
            std::cout<<"readyok\n"<<std::flush;
        } else if(cmd=="ucinewgame"){
            stop_search(); pos.set_fen("startpos"); engine.clear();
        } else if(cmd=="setoption"){
            stop_search(); std::string name,value,token; is>>token; if(token=="name") { while(is>>token){ if(token=="value"){is>>value;break;} if(!name.empty())name+=' ';name+=token; } }
            if(name=="Hash"&&!value.empty()){int mb=std::clamp(std::stoi(value),1,2048);engine.set_hash_mb(mb);} 
        } else if(cmd=="position"){
            stop_search(); std::string kind;is>>kind;
            if(kind=="startpos")pos.set_fen("startpos");
            else if(kind=="fen"){std::string f,fen;std::vector<std::string> toks;for(int i=0;i<6&&is>>f;i++)toks.push_back(f);for(size_t i=0;i<toks.size();i++){if(i)fen+=' ';fen+=toks[i];}pos.set_fen(fen);}
            std::string t;if(is>>t&&t=="moves"){while(is>>t){Move m=find_uci(pos,t);if(m.data){Undo u;pos.make(m,u);}}}
        } else if(cmd=="d")print_board(pos);
        else if(cmd=="fen")std::cout<<pos.fen()<<"\n"<<std::flush;
        else if(cmd=="moves"){for(auto&m:pos.legal())std::cout<<move_uci(m)<<" ";std::cout<<"\n"<<std::flush;}
        else if(cmd=="perft"){int d;is>>d;std::uint64_t n=engine.perft(d);std::cout<<"perft "<<d<<" nodes "<<n<<"\n"<<std::flush;}
        else if(cmd=="stop")stop_search();
        else if(cmd=="go"){
            stop_search(); GoLimits g=parse_go(is); std::uint64_t tm=g.movetime;
            if(!tm&&!g.infinite){if(pos.side==WHITE&&g.wtime)tm=g.wtime/(g.movestogo?g.movestogo:30)+(g.winc/2);if(pos.side==BLACK&&g.btime)tm=g.btime/(g.movestogo?g.movestogo:30)+(g.binc/2);}
            int d=g.depth?g.depth:64; std::vector<Move> filter; for(const auto&s:g.searchmoves){Move m=find_uci(pos,s);if(m.data)filter.push_back(m);} engine.configure(tm,g.nodes,d,filter);
            search_thread=std::thread([&](){auto res=engine.think();std::cout<<"bestmove "<<move_uci(res.first)<<"\n"<<std::flush;});
        } else if(cmd=="quit"){stop_search();break;}
    }
    stop_search();
}
}
int main(){try{aquila::uci_loop();}catch(const std::exception&e){std::cerr<<"fatal: "<<e.what()<<"\n";return 1;}return 0;}