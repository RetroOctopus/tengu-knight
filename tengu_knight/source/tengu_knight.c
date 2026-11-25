/* TenguCon 3.0 / ESP32-S3 native ELF game: TENGU KNIGHT.
 * A side-view arena: a knight fights waves of the undead up to The Lich.
 * Original characters and art; no copyrighted game assets.
 * Uses badge_* firmware exports, plus a guarded SD save file. No radios or flash calls.
 * SPDX-License-Identifier: MIT
 */
#include "badge_api.h"
#include <stdint.h>

enum { W=320, H=240, FRAME_MS=35 };
enum { AY0=34, AY1=214, GY=200 };          /* arena top, arena bottom, ground line */

enum { BLACK=0x0000, WHITE=0xffff, RED=0xE187, YELLOW=0xFE89, GOLD=0xCD08,
       ORANGE=0xEC25, GREEN=0x578E, DKGREEN=0x13E5, GREY=0x9CF3, DARK=0x2104,
       CYAN=0x07ff, STEEL=0xEF7F, BONE=0xE71A, SHAFT=0x8B47, SNAKEG=0x3CC9,
       WEBC=0xCE5B, PURPLE=0x494D,
       C_WALL=0x10C5, C_PIL=0x1928, C_CAP=0x29AA, C_FLOOR=0x49C5,
       C_TILE=0x2923, C_GAP=0x1062 };

/* ---- Controls, keyboard, save file ------------------------------------- */
enum { K_UP=0, K_DOWN, K_LEFT, K_RIGHT, K_ATK, K_ITEM, K_BACK, NKEYS };
enum { NSCORES=5, NAME_LEN=8 };
static int32_t keys[NKEYS] = {-1,-1,-1,-1,-1,-1,-1};
static const char * const key_names[NKEYS] = {
    "UP", "DOWN", "LEFT", "RIGHT", "ATTACK (A)", "ITEM (B)", "MENU (ESC OR ANY KEY)"
};
static char hs_name[NSCORES][NAME_LEN+1];
static uint32_t hs_score[NSCORES];
/* Keyboard matrix, from the firmware's own table. A key code is
 * (row << 4) | column with row and column both counted from 1. */
static const char keymap[4][14] = {
    {0,'1','2','3','4','5','6','7','8','9','0','-',0,0},
    {0,'Q','W','E','R','T','Y','U','I','O','P',0,0,0},
    {0,0,'A','S','D','F','G','H','J','K','L',0,0,0},
    {0,0,0,'Z','X','C','V','B','N','M',0,'.',0,' '}
};
enum { KEY_DEL=0x1e, KEY_ENTER=0x3e };
static char key_char(int32_t code) {
    int32_t r=(code>>4)-1, c=(code&15)-1;
    if(code<0 || r<0 || r>3 || c<0 || c>13) return 0;
    return keymap[r][c];
}

/* Save file on the SD card (buttons + high scores).
 * The firmware does not export open/read/write to apps, so they are called
 * at their fixed addresses in TenguCon firmware build 69d9791. Before any
 * call, the firmware's code at those addresses is compared word for word
 * with that build. On any other firmware nothing is saved. */
enum { REC_WORDS = 1 + NKEYS + NSCORES*3 };
#define SAVE_MAGIC 0x31534B54u             /* "TKS1" */
static int32_t keys_loaded;
static void rec_unpack(const uint32_t *rec) {
    int ok=1;
    for(int i=0;i<NKEYS;i++){
        int32_t k=(int32_t)rec[1+i];
        if(k<16 || k>0x7f) ok=0;
        for(int j=0;j<i;j++) if((int32_t)rec[1+j]==k) ok=0;
    }
    if(ok){ for(int i=0;i<NKEYS;i++) keys[i]=(int32_t)rec[1+i]; keys_loaded=1; }
    for(int i=0;i<NSCORES;i++){
        const uint32_t *e = rec + 1 + NKEYS + i*3;
        for(int b=0;b<NAME_LEN;b++){
            char ch=(char)((e[b/4] >> ((b%4)*8)) & 0xffu);
            if(ch && (ch<32 || ch>126)) ch='?';
            hs_name[i][b]=ch;
        }
        hs_name[i][NAME_LEN]='\0';
        hs_score[i]=e[2];
    }
    for(int i=1;i<NSCORES;i++) if(hs_score[i]>hs_score[i-1]) hs_score[i]=hs_score[i-1];
}
static void rec_pack(uint32_t *rec) {
    rec[0]=SAVE_MAGIC;
    for(int i=0;i<NKEYS;i++) rec[1+i]=(uint32_t)keys[i];
    for(int i=0;i<NSCORES;i++){
        uint32_t *e = rec + 1 + NKEYS + i*3;
        e[0]=0; e[1]=0;
        for(int b=0;b<NAME_LEN;b++)
            e[b/4] |= ((uint32_t)(uint8_t)hs_name[i][b]) << ((b%4)*8);
        e[2]=hs_score[i];
    }
}
#ifdef HOST_TEST
static uint32_t host_file[REC_WORDS]; static int host_file_ok;
static int fw_ok(void) { return 1; }
static void store_load(void) { if(host_file_ok) rec_unpack(host_file); }
static void store_save(void) { rec_pack(host_file); host_file_ok=1; }
#else
#define FW_BASE  0x420770a0u   /* open  */
#define FW_CLOSE 0x420770d0u
#define FW_READ  0x420770e4u
#define FW_WRITE 0x420770fcu
static const uint32_t fw_sig[28] = {
    0x4200a136u, 0x61520661u, 0x08616207u, 0x02bd9179u,
    0x0d0c03cdu, 0x920f6367u, 0xc18230c1u, 0x0c019910u,
    0x99118989u, 0x0c04dd21u, 0x055e250au, 0xf01d0a2du,
    0x20004136u, 0xa0a220b2u, 0x0578a500u, 0x1d202aa0u,
    0x000000f0u, 0x20004136u, 0xc33020b2u, 0x20d44020u,
    0x71a50a0cu, 0x1d0a2d05u, 0x000000f0u, 0x20004136u,
    0xc33020b2u, 0x20d44020u, 0x65250a0cu, 0x1d0a2d05u
};
typedef int32_t (*fw_open_fn)(const char *, int32_t, int32_t);
typedef int32_t (*fw_close_fn)(int32_t);
typedef int32_t (*fw_rw_fn)(int32_t, void *, uint32_t);
static const char save_path[] = "/sdcard/TKNIGHT.SAV";
static int fw_ok(void) {
    const volatile uint32_t *code = (const volatile uint32_t *)FW_BASE;
    for(int i=0;i<28;i++) if(code[i]!=fw_sig[i]) return 0;
    return 1;
}
static void store_load(void) {
    uint32_t rec[REC_WORDS];
    int32_t fd, n;
    if(!fw_ok()) return;
    fd = ((fw_open_fn)FW_BASE)(save_path, 0, 0);          /* read only */
    if(fd<0) return;
    n = ((fw_rw_fn)FW_READ)(fd, rec, (uint32_t)sizeof rec);
    ((fw_close_fn)FW_CLOSE)(fd);
    if(n==(int32_t)sizeof rec && rec[0]==SAVE_MAGIC) rec_unpack(rec);
}
static void store_save(void) {
    uint32_t rec[REC_WORDS];
    int32_t fd;
    if(!fw_ok()) return;
    rec_pack(rec);
    fd = ((fw_open_fn)FW_BASE)(save_path, 0x42, 0x1b6);   /* read/write, create */
    if(fd<0) return;
    ((fw_rw_fn)FW_WRITE)(fd, rec, (uint32_t)sizeof rec);
    ((fw_close_fn)FW_CLOSE)(fd);
}
#endif

/* ---- Drawing primitives ------------------------------------------------- */
/* The firmware scans the key matrix only when an app asks for keys (seen in
 * its code), so a tap that starts and ends while a frame is being drawn is
 * lost. During play, rect() therefore asks for keys every few milliseconds. */
static int32_t in_play;
static uint32_t last_poll;
static void poll_keys(void);
static void rect(int32_t x,int32_t y,int32_t w,int32_t h,uint16_t c) {
    if(in_play){
        uint32_t now=badge_millis();
        if((uint32_t)(now-last_poll)>=3u){ last_poll=now; poll_keys(); }
    }
    if(x<0){w+=x;x=0;}
    if(y<0){h+=y;y=0;}
    if(x+w>W)w=W-x;
    if(y+h>H)h=H-y;
    if(w<=0 || h<=0)return;
    /* The badge drops part of a large fill (seen on hardware above roughly
     * 10,000 pixels), so big areas are sent as narrow strips. */
    while(w*h > 4000 && w > 1){
        int32_t strip = 4000/h;
        if(strip<1)strip=1;
        if(strip>w)strip=w;
        badge_fill_rect(x,y,strip,h,c);
        x+=strip; w-=strip;
    }
    if(w>0)badge_fill_rect(x,y,w,h,c);
}
static void text(int32_t x,int32_t y,const char *t,uint16_t c) {
    badge_draw_text(x,y,t,c,0);
}
static void print_num(int32_t x, int32_t y, uint32_t n, uint16_t color) {
    char b[12]; int32_t i=10;
    b[11]='\0';
    do { b[i--] = (char)('0' + n%10u); n/=10u; } while(n && i>=0);
    badge_draw_text(x,y,b+i+1,color,0);
}
static int32_t slen(const char *s){ int32_t n=0; while(s[n]) n++; return n; }

/* The arena is composed off screen every frame as a grid of 2x2-pixel cells
 * (one palette byte per cell). Only cells whose colour differs from what is
 * already on the display are sent, each exactly once, so nothing is erased
 * and redrawn: no flicker. */
enum { CW=160, CH=90, C_BANNER=255 };
static uint8_t cur[CH][CW], prv[CH][CW];
static uint16_t palette[80];
static int32_t npal;
static int g_hud;
static uint8_t pal_index(uint16_t c) {
    for(int32_t i=0;i<npal;i++) if(palette[i]==c) return (uint8_t)(i+1);
    if(npal<80){ palette[npal]=c; npal++; return (uint8_t)npal; }
    return 1;
}
static void cells(int32_t x,int32_t y,int32_t w,int32_t h,uint8_t v) {
    int32_t x0, y0, x1, y1;
    if(w<=0 || h<=0) return;
    y-=AY0;
    x0 = x<0 ? 0 : x>>1;            y0 = y<0 ? 0 : y>>1;
    x1 = x+w<=0 ? 0 : (x+w+1)>>1;   y1 = y+h<=0 ? 0 : (y+h+1)>>1;
    if(x1>CW)x1=CW;
    if(y1>CH)y1=CH;
    for(int32_t j=y0;j<y1;j++) for(int32_t i=x0;i<x1;i++) cur[j][i]=v;
}
static void prim(int32_t x,int32_t y,int32_t w,int32_t h,uint16_t c) {
    if(g_hud){ rect(x,y,w,h,c); return; }
    cells(x,y,w,h,pal_index(c));
}
/* bar of length len starting at x0 and running in direction s (+1/-1) */
static void hbar(int32_t x0,int32_t y,int32_t len,int32_t s,int32_t h,uint16_t c) {
    if(s>0) prim(x0,y,len,h,c); else prim(x0-len,y,len,h,c);
}
/* rectangle placed dx pixels in front of x (mirrors with s) */
static void rel(int32_t x,int32_t dx,int32_t y,int32_t w,int32_t h,int32_t s,uint16_t c) {
    if(s>0) prim(x+dx,y,w,h,c); else prim(x-dx-w,y,w,h,c);
}

/* ---- Sprites: rows of palette letters, '.' is transparent -------------- */
typedef struct { int32_t w, h; const char * const *rows; } Spr;
static uint16_t g_bone = BONE;
static uint16_t pal(char ch, uint16_t eye) {
    switch(ch){
    case 'W': return WHITE;   case 'B': return g_bone;    case 'D': return 0x2124;
    case 'H': return 0xBE19;  case 'A': return 0x8CB5;  case 'L': return 0x5B2E;
    case 'N': return 0x3984;  case 'S': return 0x2B7A;  case 'R': return RED;
    case 'Y': return YELLOW;  case 'O': return GOLD;    case 'E': return STEEL;
    case 'K': return 0x4228;  case 'J': return 0x7BF0;  case 'P': return 0x3949;
    case 'Q': return 0x6A4F;  case 'V': return SNAKEG;  case 'U': return 0xAECE;
    case 'M': return 0x28C7;  case 'X': return PURPLE;  case 'e': return eye;
    default:  return WHITE;
    }
}
static uint16_t spr_px(const Spr *s,int32_t j,int32_t i,int flip,uint16_t tint,uint16_t eye) {
    char ch=s->rows[j][flip ? s->w-1-i : i];
    if(ch=='.') return 1;                       /* 1 = transparent marker */
    return tint ? tint : pal(ch,eye);
}
/* does row j have a run of exactly this colour over [i, i+run)? */
static int spr_same(const Spr *s,int32_t j,int32_t i,int32_t run,uint16_t col,
                    int flip,uint16_t tint,uint16_t eye) {
    if(j<0 || j>=s->h) return 0;
    if(i>0 && spr_px(s,j,i-1,flip,tint,eye)==col) return 0;
    if(i+run<s->w && spr_px(s,j,i+run,flip,tint,eye)==col) return 0;
    for(int32_t k=i;k<i+run;k++) if(spr_px(s,j,k,flip,tint,eye)!=col) return 0;
    return 1;
}
static void spr(const Spr *s,int32_t left,int32_t top,int32_t b,int flip,
                uint16_t tint,uint16_t eye) {
    for(int32_t j=0;j<s->h;j++){
        int32_t i=0;
        while(i<s->w){
            uint16_t col=spr_px(s,j,i,flip,tint,eye);
            int32_t run=1, tall=1;
            if(col==1){ i++; continue; }
            while(i+run<s->w && spr_px(s,j,i+run,flip,tint,eye)==col) run++;
            if(!spr_same(s,j-1,i,run,col,flip,tint,eye)){
                while(spr_same(s,j+tall,i,run,col,flip,tint,eye)) tall++;
                prim(left+i*b, top+j*b, run*b, tall*b, col);
            }
            i+=run;
        }
    }
}

static const char * const KN_IDLE[] = {
    "...HHHH.....", "..HHHHWW....", "..HHRDRRRR..", "..HHRRR.....",
    "..AAAAA.SS..", ".AAAAAAASSS.", ".AAAAAAASSS.", "..AAAAA.SSS.",
    "..AAAAA.SS..", "...L..L.....", "...L..L.....", "..NN..NN...."};
static const char * const KN_W1[] = {
    "...HHHH.....", "..HHHHWW....", "..HHRDRRRR..", "..HHRRR.....",
    "..SAAAA.....", ".SSAAAAA....", ".SSAAAAA....", ".SSAAAA.....",
    "..SAAAA.....", "...L...L....", "..L.....L...", ".NN.....NN.."};
static const char * const KN_W2[] = {
    "...HHHH.....", "..HHHHWW....", "..HHRDRRRR..", "..HHRRR.....",
    "..SAAAA.....", ".SSAAAAA....", ".SSAAAAA....", ".SSAAAA.....",
    "..SAAAA.....", "....LL......", "....LL......", "...NNN......"};
static const char * const KN_WIND[] = {
    "...HHHH.....", "..HHHHWW....", "..HHRDRRRR..", "..HHRRR.....",
    "..AAAAA.SS..", "OAAAAAAASSS.", ".AAAAAAASSS.", "..AAAAA.SSS.",
    "..AAAAA.SS..", "..L....L....", "..L....L....", ".NN....NN..."};
static const char * const KN_EXT[] = {
    "...HHHH.....", "..HHHHWW....", "..HHRDRRRR..", "..HHRRR.....",
    "..SAAAA.....", ".SSAAAAAAAOO", ".SSAAAAA....", ".SSAAAA.....",
    "..SAAAA.....", "..L....L....", "..L.....L...", ".NN.....NN.."};
static const char * const KN_HIGH[] = {
    "...HHHH.....", "..HHHHWW....", "..HHRDRRRR..", "..HHRRR...O.",
    "..SAAAAAAA..", ".SSAAAAA....", ".SSAAAAA....", ".SSAAAA.....",
    "..SAAAA.....", "..L....L....", "..L....L....", ".NN....NN..."};
static const char * const KN_LOW[] = {
    "...HHHH.....", "..HHHHWW....", "..HHRDRRRR..", "..HHRRR.....",
    "..SAAAA.....", ".SSAAAAA....", ".SSAAAAAA...", ".SSAAAA.AO..",
    "..SAAAA.....", "..L....L....", ".L......L...", "NN......NN.."};
static const char * const KN_THROW[] = {
    "...HHHH...OO", "..HHHHWW..A.", "..HHRDRRRRA.", "..HHRRR..A..",
    "..SAAAAAA...", ".SSAAAAA....", ".SSAAAAA....", ".SSAAAA.....",
    "..SAAAA.....", "...L..L.....", "...L..L.....", "..NN..NN...."};
static const Spr sp_kn_idle={12,12,KN_IDLE}, sp_kn_w1={12,12,KN_W1},
    sp_kn_w2={12,12,KN_W2}, sp_kn_wind={12,12,KN_WIND}, sp_kn_ext={12,12,KN_EXT},
    sp_kn_high={12,12,KN_HIGH}, sp_kn_low={12,12,KN_LOW}, sp_kn_throw={12,12,KN_THROW};

static const char * const SK_A[] = {
    "...BBBB...", "..BeBBeB..", "..BBBBBB..", "...BDDB...",
    "....BB...E", "..BBBBBB.E", ".B.BBBB.BE", ".B.B..B.BO",
    "...BBBB...", "...B..B...", "...B..B...", "..BB..BB.."};
static const char * const SK_B[] = {
    "...BBBB...", "..BeBBeB..", "..BBBBBB..", "...BDDB...",
    "....BB...E", "..BBBBBB.E", ".B.BBBB.BE", ".B.B..B.BO",
    "...BBBB...", "..B....B..", ".B......B.", "BB......BB"};
static const char * const SK_WIND[] = {
    "...BBBB..E", "..BeBBeB.E", "..BBBBBB.E", "...BDDB..O",
    "....BB..B.", "..BBBBBB..", ".B.BBBB...", ".B.B..B...",
    "...BBBB...", "...B..B...", "...B..B...", "..BB..BB.."};
static const char * const SK_STRIKE[] = {
    "...BBBB...", "..BeBBeB..", "..BBBBBB..", "...BDDB...",
    "....BB....", "..BBBBBB..", ".B.BBBBBBO", ".B.B..B...",
    "...BBBB...", "...B..B...", "...B..B...", "..BB..BB.."};
static const Spr sp_sk_a={10,12,SK_A}, sp_sk_b={10,12,SK_B},
    sp_sk_wind={10,12,SK_WIND}, sp_sk_strike={10,12,SK_STRIKE};

static const char * const SKULL[] = {
    ".BBBBBB.", "BBBBBBBB", "BeeBBeeB", "BeeBBeeB",
    "BBBDDBBB", ".BBBBBB.", ".B.BB.B."};
static const Spr sp_skull={8,7,SKULL};

static const char * const BK[] = {
    "....KKKK....", "...KKKKKK...", "...KKKRRK...", "...KKKKKK...",
    "....JKKJ....", "..KKKKKKKK..", ".KKJKKKKJKK.", ".KK.KKKK.KK.",
    ".KK.KKKK.KK.", "....KJJK....", "....K..K....", "....K..K....",
    "...KK..KK...", "...KK..KK..."};
static const Spr sp_bk={12,14,BK};

static const char * const SP_A[] = {
    "....PPPP......", "...PPPPPPRR...", "..PPPPPPPRRP..", "Q..PPPPPPPP..Q",
    ".QQ.Q.QQ.Q.QQ.", "Q..Q..Q..Q..Q.", "Q...Q.Q.Q...Q."};
static const char * const SP_B[] = {
    "....PPPP......", "...PPPPPPRR...", "..PPPPPPPRRP..", ".Q.PPPPPPPP.Q.",
    "Q.Q.Q.QQ.Q.Q.Q", ".Q..Q..Q..Q..Q", ".Q...Q.Q.Q..Q."};
static const char * const SP_REAR[] = {
    "..........RR..", "...PPPPPPRRP..", "..PPPPPPPPPP..", ".QPPPPPPPP.Q..",
    "Q.Q.Q.QQ.Q..Q.", ".Q..Q..Q..Q..Q", ".Q...Q.Q.Q..Q."};
static const Spr sp_sp_a={14,7,SP_A}, sp_sp_b={14,7,SP_B}, sp_sp_rear={14,7,SP_REAR};

static const char * const SN_A[] = {
    "...VVVV...", "..VVVVYV..", "..VVVVVVR.", "...VVVV...",
    "....VV...E", "..VVUUVV.E", ".V.VUUV.VE", ".V.VUUV.VO",
    "...VUUV...", "...VVVV...", "....VVVV..", "..VVVVVV..",
    ".VVVVVVVV."};
static const char * const SN_B[] = {
    "...VVVV...", "..VVVVYV..", "..VVVVVVR.", "...VVVV...",
    "....VV...E", "..VVUUVV.E", ".V.VUUV.VE", ".V.VUUV.VO",
    "...VUUV...", "...VVVV...", "..VVVV....", "...VVVVVV.",
    ".VVVVVVVV."};
static const char * const SN_WIND[] = {
    "...VVVV..E", "..VVVVYV.E", "..VVVVVV.E", "...VVVV..O",
    "....VV..V.", "..VVUUVV..", ".V.VUUV...", ".V.VUUV...",
    "...VUUV...", "...VVVV...", "....VVVV..", "..VVVVVV..",
    ".VVVVVVVV."};
static const char * const SN_STRIKE[] = {
    "...VVVV...", "..VVVVYV..", "..VVVVVVR.", "...VVVV...",
    "....VV....", "..VVUUVV..", ".V.VUUVVVO", ".V.VUUV...",
    "...VUUV...", "...VVVV...", "....VVVV..", "..VVVVVV..",
    ".VVVVVVVV."};
static const char * const SN_NECK[] = {
    "..........", "..........", "..........", "..........",
    "....VV....", "..VVUUVV..", ".V.VUUV.V.", ".V.VUUV.V.",
    "...VUUV...", "...VVVV...", "....VVVV..", "..VVVVVV..",
    ".VVVVVVVV."};
static const Spr sp_sn_a={10,13,SN_A}, sp_sn_b={10,13,SN_B},
    sp_sn_wind={10,13,SN_WIND}, sp_sn_strike={10,13,SN_STRIKE},
    sp_sn_neck={10,13,SN_NECK};

static const char * const LICH[] = {
    ".....MMMMM......", "....MMMMMMM.....", "...MMXXXXXMM....", "...MXBBBBBXM....",
    "...MXBeBBeBM....", "...MXBBBBBXM....", "...MXXBDDBXM....", "..MMMXXXXXMMM...",
    ".MMMMMMMMMMMMM..", ".MMXMMMMMMMXMM..", "MMMXMMMMMMMXMMM.", "MMMXMMMMMMMXMMM.",
    "MMXMMMMMMMMMXMM.", "MMXMMMMMMMMMXMM.", "MXMMMMMMMMMMMXM.", "MXMMMMMMMMMMMXM.",
    "MXMXMXMXMXMXMXM.", ".X.X.X.X.X.X.X.."};
static const Spr sp_lich={16,18,LICH};

/* ---- Game objects ------------------------------------------------------ */
enum { O_NONE=0, O_PLAYER, O_SKEL, O_RSKEL, O_SKULL, O_BKNIGHT, O_SPIDER, O_SNAKE,
       O_LICH, O_FBALL, O_GBALL, O_WEB, O_KNIFE, O_POTION, O_FIRE, O_EFIRE,
       O_GZONE, O_PICKUP, O_FX, O_BANNER };
typedef struct {
    int32_t type, st, face, sub;
    int32_t x, y, vx, vy;
    int32_t hp, t, t2, flash;
    int32_t a, b, hit_id, summoned;
    int32_t burn, life, stun;
} Obj;
enum { NOBJ=40 };
static Obj ob[NOBJ];

enum { P_NORM=0, P_ATK, P_HURT, P_WEB, P_THROW, P_DEAD, P_BASH };
enum { E_WALK=0, E_WIND, E_STRIKE, E_REC, E_DODGE, E_HURT, E_DIE, E_RUSH, E_ENTER };
enum { L_IDLE=20, L_SWIPE_W, L_SWIPE, L_SWIPE_R, L_BASH_W, L_BASH, L_BASH_R,
       L_ZONE, L_BALL_W, L_BALL, L_SUMMON_W, L_RAGE_W, L_RAGE };
enum { IT_KNIFE=0, IT_CROSS, IT_FIRE, IT_HEAL, NITEMS };
/* Sword combo: four swings, each about a quarter of a second. */
enum { SWING_MS=260, FINISH_MS=330, HIT_FROM=60, HIT_TO=200, COMBO_GAP_MS=350, BUFFER_MS=300 };
enum { SWORD_REACH=28 };                  /* from the knight's centre, in pixels */
/* Damage units: one sword hit = 2, one knife = 1. */
enum { DMG_SWORD=2, DMG_FINISH=3, DMG_KNIFE=1 };
enum { KILL_GOAL=50, EACH_GOAL=5, NKINDS=5 };
enum { LICH_HP=40 };                      /* 20 sword hits */
enum { BASH_REACH=14, BASH_MS=220, STUN_MS=1500 };
static const char * const item_names[NITEMS] = {"KNIVES","CROSS","FIRE FLASK","HEAL POTION"};
static const int32_t item_amount[NITEMS] = {2, 1, 1, 1};
static const int32_t item_max[NITEMS]    = {99, 6, 2, 2};

static int32_t cnt[NITEMS], sel;
static int32_t pl_inv, pl_block, pl_moving, walk_anim, thrust_id, atk_fresh;
static int32_t buf_atk, buf_item, combo, combo_t;
static uint32_t score, seed;
static int32_t gt, gstate, gtimer, spawn_cd, spawn_side;
static int32_t kills[NKINDS], total_kills, lich_on;   /* skeleton, red-eye, spider, snake, black knight */
static int banner_dirty;
static char banner[24];
#ifdef HOST_TEST
static int god;
#endif

static uint32_t rnd(void) {
    seed = seed*1664525u + 1013904223u;
    return seed >> 16;
}
static int32_t iabs(int32_t v){ return v<0 ? -v : v; }
static int is_enemy(int32_t t){ return t>=O_SKEL && t<=O_LICH; }
static int is_small(int32_t t){ return t==O_SKEL||t==O_RSKEL||t==O_SKULL||t==O_SPIDER||t==O_SNAKE; }

static void obj_clear(Obj *o) {
    int32_t *w=(int32_t *)o;
    for(int i=0;i<(int)(sizeof(Obj)/sizeof(int32_t));i++) w[i]=0;
}
static Obj *spawn(int32_t type,int32_t x,int32_t y) {
    for(int i=1;i<NOBJ;i++) if(ob[i].type==O_NONE){
        obj_clear(&ob[i]);
        ob[i].type=type; ob[i].x=x; ob[i].y=y; ob[i].face=1;
        return &ob[i];
    }
    return 0;
}
static void fx(int32_t kind,int32_t x,int32_t y,int32_t life) {
    Obj *o=spawn(O_FX,x,y);
    if(o){ o->a=kind; o->life=life; }
}
static void hurtbox(const Obj *o,int32_t *x0,int32_t *y0,int32_t *x1,int32_t *y1) {
    int32_t hw=7, h=24;
    if(o->type==O_SKULL){ *x0=o->x-7; *x1=o->x+7; *y0=o->y-7; *y1=o->y+7; return; }
    if(o->type==O_BKNIGHT){ hw=9; h=28; }
    else if(o->type==O_SPIDER){ hw=12; h=14; }
    else if(o->type==O_SNAKE){ hw=8; h=26; }
    else if(o->type==O_LICH){ hw=22; h=72; }
    *x0=o->x-hw; *x1=o->x+hw; *y0=GY-h; *y1=GY;
}
static int overlap(int32_t ax0,int32_t ay0,int32_t ax1,int32_t ay1,
                   int32_t bx0,int32_t by0,int32_t bx1,int32_t by1) {
    return ax0<bx1 && bx0<ax1 && ay0<by1 && by0<ay1;
}
static int hits_player(int32_t x0,int32_t y0,int32_t x1,int32_t y1) {
    const Obj *p=&ob[0];
    return overlap(x0,y0,x1,y1, p->x-7,GY-24,p->x+7,GY);
}
static int32_t pdist(const Obj *e){ return iabs(ob[0].x - e->x); }
static void face_player(Obj *e){ e->face = ob[0].x>=e->x ? 1 : -1; }
static int in_reach(const Obj *e,int32_t reach) {
    int32_t dx=ob[0].x-e->x;
    return dx*e->face>=0 && iabs(dx)<=reach;
}

/* ---- Damage ---------------------------------------------------------- */
static int hurt_player(int32_t dmg,int32_t sx,int blockable) {
    Obj *p=&ob[0];
    if(p->st==P_DEAD) return 0;
    if(blockable && pl_block && ((sx>=p->x) == (p->face>0))){
        fx(0, p->x + 14*p->face, GY-14, 140);
        p->x += sx>=p->x ? -3 : 3;
        return 0;
    }
    if(pl_inv>0) return 0;
    pl_inv=1000;
#ifdef HOST_TEST
    if(god) return 1;
#endif
    p->hp-=dmg;
    if(p->hp<=0){ p->hp=0; p->st=P_DEAD; p->t=0; return 1; }
    if(p->st!=P_WEB){ p->st=P_HURT; p->t=0; p->vx = sx>=p->x ? -3 : 3; }
    return 1;
}
static uint32_t points(int32_t t) {
    switch(t){
    case O_SKEL: return 100u;   case O_RSKEL: return 150u;  case O_SKULL: return 50u;
    case O_BKNIGHT: return 600u; case O_SPIDER: return 150u; case O_SNAKE: return 200u;
    case O_LICH: return 3000u;  default: return 0u;
    }
}
static void drop_item(int32_t x,int32_t sure) {
    Obj *o;
    int32_t r=(int32_t)(rnd()%100u), kind;
    if(!sure && r>=40) return;
    r=(int32_t)(rnd()%100u);
    kind = r<45 ? IT_KNIFE : r<70 ? IT_CROSS : r<88 ? IT_FIRE : IT_HEAL;
    if(x<16)x=16;
    if(x>304)x=304;
    o=spawn(O_PICKUP,x,GY);
    if(o){ o->a=kind; o->life=9000; }
}
static int32_t kind_of(int32_t type) {
    return type==O_SKEL ? 0 : type==O_RSKEL ? 1 : type==O_SPIDER ? 2 :
           type==O_SNAKE ? 3 : type==O_BKNIGHT ? 4 : -1;
}
static void kill_enemy(Obj *e);
static void hurt_enemy(Obj *e,int32_t dmg,int32_t sx,int knock) {
    if(e->st==E_DIE || e->st==E_ENTER) return;
    e->hp-=dmg; e->flash=180;
    if(e->hp<=0){ kill_enemy(e); return; }
    if(knock){
        /* light enemies fly back, heavier ones less, the black knight and Lich not at all */
        int32_t kb = (e->type==O_SPIDER||e->type==O_SKULL) ? 16 :
                     (e->type==O_SKEL||e->type==O_RSKEL) ? 8 :
                     e->type==O_SNAKE ? 6 : 0;
        e->x += sx<e->x ? kb : -kb;
        if(e->x<-20)e->x=-20;
        if(e->x>340)e->x=340;
        if(e->type==O_SPIDER||e->type==O_SKULL){ e->st=E_HURT; e->t=0; }   /* and are stunned */
    }
    if((e->type==O_SKEL||e->type==O_RSKEL||e->type==O_SPIDER||e->type==O_SNAKE)
       && e->st==E_WIND){ e->st=E_HURT; e->t=0; }
}
static Obj *init_enemy(int32_t type,int32_t x);
static void kill_enemy(Obj *e) {
    e->st=E_DIE; e->t=0; e->flash=0;
    score+=points(e->type);
    if(!e->summoned && e->type!=O_LICH){
        int32_t k=kind_of(e->type);
        if(k>=0) kills[k]++;
        total_kills++;
    }
    if(e->type==O_RSKEL){
        Obj *s=init_enemy(O_SKULL,e->x);
        if(s){ s->summoned=e->summoned; }
    }
    if(e->type==O_LICH){
        for(int i=1;i<NOBJ;i++){
            Obj *o=&ob[i];
            if(o!=e && is_enemy(o->type) && o->st!=E_DIE){ o->st=E_DIE; o->t=0; }
            if(o->type==O_GBALL||o->type==O_FBALL||o->type==O_EFIRE||o->type==O_GZONE||o->type==O_WEB)
                o->type=O_NONE;
        }
        return;
    }
    if(!e->summoned){
        drop_item(e->x, e->type==O_BKNIGHT);
        if(rnd()%100u<40u){                            /* half a heart, used on pickup */
            int32_t hx=e->x+14;
            Obj *h;
            if(hx<16)hx=16;
            if(hx>304)hx=304;
            h=spawn(O_PICKUP,hx,GY);
            if(h){ h->a=NITEMS; h->life=9000; }
        }
    }
}
static Obj *init_enemy(int32_t type,int32_t x) {
    Obj *e=spawn(type,x,GY);
    if(!e) return 0;
    switch(type){
    /* hp in damage units: 2 per sword hit */
    case O_SKEL:    e->hp=6; break;                       /* 3 hits */
    case O_RSKEL:   e->hp=6; break;
    case O_SKULL:   e->hp=4; e->y=182; e->t2=1500; break; /* 2 hits */
    case O_BKNIGHT: e->hp=14; break;                      /* 7 hits */
    case O_SPIDER:  e->hp=4; e->t2=1500; break;           /* 2 hits */
    case O_SNAKE:   e->hp=10; break;                      /* 5 hits */
    case O_LICH:    e->hp=LICH_HP; e->st=E_ENTER; e->t2=1200; break;
    }
    face_player(e);
    return e;
}

/* ---- Player ---------------------------------------------------------- */
static void cycle_item(void) {
    /* shield (sel = -1), then each item you hold, then back to the shield */
    for(int32_t i=sel+1;i<NITEMS;i++) if(cnt[i]>0){ sel=i; return; }
    sel=-1;
}
/* Shield bash: half the sword's reach, no damage, stuns for 1.5 seconds.
 * The black knight and the Lich shrug it off. */
static void shield_bash(void) {
    const Obj *p=&ob[0];
    int32_t x0 = p->face>0 ? p->x : p->x-BASH_REACH;
    for(int i=1;i<NOBJ;i++){
        Obj *e=&ob[i];
        int32_t a,b,c,d;
        if(!is_enemy(e->type) || e->st==E_DIE || e->st==E_ENTER) continue;
        if(e->type==O_BKNIGHT || e->type==O_LICH) continue;
        hurtbox(e,&a,&b,&c,&d);
        if(overlap(x0,GY-30,x0+BASH_REACH,GY, a,b,c,d)){
            e->stun=STUN_MS; e->st=E_WALK; e->t=0; e->flash=120;
            if(e->type==O_SNAKE) e->b=0;
            fx(0, e->x, d-(d-b)/2, 160);
        }
    }
}
static void cross_blast(void) {
    Obj *p=&ob[0];
    fx(1, p->x, GY-12, 350);
    if(p->st==P_WEB){ p->st=P_NORM; p->t=0; pl_inv=600; }     /* burns the web away */
    for(int i=1;i<NOBJ;i++){
        Obj *e=&ob[i];
        if(e->type==O_WEB && iabs(e->x - p->x)<=60){ e->type=O_NONE; continue; }
        if(!is_enemy(e->type) || e->st==E_DIE || e->st==E_ENTER) continue;
        if(iabs(e->x - p->x) > 58) continue;
        if(is_small(e->type)) hurt_enemy(e, 99, p->x, 0);
        else if(e->type==O_BKNIGHT) hurt_enemy(e, 8, p->x, 0);
    }
}
static int use_item(void) {
    Obj *p=&ob[0], *o;
    int32_t s=p->face;
    if(sel<0){ shield_bash(); p->st=P_BASH; p->t=0; return 1; }
    if(cnt[sel]<=0) return 0;
    switch(sel){
    case IT_KNIFE:
        o=spawn(O_KNIFE, p->x+12*s, GY-14);
        if(o){ o->vx=8*s; o->face=s; }
        break;
    case IT_CROSS:
        cross_blast();
        break;
    case IT_FIRE:
        o=spawn(O_POTION, p->x+8*s, GY-22);
        if(o){ o->vx=4*s; o->vy=-6; }
        break;
    default:
        p->hp+=4;
        if(p->hp>10) p->hp=10;
        fx(2, p->x, GY-30, 500);
        break;
    }
    if(--cnt[sel]<=0) cycle_item();
    p->st=P_THROW; p->t=0;
    return 1;
}
static void sword_hits(void) {
    const Obj *p=&ob[0];
    int32_t x0 = p->face>0 ? p->x+6 : p->x-SWORD_REACH;
    for(int i=1;i<NOBJ;i++){                    /* a swing cuts a web out of the air */
        Obj *w=&ob[i];
        if(w->type==O_WEB && w->x>=x0-4 && w->x<x0+SWORD_REACH-2){
            w->type=O_NONE;
            fx(0, w->x, w->y, 140);
        }
    }
    for(int i=1;i<NOBJ;i++){
        Obj *e=&ob[i];
        int32_t a,b,c,d;
        if(!is_enemy(e->type) || e->st==E_DIE || e->hit_id==thrust_id) continue;
        hurtbox(e,&a,&b,&c,&d);
        if(overlap(x0,GY-30,x0+SWORD_REACH-6,GY, a,b,c,d)){
            e->hit_id=thrust_id;
            hurt_enemy(e, p->a==3 ? DMG_FINISH : DMG_SWORD, p->x, 1);
        }
    }
}
static void player_step(int left,int right,int atk,int item,int swap) {
    Obj *p=&ob[0];
    if(pl_inv>0) pl_inv-=FRAME_MS;
    p->t+=FRAME_MS;
    pl_block=0; pl_moving=0; atk_fresh=0;
    if(swap) cycle_item();
    if(buf_atk>0) buf_atk-=FRAME_MS;
    if(buf_item>0) buf_item-=FRAME_MS;
    if(atk) buf_atk=BUFFER_MS;
    if(item) buf_item=BUFFER_MS;
    switch(p->st){
    case P_DEAD:
        return;
    case P_WEB:
        if(buf_item>0 && sel==IT_CROSS && cnt[IT_CROSS]>0){ buf_item=0; use_item(); return; }
        if(atk && ++p->b>=6) p->t=99999;
        if(p->t>=5000){ p->st=P_NORM; p->t=0; pl_inv=600; }
        return;
    case P_HURT:
        p->x+=p->vx;
        if(p->x<12)p->x=12;
        if(p->x>308)p->x=308;
        if(p->t>=250){ p->st=P_NORM; p->t=0; }
        return;
    case P_ATK:
        /* free to walk and turn mid-swing */
        if(left && !right){ p->face=-1; p->x-=3; }
        else if(right && !left){ p->face=1; p->x+=3; }
        if(p->x<12)p->x=12;
        if(p->x>308)p->x=308;
        if(p->t>=HIT_FROM && p->t<HIT_TO) sword_hits();
        if(p->t>=(p->a==3 ? FINISH_MS : SWING_MS)){ p->st=P_NORM; p->t=0; combo_t=COMBO_GAP_MS; }
        return;
    case P_THROW:
        if(p->t>=140){ p->st=P_NORM; p->t=0; }
        return;
    case P_BASH:
        pl_block=1;
        if(p->t>=BASH_MS){ p->st=P_NORM; p->t=0; }
        return;
    }
    if(combo_t>0){ combo_t-=FRAME_MS; if(combo_t<=0) combo=0; }
    if(buf_atk>0){
        /* mash A: horizontal slash, rising slash, falling slash, overhead */
        buf_atk=0; p->st=P_ATK; p->t=0; p->a=combo; combo=(combo+1)&3;
        thrust_id++; atk_fresh=1;
        return;
    }
    if(buf_item>0){ buf_item=0; if(use_item()) return; }
    if(left && !right){ p->face=-1; p->x-=3; pl_moving=1; }
    else if(right && !left){ p->face=1; p->x+=3; pl_moving=1; }
    if(p->x<12)p->x=12;
    if(p->x>308)p->x=308;
    if(pl_moving) walk_anim+=FRAME_MS;
    else pl_block=1;            /* standing still: shield up */
}

/* ---- Enemies --------------------------------------------------------- */
static void step_toward(Obj *e,int32_t speed){ e->x += e->face*speed; }
static int32_t count_minions(void) {
    int32_t n=0;
    for(int i=1;i<NOBJ;i++) if(is_enemy(ob[i].type) && ob[i].type!=O_LICH && ob[i].st!=E_DIE) n++;
    return n;
}
static int tick2(void){ return (gt/FRAME_MS)&1; }
static int tick4(void){ return ((gt/FRAME_MS)&3)==0; }
/* skeletons step on odd frames: skip one in twenty of those (5% slower) */
static int skel_tick(void){ int32_t n=gt/FRAME_MS; return (n&1) && n%40!=39; }
/* spiders move every frame: skip one in twenty (5% slower) */
static int spider_go(void){ return (gt/FRAME_MS)%20!=0; }
static void enemy_walk_in(Obj *e,int32_t speed) {
    /* enemies that start off screen just walk in */
    face_player(e);
    if(e->type==O_SPIDER){ if(spider_go()) step_toward(e,speed); }
    else if(speed>1) step_toward(e,speed);
    else if(e->type==O_SKEL||e->type==O_RSKEL){ if(skel_tick()) step_toward(e,3); }
    else if(tick2()) step_toward(e,2);
}
static void skel_step(Obj *e) {
    const Obj *p=&ob[0];
    int32_t d=pdist(e);
    switch(e->st){
    case E_WALK:
        face_player(e);
        if(e->sub>0) e->sub-=FRAME_MS;
        if(e->t2>0) e->t2-=FRAME_MS;
        if(atk_fresh && d<40 && e->t2<=0 && rnd()%100u<30u){
            e->st=E_DODGE; e->t=0; e->t2=1800; break;
        }
        if(d>22 || p->st==P_DEAD){ if(skel_tick()) step_toward(e,3); }
        else if(e->sub<=0){ e->st=E_WIND; e->t=0; }
        break;
    case E_WIND:
        if(e->t>=260){
            e->st=E_STRIKE; e->t=0;
            if(pdist(e)>18) e->x+=4*e->face;                 /* steps into the cut */
            if(in_reach(e,26)) hurt_player(1,e->x,1);
        }
        break;
    case E_STRIKE: if(e->t>=140){ e->st=E_REC; e->t=0; } break;
    case E_REC:    if(e->t>=260){ e->st=E_WALK; e->t=0; e->sub=180; } break;
    case E_DODGE:
        e->x -= 3*e->face;
        if(e->t>=240){ e->st=E_WALK; e->t=0; e->sub=0; }
        break;
    case E_HURT:   if(e->t>=200){ e->st=E_WALK; e->t=0; e->sub=0; } break;
    }
}
static void skull_step(Obj *e) {
    const Obj *p=&ob[0];
    int32_t want, ph;
    if(e->st==E_HURT){                       /* stunned: hangs in place */
        if(e->t>=220){ e->st=E_WALK; e->t=0; }
        return;
    }
    face_player(e);
    ph = (gt/120)%12;
    e->y = 182 + (ph<6 ? ph : 12-ph) - 3;
    want = p->x - 70*e->face;
    if(want<10)want=10;
    if(want>310)want=310;
    if((gt/FRAME_MS)&1){ if(e->x<want)e->x++; else if(e->x>want)e->x--; }
    if(e->st==E_WIND){
        if(e->t>=450){
            Obj *f=spawn(O_FBALL,e->x+6*e->face,e->y);
            if(f){
                int32_t frames=pdist(e)/4;
                if(frames<1)frames=1;
                f->vx=4*e->face;
                f->vy=((GY-14)-e->y)/frames;
            }
            e->st=E_WALK; e->t=0; e->t2=2600+(int32_t)(rnd()%800u);
        }
        return;
    }
    e->t2-=FRAME_MS;
    if(e->t2<=0 && e->x>4 && e->x<316 && p->st!=P_DEAD){ e->st=E_WIND; e->t=0; }
}
static const int32_t bk_wind[3]={480,320,380}, bk_strike[3]={160,160,220},
                     bk_reach[3]={40,44,56}, bk_dmg[3]={2,1,2};
static void bk_step(Obj *e) {
    switch(e->st){
    case E_WALK:
        face_player(e);
        if(e->sub>0) e->sub-=FRAME_MS;
        if(pdist(e)>44){ if(tick4()) step_toward(e,2); }
        else if(e->sub<=0){ e->st=E_WIND; e->t=0; e->a=0; }
        break;
    case E_WIND:
        if(e->t>=bk_wind[e->a]){
            e->st=E_STRIKE; e->t=0;
            if(in_reach(e,bk_reach[e->a])) hurt_player(bk_dmg[e->a],e->x,1);
        }
        break;
    case E_STRIKE:
        if(e->t>=bk_strike[e->a]){ e->st=E_REC; e->t=0; }
        break;
    case E_REC:
        if(e->a<2){
            if(e->t>=220){ e->a++; e->st=E_WIND; e->t=0; face_player(e); }
        } else if(e->t>=1100){ e->st=E_WALK; e->t=0; e->a=0; e->sub=900; }
        break;
    }
}
static void spider_step(Obj *e) {
    const Obj *p=&ob[0];
    int32_t d=pdist(e);
    if(p->st==P_WEB && e->st!=E_RUSH && e->st!=E_HURT){ e->st=E_RUSH; e->t=0; }
    switch(e->st){
    case E_RUSH:
        if(p->st!=P_WEB){ e->st=E_REC; e->t=0; break; }
        face_player(e);
        if(d>16){ if(spider_go()) step_toward(e,3); }
        else if(e->t>=800){ e->t=0; hurt_player(1,e->x,0); }
        break;
    case E_WALK:
        face_player(e);
        if(!spider_go()) { /* rest a step */ }
        else if(d<60) e->x -= 2*e->face;
        else if(d>100) step_toward(e,2);
        if(e->x<8)e->x=8;
        if(e->x>312)e->x=312;
        e->t2-=FRAME_MS;
        if(e->t2<=0 && p->st!=P_DEAD){
            e->a = (rnd()%100u<55u) ? 0 : 1;     /* 0 web, 1 lunge */
            e->st=E_WIND; e->t=0;
        }
        break;
    case E_WIND:
        if(e->a==0 && e->t>=420){
            Obj *w=spawn(O_WEB,e->x+12*e->face,GY-12);
            if(w){ w->vx=5*e->face; w->life=1400; }
            e->st=E_REC; e->t=0;
        } else if(e->a==1 && e->t>=320){
            e->st=E_STRIKE; e->t=0; e->sub=0;
        }
        break;
    case E_STRIKE:
        if(spider_go()) step_toward(e,6);
        if(e->x<8)e->x=8;
        if(e->x>312)e->x=312;
        if(!e->sub && pdist(e)<=16){ e->sub=1; hurt_player(1,e->x,1); }
        if(e->t>=300){ e->st=E_REC; e->t=0; }
        break;
    case E_REC:
        if(e->t>=600){ e->st=E_WALK; e->t=0; e->t2=2400+(int32_t)(rnd()%1000u); }
        break;
    case E_HURT:
        if(e->t>=220){ e->st=E_WALK; e->t=0; if(e->t2<600) e->t2=600; }
        break;
    }
}
static void snake_step(Obj *e) {
    const Obj *p=&ob[0];
    int32_t d=pdist(e);
    switch(e->st){
    case E_WALK:
        face_player(e);
        e->b=0;
        if(e->sub>0) e->sub-=FRAME_MS;
        if(d<=22 && e->sub<=0){ e->st=E_WIND; e->t=0; e->a=0; }
        else if(d>=30 && d<=62 && e->sub<=0 && rnd()%100u<5u && p->st!=P_DEAD){
            e->st=E_WIND; e->t=0; e->a=1;
        } else if(d>20 && tick4()) step_toward(e,2);
        break;
    case E_WIND:
        if(e->a==1) e->b=-6;
        if(e->t>=(e->a ? 450 : 360)){
            e->st=E_STRIKE; e->t=0; e->hit_id=0;
            if(e->a==0 && in_reach(e,26)) hurt_player(1,e->x,1);
        }
        break;
    case E_STRIKE:
        if(e->a==0){ if(e->t>=150){ e->st=E_REC; e->t=0; } break; }
        e->b = e->t<140 ? e->t*52/140 : e->t<240 ? 52 : 52 - (e->t-240)*52/100;
        if(e->b<0) e->b=0;
        if(!e->hit_id){
            int32_t hx = e->x + (2+e->b)*e->face;
            int32_t hx0 = e->face>0 ? hx : hx-10;
            if(hits_player(hx0,GY-22,hx0+10,GY-14)){ e->hit_id=1; hurt_player(1,e->x,1); }
        }
        if(e->t>=340){ e->st=E_REC; e->t=0; e->b=0; }
        break;
    case E_REC:
        if(e->t>=(e->a ? 500 : 420)){ e->st=E_WALK; e->t=0; e->sub = e->a ? 900 : 600; }
        break;
    case E_HURT: e->b=0; if(e->t>=220){ e->st=E_WALK; e->t=0; } break;
    }
}
static void lich_summon(Obj *l) {
    for(int k=0;k<2;k++){
        int32_t x, r=(int32_t)(rnd()%100u), type;
        Obj *m;
        if(count_minions()>=3) return;
        x = l->x + ((rnd()&1) ? 50 : -50);
        if(x<16)x=16;
        if(x>304)x=304;
        type = r<40 ? O_SKEL : r<65 ? O_RSKEL : O_SKULL;
        m=init_enemy(type,x);
        if(m){ m->summoned=1; fx(3,x,GY-12,300); }
    }
}
static void lich_zone(int32_t x) {
    Obj *z;
    if(x<24)x=24;
    if(x>296)x=296;
    z=spawn(O_GZONE,x,GY);
    if(z) z->a=44;
}
static void lich_step(Obj *l) {
    const Obj *p=&ob[0];
    int32_t d=pdist(l);
    int hard = l->hp<=LICH_HP/2;
    int32_t cool = hard ? 450 : 800;
    /* standing right next to him builds up a fire burst */
    if(l->st!=L_RAGE_W && l->st!=L_RAGE){
        if(d<=46 && p->st!=P_DEAD) l->a+=FRAME_MS;
        else if(l->a>0) l->a-=FRAME_MS;
    }
    switch(l->st){
    case E_ENTER:
        l->face=-1;
        if(tick2()) l->x-=2;
        if(l->x<=250){ l->st=L_IDLE; l->t=0; }
        return;
    case L_IDLE:
        face_player(l);
        if(tick4()){
            if(d<70 && l->x-2*l->face>=40 && l->x-2*l->face<=280) l->x-=2*l->face;
            else if(d>150) l->x+=2*l->face;
        }
        if(p->st==P_DEAD) return;
        if(l->a>=1600){ l->a=0; l->st=L_RAGE_W; l->t=800; return; }   /* quick flare-up */
        l->t2-=FRAME_MS;
        if(l->t2>0) return;
        {
            int32_t r=(int32_t)(rnd()%100u);
            if(d<=64){
                if(r<50) l->st=L_SWIPE_W;
                else if(r<85) l->st=L_RAGE_W;
                else l->st=L_ZONE;
            } else {
                if(r<35) l->st=L_BALL_W;
                else if(r<60) l->st=L_ZONE;
                else if(r<72 && d<=110) l->st=L_RAGE_W;
                else if(count_minions()<3) l->st=L_SUMMON_W;
                else l->st=L_BALL_W;
            }
            l->t=0;
        }
        return;
    case L_SWIPE_W:
        if(l->t>=220){ l->st=L_SWIPE; l->t=0; if(in_reach(l,60)) hurt_player(2,l->x,1); }
        return;
    case L_SWIPE:  if(l->t>=150){ l->st=L_SWIPE_R; l->t=0; } return;
    case L_SWIPE_R:
        if(l->t>=150){ face_player(l); l->st=L_BASH_W; l->t=0; }
        return;
    case L_BASH_W:
        if(l->t>=500){
            l->st=L_BASH; l->t=0;
            fx(4, l->x+78*l->face, GY-4, 250);
            if(in_reach(l,80)) hurt_player(3,l->x,0);     /* overhead: breaks guard */
        }
        return;
    case L_BASH:   if(l->t>=200){ l->st=L_BASH_R; l->t=0; } return;
    case L_BASH_R: if(l->t>=650){ l->st=L_IDLE; l->t=0; l->t2=cool; } return;
    case L_ZONE:
        if(l->t>=400){
            lich_zone(p->x);
            if(hard) lich_zone(p->x + ((rnd()&1) ? 54 : -54));
            l->st=L_IDLE; l->t=0; l->t2=cool;
        }
        return;
    case L_BALL_W:
        if(l->t>=450){ l->st=L_BALL; l->t=0; l->sub=0; }
        return;
    case L_BALL:
        face_player(l);
        if(l->t >= l->sub*220){
            int32_t sx = l->x + 30*l->face, sy = GY-86;
            Obj *f=spawn(O_GBALL,sx,sy);
            if(f){
                int32_t frames=iabs(p->x-sx)/5;
                if(frames<1)frames=1;
                f->vx=5*l->face;
                f->vy=((GY-14)-sy)/frames;
                if(f->vy>4)f->vy=4;
                if(f->vy<1)f->vy=1;
            }
            if(++l->sub>=(hard ? 5 : 4)){ l->st=L_IDLE; l->t=0; l->t2=cool; }
        }
        return;
    case L_SUMMON_W:
        if(l->t>=1600){ lich_summon(l); l->st=L_IDLE; l->t=0; l->t2=cool+300; }
        return;
    case L_RAGE_W:
        l->b=1;                                     /* red eyes */
        if(l->t>=1500){
            Obj *f=spawn(O_EFIRE,l->x,GY);
            if(f){ f->a=136; f->life=2000; }
            l->st=L_RAGE; l->t=0; l->a=0;
        }
        return;
    case L_RAGE:
        if(l->t>=2000){ l->b=0; l->st=L_IDLE; l->t=0; l->t2=cool; }
        return;
    }
}
static void enemy_step(Obj *e) {
    e->t+=FRAME_MS;
    if(e->flash>0) e->flash-=FRAME_MS;
    if(e->burn>0) e->burn-=FRAME_MS;
    if(e->st==E_DIE){
        if(e->t>=(e->type==O_LICH ? 1500 : 420)) e->type=O_NONE;
        return;
    }
    if(e->stun>0){ e->stun-=FRAME_MS; e->t=0; return; }
    if(e->type!=O_LICH && (e->x<6 || e->x>314) && e->st==E_WALK){
        enemy_walk_in(e, e->type==O_SPIDER ? 2 : 1);
        return;
    }
    switch(e->type){
    case O_SKEL: case O_RSKEL: skel_step(e); break;
    case O_SKULL:   skull_step(e); break;
    case O_BKNIGHT: bk_step(e); break;
    case O_SPIDER:  spider_step(e); break;
    case O_SNAKE:   snake_step(e); break;
    case O_LICH:    lich_step(e); break;
    }
    if(e->type!=O_LICH){
        if(e->x<-20)e->x=-20;
        if(e->x>340)e->x=340;
    }
}

/* ---- Projectiles, hazards, pickups ----------------------------------- */
static void misc_step(Obj *o) {
    Obj *p=&ob[0];
    switch(o->type){
    case O_FBALL: case O_GBALL:
        o->x+=o->vx; o->y+=o->vy;
        if(hits_player(o->x-3,o->y-3,o->x+3,o->y+3)){
            hurt_player(1, o->x - o->vx*4, 1); o->type=O_NONE;
        } else if(o->x<-10 || o->x>330 || o->y>GY-2) o->type=O_NONE;
        break;
    case O_WEB:
        o->x+=o->vx; o->life-=FRAME_MS;
        if(hits_player(o->x-4,o->y-3,o->x+4,o->y+3) && pl_inv<=0 &&
           p->st!=P_DEAD && p->st!=P_WEB){
            p->st=P_WEB; p->t=0; p->b=0; o->type=O_NONE;
        } else if(o->life<=0 || o->x<-10 || o->x>330) o->type=O_NONE;
        break;
    case O_KNIFE:
        o->x+=o->vx;
        for(int i=1;i<NOBJ;i++){
            Obj *e=&ob[i];
            int32_t a,b,c,d;
            if(!is_enemy(e->type) || e->st==E_DIE || e->st==E_ENTER) continue;
            hurtbox(e,&a,&b,&c,&d);
            if(o->x>=a && o->x<c && o->y>=b && o->y<d){
                hurt_enemy(e,DMG_KNIFE,o->x-o->vx,1); o->type=O_NONE; break;
            }
        }
        if(o->type && (o->x<-10 || o->x>330)) o->type=O_NONE;
        break;
    case O_POTION:
        o->x+=o->vx; o->vy+=1; o->y+=o->vy;
        if(o->y>=GY-4){
            int32_t x=o->x;
            o->type=O_FIRE; o->y=GY; o->a=60; o->life=4000;
            if(x<30)x=30;
            if(x>290)x=290;
            o->x=x;
        }
        break;
    case O_FIRE:
        o->life-=FRAME_MS;
        for(int i=1;i<NOBJ;i++){
            Obj *e=&ob[i];
            if(!is_enemy(e->type) || e->st==E_DIE || e->type==O_SKULL || e->burn>0) continue;
            if(iabs(e->x-o->x) <= o->a/2 + 6){ e->burn=500; hurt_enemy(e,2,o->x,0); }
        }
        if(o->life<=0) o->type=O_NONE;
        break;
    case O_EFIRE:
        o->life-=FRAME_MS;
        if(iabs(p->x-o->x) <= o->a/2 + 6) hurt_player(2,o->x,0);
        if(o->life<=0) o->type=O_NONE;
        break;
    case O_GZONE:
        o->t+=FRAME_MS;
        if(o->t>=2500 && !o->b){
            o->b=1;
            if(iabs(p->x-o->x) <= o->a/2 + 6) hurt_player(2,o->x,0);
        }
        if(o->t>=2900) o->type=O_NONE;
        break;
    case O_PICKUP:
        o->life-=FRAME_MS;
        if(p->st!=P_DEAD && iabs(p->x-o->x)<=12 && o->a>=NITEMS){
            if(p->hp<10) p->hp++;
            fx(2, p->x, GY-30, 400);
            o->type=O_NONE;
        } else if(p->st!=P_DEAD && iabs(p->x-o->x)<=12){
            cnt[o->a]+=item_amount[o->a];
            if(cnt[o->a]>item_max[o->a]) cnt[o->a]=item_max[o->a];
            o->type=O_NONE;
        } else if(o->life<=0) o->type=O_NONE;
        break;
    case O_FX: case O_BANNER:
        o->life-=FRAME_MS;
        if(o->life<=0) o->type=O_NONE;
        break;
    }
}

/* ---- Drawing objects ------------------------------------------------- */
static void item_icon(int32_t kind,int32_t x,int32_t base) {
    switch(kind){
    case IT_KNIFE:
        prim(x-6,base-6,10,2,STEEL); prim(x+4,base-8,2,6,GOLD); break;
    case IT_CROSS:
        prim(x-2,base-12,4,12,GOLD); prim(x-6,base-10,12,4,GOLD); break;
    case IT_FIRE:
        prim(x-4,base-8,8,8,ORANGE); prim(x-2,base-12,4,4,BONE); prim(x-2,base-6,4,4,YELLOW); break;
    case NITEMS:                                   /* half-heart pickup */
        prim(x-4,base-8,2,2,RED); prim(x+2,base-8,2,2,RED);
        prim(x-4,base-6,8,2,RED); prim(x-2,base-4,4,2,RED);
        break;
    default:
        prim(x-2,base-12,4,2,BONE);
        prim(x-6,base-10,4,2,RED); prim(x+2,base-10,4,2,RED);
        prim(x-6,base-8,12,4,RED); prim(x-4,base-4,8,2,RED); prim(x-2,base-2,4,2,RED);
        break;
    }
}
static void draw_halberd(int32_t hx,int32_t top,int32_t s,int32_t pose) {
    if(pose<=1){
        int32_t y0 = top-8-(pose ? 10 : 0);
        prim(hx,y0,2,36,SHAFT);
        rel(hx,2,y0,6,8,s,STEEL);
        prim(hx,y0-4,2,4,STEEL);
        return;
    }
    if(pose==3){                                  /* sweep wind-up: held low, behind */
        hbar(hx,top+22,30,-s,2,SHAFT);
        rel(hx,-36,top+16,6,8,s,STEEL);
        return;
    }
    if(pose==5){                                  /* thrust wind-up: pulled back */
        hbar(hx-20*s,top+14,30,s,2,SHAFT);
        rel(hx,10,top+14,6,2,s,STEEL);
        return;
    }
    {
        int32_t y = pose==2 ? top+2 : pose==4 ? top+22 : top+14;
        int32_t len = pose==2 ? 30 : pose==4 ? 34 : 42;
        int32_t tip = hx + len*s;
        hbar(hx,y,len,s,2,SHAFT);
        if(pose==2) rel(tip,-6,y,6,10,s,STEEL);
        else if(pose==4) rel(tip,-6,y-8,6,10,s,STEEL);
        else { hbar(tip,y,8,s,2,STEEL); rel(tip,-6,y-4,4,10,s,STEEL); }
    }
}
static void draw_staff(int32_t hx,int32_t top,int32_t s,int32_t pose,int glow) {
    if(pose==2 || pose==4){                       /* swipe / bash: staff laid out forward */
        int32_t y = pose==2 ? top+36 : GY-10;
        int32_t len = pose==2 ? 30 : 48;
        int32_t tip = hx+len*s;
        hbar(hx,y,len,s,4,BONE);
        rel(tip,-2,y-4,10,10,s,BONE);
        rel(tip,2,y-2,2,2,s,DARK);
        rel(tip,6,y-2,2,2,s,DARK);
        rel(tip,6,y-10,4,6,s,BONE);
        prim(hx-2,top+34,6,6,BONE);
        return;
    }
    {
        int32_t up = (pose==3||pose==5) ? 16 : 0;
        int32_t y0 = top-10-up;
        prim(hx-2,y0,4,(GY-up)-y0,BONE);
        prim(hx-4,y0-8,8,8,BONE);                 /* skull ornament */
        prim(hx-2,y0-6,2,2,glow ? GREEN : DARK);
        prim(hx+2,y0-6,2,2,glow ? GREEN : DARK);
        rel(hx,-8,y0-16,4,10,s,BONE);             /* hooked bone crown */
        rel(hx,-8,y0-18,10,4,s,BONE);
        rel(hx,4,y0-14,4,6,s,BONE);
        prim(hx-2,top+34-up,6,6,BONE);            /* hand */
        if(glow && ((gt/140)&1)){
            prim(hx-10,y0-24,20,2,GREEN);
            prim(hx-12,y0-12,2,10,GREEN);
            prim(hx+10,y0-12,2,10,GREEN);
        }
    }
}
/* The knight's blade: five 2x2 blocks starting at (x+off, y), in one of
 * eight directions. Body poses are separate sprites, so the arm and torso
 * move with the swing. */
enum { B_FWD=0, B_UPF, B_DNF, B_UP, B_DN, B_BACK, B_UPB, B_DNB };
enum { POSE_WIND=0, POSE_EXT, POSE_HIGH, POSE_LOW };
static void blade(int32_t x,int32_t off,int32_t y,int32_t s,int32_t dir) {
    static const int8_t step[8][2] = {{2,0},{2,-2},{2,2},{0,-2},{0,2},{-2,0},{-2,-2},{-2,2}};
    for(int32_t i=0;i<5;i++)
        rel(x, off+step[dir][0]*i, y+step[dir][1]*i, 2, 2, s, i==4 ? WHITE : STEEL);
}
static void draw_fire(int32_t cx,int32_t w,int32_t ph) {
    int32_t x0=cx-w/2;
    prim(x0,GY-4,w,4,ORANGE);
    for(int32_t c=0;c*4<w;c++){
        int32_t h = 6 + (int32_t)(((uint32_t)(c*7 + ph*3 + c*c)) % 9u);
        prim(x0+c*4, GY-h, 4, h-6, (c+ph)&1 ? ORANGE : RED);
        prim(x0+c*4, GY-h-2, 4, 2, YELLOW);
    }
}
static void draw_obj(Obj *o) {
    int32_t s=o->face, flip = s<0, top;
    int32_t X = o->x & ~1, Y = o->y & ~1;         /* everything sits on the 2-pixel grid */
    uint16_t tint = (o->flash>0 && ((o->flash/60)&1)) ? WHITE : 0;
    if(is_enemy(o->type) && o->st==E_DIE && ((o->t/70)&1)) return;
    switch(o->type){
    case O_PLAYER: {
        const Spr *sp=&sp_kn_idle;
        if(o->st==P_DEAD && ((o->t/100)&1)) return;
        tint = (pl_inv>0 && o->st!=P_WEB && o->st!=P_DEAD && ((pl_inv/70)&1)) ? WHITE : 0;
        if(o->st==P_ATK){
            /* each swing: wind-up, three sweep positions, follow-through {pose, blade} */
            static const uint8_t swing[4][5][2] = {
              {{POSE_WIND,B_BACK},{POSE_EXT,B_UPF}, {POSE_EXT,B_FWD}, {POSE_EXT,B_DNF}, {POSE_EXT,B_DNF}},  /* flat slash */
              {{POSE_WIND,B_DNB}, {POSE_LOW,B_DNF}, {POSE_EXT,B_FWD}, {POSE_HIGH,B_UPF},{POSE_HIGH,B_UPF}}, /* rising     */
              {{POSE_WIND,B_UPB}, {POSE_HIGH,B_UPF},{POSE_EXT,B_FWD}, {POSE_LOW,B_DNF}, {POSE_LOW,B_DNF}},  /* falling    */
              {{POSE_WIND,B_UPB}, {POSE_HIGH,B_UP}, {POSE_HIGH,B_UPF},{POSE_LOW,B_DN},  {POSE_LOW,B_DN}}    /* overhead   */
            };
            static const int8_t hand[4][2] = {{-14,10},{12,10},{10,4},{8,16}};   /* blade start per pose */
            int32_t f = o->t<HIT_FROM ? 0 : o->t<105 ? 1 : o->t<150 ? 2 : o->t<HIT_TO ? 3 : 4;
            int32_t pose=swing[o->a&3][f][0], dir=swing[o->a&3][f][1];
            int32_t by=hand[pose][1];
            if(pose==POSE_WIND) by += dir==B_UPB ? -2 : dir==B_DNB ? 2 : 0;
            sp = pose==POSE_WIND ? &sp_kn_wind : pose==POSE_EXT ? &sp_kn_ext :
                 pose==POSE_HIGH ? &sp_kn_high : &sp_kn_low;
            top=GY-24;
            spr(sp,X-12,top,2,flip,tint,0);
            blade(X, hand[pose][0], top+by, s, dir);
            break;
        }
        if(o->st==P_BASH){
            top=GY-24;
            spr(&sp_kn_idle,X-12+4*s,top,2,flip,tint,0);      /* lunges shield-first */
            if(o->t<120) rel(X,16,top+6,2,12,s,WHITE);
            break;
        }
        if(o->st==P_THROW) sp=&sp_kn_throw;
        else if(o->st==P_WEB || o->st==P_HURT || o->st==P_DEAD) sp=&sp_kn_w2;
        else if(pl_moving) sp = ((walk_anim/140)&1) ? &sp_kn_w1 : &sp_kn_w2;
        top=GY-24;
        spr(sp,X-12,top,2,flip,tint,0);
        if(o->st==P_WEB){
            int32_t l=X-12;
            prim(l+2,top+6,20,2,WEBC); prim(l+4,top+12,16,2,WEBC);
            prim(l+2,top+18,20,2,WEBC); prim(l+6,top+2,2,20,WEBC);
            prim(l+16,top+4,2,18,WEBC);
        }
        break; }
    case O_SKEL: case O_RSKEL: {
        const Spr *sp = o->st==E_WIND ? &sp_sk_wind : o->st==E_STRIKE ? &sp_sk_strike :
                        o->st==E_DODGE ? &sp_sk_b : ((o->x>>2)&1) ? &sp_sk_a : &sp_sk_b;
        top=GY-24;
        if(o->type==O_RSKEL) g_bone=0xCD2F;        /* red-eyed: brown-beige bones */
        spr(sp,X-10,top,2,flip,tint,o->type==O_RSKEL ? 0xF800 : 0x2124);
        g_bone=BONE;
        if(o->type==O_RSKEL && !tint) rel(X,-2,top+2,8,2,s,0xF800);   /* glowing eye band */
        if(o->st==E_STRIKE) hbar(X+10*s,top+12,10,s,2,STEEL);
        break; }
    case O_SKULL:
        spr(&sp_skull,X-8,Y-8,2,flip,tint,o->st==E_WIND ? YELLOW : ORANGE);
        break;
    case O_BKNIGHT: {
        int32_t pose=0;
        if(o->st==E_WIND) pose = o->a==0 ? 1 : o->a==1 ? 3 : 5;
        else if(o->st==E_STRIKE) pose = o->a==0 ? 2 : o->a==1 ? 4 : 6;
        top=GY-28;
        spr(&sp_bk,X-12,top,2,flip,tint,0);
        draw_halberd(X+10*s,top,s,pose);
        break; }
    case O_SPIDER: {
        const Spr *sp = (o->st==E_WIND || (o->st==E_RUSH && pdist(o)<=16)) ? &sp_sp_rear :
                        ((o->x>>2)&1) ? &sp_sp_a : &sp_sp_b;
        spr(sp,X-14,GY-14,2,flip,tint,0);
        break; }
    case O_SNAKE: {
        int neck = o->a==1 && (o->st==E_WIND || o->st==E_STRIKE);
        const Spr *sp = neck ? &sp_sn_neck : o->st==E_WIND ? &sp_sn_wind :
                        o->st==E_STRIKE ? &sp_sn_strike : ((o->x>>2)&1) ? &sp_sn_a : &sp_sn_b;
        top=GY-26;
        spr(sp,X-10,top,2,flip,tint,0);
        if(o->st==E_STRIKE && o->a==0) hbar(X+10*s,top+12,14,s,2,STEEL);
        if(neck){
            int32_t L=o->b & ~1, hx=X+(2+L)*s;
            uint16_t c = tint ? tint : SNAKEG;
            if(L>0) hbar(X+2*s,top+8,L,s,4,c);
            rel(hx,0,top+4,10,8,s,c);
            rel(hx,6,top+6,2,2,s,YELLOW);
            rel(hx,10,top+8,4,2,s,RED);
        }
        break; }
    case O_LICH: {
        int32_t pose=0, hx;
        int glow = o->st==L_SUMMON_W || o->st==L_BALL_W || o->st==L_ZONE;
        if(o->st==L_SWIPE_W) pose=1;
        else if(o->st==L_SWIPE) pose=2;
        else if(o->st==L_BASH_W) pose=3;
        else if(o->st==L_BASH) pose=4;
        else if(o->st==L_SUMMON_W || o->st==L_BALL_W || o->st==L_BALL) pose=5;
        top=GY-72;
        spr(&sp_lich,X-32,top,4,flip,tint,o->b ? 0xF800 : GREEN);
        hx = pose==1 ? X-30*s : X+30*s;
        draw_staff(hx,top,s,pose,glow);
        break; }
    case O_FBALL: case O_GBALL:
        prim(X-4,Y-4,8,8,o->type==O_GBALL ? GREEN : ORANGE);
        prim(X-2,Y-2,4,4,o->type==O_GBALL ? WHITE : YELLOW);
        break;
    case O_WEB:
        prim(X-4,Y-4,8,2,WEBC); prim(X-4,Y+2,8,2,WEBC);
        prim(X-2,Y-4,2,8,WEBC);
        break;
    case O_KNIFE:
        hbar(X,Y,8,o->vx>0 ? -1 : 1,2,STEEL);
        prim(X - (o->vx>0 ? 10 : -8),Y-2,2,4,GOLD);
        break;
    case O_POTION:
        prim(X-4,Y-4,6,8,ORANGE); prim(X-2,Y-6,2,2,BONE);
        break;
    case O_FIRE: case O_EFIRE:
        draw_fire(X,o->a,(gt/140)%4);
        break;
    case O_GZONE: {
        int32_t x0=X-o->a/2;
        if(o->b){ prim(x0,GY-24,o->a,24,GREEN); break; }
        {
            int32_t period = o->t>1900 ? 120 : 250;
            uint16_t c = ((o->t/period)&1) ? GREEN : DKGREEN;
            prim(x0,GY-2,o->a,2,c);
            prim(x0,GY-8,2,6,c); prim(x0+o->a-2,GY-8,2,6,c);
        }
        break; }
    case O_PICKUP:
        if(o->life<2000 && ((o->life/140)&1)) break;
        item_icon(o->a,X,GY);
        break;
    case O_FX: {
        int32_t el;
        if(o->a==0){ prim(X-4,Y-2,8,2,YELLOW); prim(X-2,Y-4,2,8,YELLOW); break; }
        if(o->a==1){
            int32_t r;
            el = 350-o->life;
            r = (12 + el*46/350) & ~1;
            prim(X-r,Y-r/2,2*r,2,GOLD); prim(X-r,Y+r/2,2*r,2,GOLD);
            prim(X-r,Y-r/2,2,r,GOLD);   prim(X+r-2,Y-r/2,2,r,GOLD);
            break;
        }
        if(o->a==2){
            el = ((500-o->life)/40) & ~1;
            prim(X-4,Y-el-2,2,2,RED); prim(X+2,Y-el-2,2,2,RED);
            prim(X-4,Y-el,8,2,RED); prim(X-2,Y-el+2,4,2,RED);
            break;
        }
        if(o->a==3){ prim(X-8,Y-10,16,2,GREEN); prim(X-8,Y+8,16,2,GREEN);
                     prim(X-2,Y-10,2,20,GREEN); break; }
        prim(X-10,Y-2,20,4,GREY); prim(X-6,Y-6,4,4,GREY); prim(X+4,Y-8,4,4,GREY);
        break; }
    case O_BANNER: {
        /* the banner is drawn straight to the screen; here it only reserves its cells */
        int32_t n=slen(banner), w=(n*7+16+1)&~1, x0=(160-w/2)&~1;
        cells(x0,98,w,26,C_BANNER);
        break; }
    }
    if(is_enemy(o->type) && o->stun>0 && o->st!=E_DIE){      /* stars over a stunned enemy */
        int32_t a,b,c,d, ph=(gt/140)&1;
        hurtbox(o,&a,&b,&c,&d);
        b&=~1;
        prim(X-8+ph*4,b-6,2,2,YELLOW);
        prim(X+4-ph*4,b-8,2,2,YELLOW);
        prim(X-2,b-10+ph*2,2,2,WHITE);
    }
}
static int zof(int32_t t) {
    switch(t){
    case O_FIRE: case O_EFIRE: case O_GZONE: return 0;
    case O_LICH: return 1;
    case O_PICKUP: return 2;
    case O_PLAYER: return 4;
    case O_FBALL: case O_GBALL: case O_WEB: case O_KNIFE: case O_POTION: return 5;
    case O_FX: return 6;
    case O_BANNER: return 7;
    default: return 3;
    }
}

/* Key presses are collected here. poll_keys() is also called from inside the
 * renderer, so a quick tap made while a busy frame is being drawn still counts. */
static int32_t pend_atk, pend_item, pend_swap, pend_quit;
static void poll_keys(void) {
    last_poll=badge_millis();
    for(int n=0;n<8;n++){
        int32_t k=badge_key_pressed();
        if(k<0)break;
        if(k==keys[K_BACK]) pend_quit=1;
        if(k==keys[K_ATK]) pend_atk=1;
        if(k==keys[K_ITEM]) pend_item=1;
        if(k==keys[K_DOWN]) pend_swap=1;
    }
}

/* ---- Compose the arena, then send only the cells that changed -------- */
static void compose_bg(void) {
    prim(0,AY0,W,GY-AY0,C_WALL);
    for(int32_t i=0;i<4;i++){
        int32_t px=10+i*78;
        prim(px,50,14,GY-50,C_PIL);
        prim(px-2,46,18,6,C_CAP);
        prim(px+2,64,8,6,ORANGE);
        prim(px+4,60,4,4,YELLOW);
    }
    prim(0,GY,W,4,C_FLOOR);
    prim(0,GY+4,W,AY1-GY-4,C_GAP);
    for(int32_t tx=0; tx<W; tx+=20) prim(tx,GY+4,18,AY1-GY-4,C_TILE);
}
static void draw_banner(void) {
    for(int i=1;i<NOBJ;i++) if(ob[i].type==O_BANNER){
        int32_t n=slen(banner), w=(n*7+16+1)&~1, x0=(160-w/2)&~1;
        uint16_t c = ob[i].a ? RED : GOLD;
        rect(x0,98,w,26,BLACK);
        rect(x0,98,w,2,c); rect(x0,122,w,2,c);
        text(x0+8,104,banner,ob[i].a ? RED : YELLOW);
        return;
    }
}
static void render(int full) {
    if(full){
        for(int32_t j=0;j<CH;j++) for(int32_t i=0;i<CW;i++) prv[j][i]=0;
        banner_dirty=1;
    }
    compose_bg();
    for(int z=0;z<8;z++)
        for(int i=0;i<NOBJ;i++)
            if(ob[i].type!=O_NONE && zof(ob[i].type)==z) draw_obj(&ob[i]);
    for(int32_t y=0;y<CH;y++){
        for(int32_t x=0;x<CW;x++){
            uint8_t c=cur[y][x];
            int32_t x1, y1;
            if(c==prv[y][x]) continue;
            if(c==C_BANNER){ prv[y][x]=c; banner_dirty=1; continue; }
            x1=x+1;
            while(x1<CW && cur[y][x1]==c && prv[y][x1]!=c) x1++;
            y1=y+1;
            while(y1<CH){
                int32_t k=x;
                while(k<x1 && cur[y1][k]==c && prv[y1][k]!=c) k++;
                if(k<x1) break;
                y1++;
            }
            for(int32_t j=y;j<y1;j++) for(int32_t k=x;k<x1;k++) prv[j][k]=c;
            rect(x*2, AY0+y*2, (x1-x)*2, (y1-y)*2, palette[c-1]);
        }
    }
    if(banner_dirty){ banner_dirty=0; draw_banner(); }
}

/* ---- HUD ------------------------------------------------------------- */
static int32_t h_hp=-1, h_total=-1, h_boss=-1, h_sel=-2, h_cnt=-1, h_kills[NKINDS];
static uint32_t h_score=0xffffffffu;
static int32_t lich_hp(void) {
    for(int i=1;i<NOBJ;i++) if(ob[i].type==O_LICH) return ob[i].st==E_DIE ? 0 : ob[i].hp;
    return -1;
}
static void draw_heart(int32_t x,int32_t y,int32_t fill) {
    uint16_t l = fill>=1 ? RED : 0x3145, r = fill>=2 ? RED : 0x3145;
    rect(x,y,4,4,l); rect(x+6,y,4,4,r);
    rect(x,y+2,5,4,l); rect(x+5,y+2,5,4,r);
    rect(x+2,y+6,3,2,l); rect(x+5,y+6,3,2,r);
    rect(x+4,y+8,2,1,l);
}
static void draw_hud(int full) {
    static const char * const kind_tag[NKINDS] = {"SK","RE","SP","SN","BK"};
    const Obj *p=&ob[0];
    int32_t lhp=lich_hp();
    if(full){
        rect(0,0,W,AY0,BLACK); rect(0,AY1,W,H-AY1,BLACK);
        text(6,3,"HP",WHITE);
        text(104,3,"KILLS",CYAN);
        text(200,3,"SCORE",WHITE);
        rect(4,32,W-8,1,0x29AA);
        text(194,222,"B USE  DOWN SWAP",GREY);
        h_hp=-1; h_total=-1; h_boss=-1; h_sel=-2; h_cnt=-1; h_score=0xffffffffu;
        for(int i=0;i<NKINDS;i++) h_kills[i]=-1;
        if(lhp<0) for(int i=0;i<NKINDS;i++) text(6+i*62,17,kind_tag[i],GREY);
    }
    if(h_hp!=p->hp){
        for(int32_t i=0;i<5;i++){
            int32_t f = p->hp - i*2;
            draw_heart(28+i*14,5,f<0?0:f>2?2:f);
        }
        h_hp=p->hp;
    }
    if(h_total!=total_kills){
        rect(144,2,52,14,BLACK);
        print_num(144,3,(uint32_t)total_kills,total_kills>=KILL_GOAL ? GREEN : CYAN);
        text(total_kills>=10 ? 160 : 153,3,"/50",GREY);
        h_total=total_kills;
    }
    if(h_score!=score){ rect(242,2,76,14,BLACK); print_num(242,3,score,YELLOW); h_score=score; }
    if(lhp>=0){
        if(h_boss!=lhp){
            int32_t bw = lhp*200/LICH_HP;
            if(h_boss<0){ rect(0,17,W,14,BLACK); text(6,17,"THE LICH",GREEN); }
            rect(76,20,bw,7,RED); rect(76+bw,20,200-bw,7,0x3145);
            h_boss=lhp;
        }
    } else {
        if(h_boss>=0){
            rect(0,17,W,14,BLACK);
            for(int i=0;i<NKINDS;i++){ text(6+i*62,17,kind_tag[i],GREY); h_kills[i]=-1; }
            h_boss=-1;
        }
        for(int i=0;i<NKINDS;i++) if(h_kills[i]!=kills[i]){
            int32_t shown = kills[i]>99 ? 99 : kills[i];
            rect(24+i*62,17,36,14,BLACK);
            print_num(24+i*62,17,(uint32_t)shown,kills[i]>=EACH_GOAL ? GREEN : WHITE);
            text(24+i*62+(shown>=10 ? 14 : 7),17,"/5",GREY);
            h_kills[i]=kills[i];
        }
    }
    if(h_sel!=sel || (sel>=0 && h_cnt!=cnt[sel])){
        rect(0,216,190,22,BLACK);
        if(sel<0){
            rect(8,222,4,10,0x2B7A); rect(12,220,6,14,0x2B7A); rect(18,222,2,10,0x2B7A);
            text(28,222,"SHIELD",WHITE);
            text(84,222,"B: BASH",GREY);
        }
        else {
            g_hud=1; item_icon(sel,14,234); g_hud=0;
            text(28,222,item_names[sel],WHITE);
            text(120,222,"X",GREY);
            print_num(130,222,(uint32_t)cnt[sel],WHITE);
        }
        h_sel=sel; h_cnt = sel>=0 ? cnt[sel] : -1;
    }
}

/* ---- Waves and the round loop ---------------------------------------- */
enum { G_FIGHT=0, G_WIN, G_LOST };
static void show_banner(const char *a,uint32_t n,int red,int32_t life) {
    int32_t i=0, j=0;
    Obj *o;
    while(a[i] && i<14){ banner[i]=a[i]; i++; }
    if(n){
        char b[8]; int32_t k=7;
        b[7]='\0';
        do { b[--k]=(char)('0'+n%10u); n/=10u; } while(n && k>0);
        banner[i++]=' ';
        while(b[k+j] && i<22){ banner[i++]=b[k+j]; j++; }
    }
    banner[i]='\0';
    for(int k=1;k<NOBJ;k++) if(ob[k].type==O_BANNER) ob[k].type=O_NONE;
    o=spawn(O_BANNER,160,110);
    if(o){ o->life=life; o->a=red; }
    /* forget the old banner on screen so a different-sized one redraws cleanly */
    for(int32_t j=0;j<CH;j++) for(int32_t i2=0;i2<CW;i2++) if(prv[j][i2]==C_BANNER) prv[j][i2]=0;
    banner_dirty=1;
}
/* Which enemy comes next: mostly random, easier kinds first, and a kind you
 * have already beaten many of becomes less likely. Returns 0 for "none now". */
static int32_t pick_enemy(void) {
    static const int32_t type_of[NKINDS] = {O_SKEL,O_RSKEL,O_SPIDER,O_SNAKE,O_BKNIGHT};
    static const int32_t from[NKINDS]    = {0, 4, 10, 18, 28};   /* total kills needed */
    int32_t w[NKINDS], sum=0, need=0, alive=0, bk=0, sn=0, sp=0, cap, lacking_only, r;
    for(int i=1;i<NOBJ;i++) if(is_enemy(ob[i].type) && ob[i].st!=E_DIE){
        alive++;
        if(ob[i].type==O_BKNIGHT) bk++;
        if(ob[i].type==O_SNAKE) sn++;
        if(ob[i].type==O_SPIDER) sp++;
    }
    cap = total_kills<6 ? 2 : total_kills<16 ? 3 : total_kills<30 ? 4 : 5;
    if(alive>=cap) return 0;
    for(int i=0;i<NKINDS;i++) if(kills[i]<EACH_GOAL) need+=EACH_GOAL-kills[i];
    lacking_only = need >= KILL_GOAL-total_kills;
    for(int i=0;i<NKINDS;i++){
        int32_t others = alive - (i==4 ? bk : i==3 ? sn : 0);
        w[i]=0;
        if(lacking_only ? kills[i]>=EACH_GOAL : total_kills<from[i]) continue;
        w[i] = 40/(4+kills[i]);
        if(w[i]<1) w[i]=1;
        if(kills[i]<EACH_GOAL && total_kills>=30) w[i]*=3;
        if(i==4 && (bk>=2 || others>2)) w[i]=0;        /* black knights: 2, plus 2 others */
        if(i!=4 && bk>0 && alive-bk>=2) w[i]=0;
        if(i==3 && (sn>=3 || others>3)) w[i]=0;        /* snake men: 3, plus 3 others */
        if(i!=3 && sn>0 && alive-sn>=3) w[i]=0;
        if(i==2 && sp>=1) w[i]=0;                      /* only ever one spider */
        sum+=w[i];
    }
    if(sum<=0) return 0;
    r=(int32_t)(rnd()%(uint32_t)sum);
    for(int i=0;i<NKINDS;i++){ if(r<w[i]) return type_of[i]; r-=w[i]; }
    return 0;
}
static int goals_met(void) {
    if(total_kills<KILL_GOAL) return 0;
    for(int i=0;i<NKINDS;i++) if(kills[i]<EACH_GOAL) return 0;
    return 1;
}
static void reset_round(void) {
    for(int i=0;i<NOBJ;i++) obj_clear(&ob[i]);
    ob[0].type=O_PLAYER; ob[0].x=160; ob[0].face=1; ob[0].hp=10;
    for(int i=0;i<NITEMS;i++) cnt[i]=0;
    for(int i=0;i<NKINDS;i++) kills[i]=0;
    total_kills=0; lich_on=0;
    sel=-1; score=0; pl_inv=0; gt=0; thrust_id=0; walk_anim=0;
    buf_atk=0; buf_item=0; combo=0; combo_t=0;
    seed = badge_millis() | 1u;
    spawn_cd=1800; gstate=G_FIGHT;
    show_banner("SLAY 50 FOES",0,0,1800);
}
static void world_step(int left,int right,int atk,int item,int swap) {
    gt+=FRAME_MS;
    player_step(left,right,atk,item,swap);
    for(int i=1;i<NOBJ;i++){
        Obj *o=&ob[i];
        if(o->type==O_NONE) continue;
        if(is_enemy(o->type)) enemy_step(o); else misc_step(o);
    }
    if(gstate==G_FIGHT){
        if(ob[0].st==P_DEAD){
            gstate=G_LOST; gtimer=3200;
            show_banner("YOU DIED",0,1,3200);
        } else if(lich_on && lich_hp()==0){
            gstate=G_WIN; gtimer=3500;
            score += (uint32_t)ob[0].hp*200u;
            show_banner("THE LICH FALLS",0,0,3500);
        } else if(!lich_on){
            spawn_cd-=FRAME_MS;
            if(goals_met()){
                int32_t alive=0;
                for(int i=1;i<NOBJ;i++) if(is_enemy(ob[i].type)) alive++;
                if(alive==0 && spawn_cd<=0){
                    Obj *o;
                    lich_on=1;
                    init_enemy(O_LICH,344);
                    show_banner("THE LICH",0,1,1800);
                    o=spawn(O_PICKUP,ob[0].x<160 ? 200 : 120,GY);   /* a gift before the final fight */
                    if(o){ o->a=IT_HEAL; o->life=20000; }
                }
            } else if(spawn_cd<=0){
                int32_t type=pick_enemy();
                if(type){
                    init_enemy(type, spawn_side ? -12 : 332);
                    spawn_side=!spawn_side;
                    spawn_cd = 1500 - total_kills*14;
                } else spawn_cd=300;
            }
        }
    } else {
        gtimer-=FRAME_MS;
    }
}

/* ---- Menus (shared with Tengu Invaders) ------------------------------ */
static void flush_keys(void) {
    for(int n=0;n<32;n++) if(badge_key_pressed()<0) break;
}
static int32_t wait_key(void) {
    for(;;){
        int32_t k=badge_key_pressed();
        if(k>=0) return k;
        badge_delay_ms(15);
    }
}
static void wait_release(int32_t key) {
    uint32_t start=badge_millis();
    while(badge_key_held(key) && badge_millis()-start<1500u) badge_delay_ms(25);
}
static void setup_buttons(void) {
    int32_t fresh[NKEYS];
    for(int stage=0;stage<NKEYS;stage++){
        int32_t key=-1;
        badge_clear(BLACK);
        text(20,20,"BUTTON SETUP",CYAN);
        text(20,60,"PRESS THE BUTTON FOR:",WHITE);
        text(20,82,key_names[stage],YELLOW);
        text(20,120,"ONE PRESS PER STEP",WHITE);
        text(20,200,fw_ok() ? "SAVED TO THE SD CARD" : "NOT SAVED (OTHER FIRMWARE)",GREEN);
        while(key<0){
            int valid=1;
            key=wait_key();
            for(int j=0;j<stage;j++)if(fresh[j]==key)valid=0;
            if(key<16) valid=0;
            if(!valid) key=-1;
        }
        fresh[stage]=key;
        wait_release(key);
        badge_delay_ms(120);
    }
    for(int i=0;i<NKEYS;i++) keys[i]=fresh[i];
    flush_keys();
    store_save();
}
static void draw_menu_cursor(int32_t sel_) {
    rect(100,66,14,90,BLACK);
    text(104,70+sel_*22,">",YELLOW);
}
/* Returns 0 play, 1 high scores, 2 buttons, 3 exit. */
static int32_t main_menu(void) {
    static const char * const items[4] = {"PLAY","HIGH SCORES","BUTTONS","EXIT GAME"};
    int32_t s=0, stray=0;
    badge_clear(BLACK);
    text(118,20,"TENGU KNIGHT",GREEN);
    rect(60,40,200,1,DARK);
    for(int i=0;i<4;i++) text(120,70+i*22,items[i],WHITE);
    text(27,176,"UP / DOWN TO MOVE, A TO CHOOSE",WHITE);
    text(27,196,"STUCK? TAP ANY OTHER KEY 3 TIMES",GREY);
    draw_menu_cursor(s);
    flush_keys();
    for(;;){
        int32_t k=wait_key();
        if(k==keys[K_UP] || k==keys[K_LEFT]){ s=(s+3)%4; stray=0; }
        else if(k==keys[K_DOWN] || k==keys[K_RIGHT]){ s=(s+1)%4; stray=0; }
        else if(k==keys[K_ATK]){ wait_release(k); return s; }
        else if(k==keys[K_BACK] || k==keys[K_ITEM]) stray=0;
        else if(++stray>=3) return 2;   /* way out if the saved buttons are wrong */
        draw_menu_cursor(s);
    }
}
static void show_scores(int32_t highlight) {
    badge_clear(BLACK);
    text(121,20,"HIGH SCORES",GREEN);
    rect(60,40,200,1,DARK);
    for(int i=0;i<NSCORES;i++){
        uint16_t col = i==highlight ? YELLOW : WHITE;
        int32_t y=62+i*24;
        print_num(70,y,(uint32_t)(i+1),col);
        if(hs_score[i]){
            text(96,y,hs_name[i],col);
            print_num(196,y,hs_score[i],col);
        } else text(96,y,"---",GREY);
    }
    text(72,206,"PRESS ANY KEY FOR MENU",WHITE);
    badge_delay_ms(300);
    flush_keys();
    wait_release(wait_key());
}
static char cycle_char(char ch, int32_t dir) {
    static const char set[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -.";
    int32_t n=(int32_t)sizeof set - 1, at=0;
    for(int i=0;i<n;i++) if(set[i]==ch) at=i;
    return set[(at+dir+n)%n];
}
static void enter_name(char *out) {
    static const char fallback[] = "KNIGHT";
    int32_t n=0;
    out[0]='\0';
    badge_clear(BLACK);
    text(107,20,"NEW HIGH SCORE!",GREEN);
    text(107,44,"SCORE",WHITE);
    print_num(152,44,score,YELLOW);
    text(104,80,"TYPE YOUR NAME",WHITE);
    rect(118,122,84,1,DARK);
    text(41,160,"KEYBOARD: TYPE      DEL: ERASE",WHITE);
    text(41,178,"A OR ENTER: SAVE",WHITE);
    text(41,196,"NO KEYBOARD? UP/DOWN/RIGHT/LEFT",GREY);
    flush_keys();
    for(;;){
        int32_t k;
        rect(118,104,90,16,BLACK);
        text(132,105,out,YELLOW);
        if(n<NAME_LEN) rect(132+7*n,117,6,2,WHITE);
        k=wait_key();
        if(k==keys[K_ATK] || k==KEY_ENTER) { wait_release(k); break; }
        else if(k==keys[K_LEFT] || k==keys[K_BACK] || k==KEY_DEL){ if(n) out[--n]='\0'; }
        else if(k==keys[K_UP] || k==keys[K_DOWN]){
            if(!n){ out[0]='A'; out[1]='\0'; n=1; }
            else out[n-1]=cycle_char(out[n-1], k==keys[K_UP] ? 1 : -1);
        }
        else if(k==keys[K_RIGHT]){ if(n<NAME_LEN){ out[n++]='A'; out[n]='\0'; } }
        else {
            char ch=key_char(k);
            if(ch && n<NAME_LEN){ out[n++]=ch; out[n]='\0'; }
        }
    }
    while(n && out[n-1]==' ') out[--n]='\0';
    if(!n) for(int i=0;i<(int)sizeof fallback;i++) out[i]=fallback[i];
}
static int32_t record_score(void) {
    char name[NAME_LEN+1];
    int32_t at=-1;
    if(!score || score<=hs_score[NSCORES-1]) return -1;
    enter_name(name);
    for(int i=0;i<NSCORES;i++) if(score>hs_score[i]){ at=i; break; }
    if(at<0) return -1;
    for(int i=NSCORES-1;i>at;i--){
        hs_score[i]=hs_score[i-1];
        for(int b=0;b<=NAME_LEN;b++) hs_name[i][b]=hs_name[i-1][b];
    }
    hs_score[at]=score;
    for(int b=0;b<=NAME_LEN;b++) hs_name[at][b]=name[b];
    store_save();
    return at;
}

/* One round. Returns when the player goes back to the menu. */
static void play(void) {
    uint32_t last;
    reset_round();
    badge_clear(BLACK);
    draw_hud(1);
    render(1);
    flush_keys();
    pend_atk=0; pend_item=0; pend_swap=0; pend_quit=0;
    in_play=1;
    last=badge_millis();
    for(;;){
        uint32_t now=badge_millis();
        poll_keys();
        if(pend_quit){ in_play=0; wait_release(keys[K_BACK]); return; }
        if((uint32_t)(now-last) >= FRAME_MS){
            int atk=pend_atk, item=pend_item, swap=pend_swap;
            pend_atk=0; pend_item=0; pend_swap=0;
            last=now;
            world_step(badge_key_held(keys[K_LEFT])!=0, badge_key_held(keys[K_RIGHT])!=0,
                       atk,item,swap);
            render(0);
            draw_hud(0);
            if((gstate==G_WIN || gstate==G_LOST) && gtimer<=0){
                in_play=0;
                show_scores(record_score());
                return;
            }
        }
        badge_delay_ms(2);
    }
}
int app_main(int argc,char **argv){
    (void)argc;(void)argv;
    if(badge_screen_width()<320 || badge_screen_height()<240)return 1;
    store_load();
    flush_keys();
    if(!keys_loaded) setup_buttons();
    for(;;){
        int32_t choice=main_menu();
        if(choice==0) play();
        else if(choice==1) show_scores(-1);
        else if(choice==2) setup_buttons();
        else break;
    }
    badge_clear(BLACK);
    flush_keys();
    return 0;
}
