#include "../buddy.h"
#include "../buddy_common.h"
#include <M5StickCPlus.h>
#include <string.h>

extern TFT_eSprite spr;

namespace chonk {

// ═══════════════════════════════════════════════════════════════════
//  CHONK —— 一只圆滚滚的猪
//  标志特征：耷拉圆耳 + 大猪鼻孔 + 四条小腿
//  睡觉时：趴着、头低、耳朵耷拉、肚子呼吸起伏
// ═══════════════════════════════════════════════════════════════════

// ─── SLEEP ─── 猪趴着睡：头低、耳朵耷拉、眼睛紧闭、肚子呼吸、偶尔打呼
static void doSleep(uint32_t t) {
  static const char* const SIDE[5]    = { "            ", "            ", "  (\\____/)  ", " ( =    = ) ", " (__OO___)~ " };
  static const char* const INHALE[5]  = { "            ", "            ", "  (\\____/)  ", " ( =    = ) ", " (~_OO___)~ " };
  static const char* const EXHALE[5]  = { "            ", "            ", "  (\\____/)  ", " ( =    = ) ", "  (__OO__)~ " };
  static const char* const SNORE[5]   = { "            ", "       ~~   ", "  (\\____/)  ", " ( =    = ) ", " (__OOO__)~ " };
  static const char* const DROOL[5]   = { "            ", "            ", "  (\\____/)  ", " ( =    . ) ", " (__OO.____)" };
  static const char* const TWITCH[5]  = { "            ", "            ", "  (\\____/)  ", " ( =    = ) ", "  (_/\\__/\\_)" };
  static const char* const DREAM[5]   = { "            ", "     o      ", "  (\\____/)  ", " ( u    u ) ", " (__OO___)~ " };

  const char* const* P[7] = { SIDE, INHALE, EXHALE, SNORE, DROOL, TWITCH, DREAM };
  static const uint8_t SEQ[] = {
    0,0,1,2,1,2,0,0,        // 慢呼吸循环
    3,3,                    // 打呼噜（鼻孔放大 + 冒气）
    1,2,1,2,                // 继续呼吸
    4,4,                    // 流口水
    0,0,                    // 回到基础
    5,5,                    // 抽搐（做梦踢腿）
    0,1,2,0,
    6,6,                    // 做梦（眼睛变 u u）
    0,1,2,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);

  // 吸气时整体上浮 1 像素（呼吸感）
  int bob = (SEQ[beat] == 1) ? -1 : 0;
  buddyPrintSprite(P[SEQ[beat]], 5, bob, 0xFD20);

  // Z 粒子：三条交错从头顶飘出去
  int p1 = (t)     % 12;
  int p2 = (t + 5) % 12;
  int p3 = (t + 9) % 12;
  buddySetColor(BUDDY_DIM);
  buddySetCursor(BUDDY_X_CENTER + 18 + p1, BUDDY_Y_OVERLAY + 20 - p1 * 2);
  buddyPrint("z");
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER + 24 + p2, BUDDY_Y_OVERLAY + 16 - p2);
  buddyPrint("Z");
  buddySetColor(BUDDY_DIM);
  buddySetCursor(BUDDY_X_CENTER + 30 + p3 / 2, BUDDY_Y_OVERLAY + 12 - p3 / 2);
  buddyPrint("z");
}

// ─── IDLE ─── 站着晃悠，偶尔嗅东西
static void doIdle(uint32_t t) {
  static const char* const REST[5]     = { "            ", "  (\\____/)  ", " ( o    o ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const LOOK_L[5]   = { "            ", "  (\\____/)  ", " (o     o ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const LOOK_R[5]   = { "            ", "  (\\____/)  ", " ( o     o) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const LOOK_U[5]   = { "            ", "  (\\____/)  ", " ( '    ' ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const BLINK[5]    = { "            ", "  (\\____/)  ", " ( -    - ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const EAR_L[5]    = { "            ", "  (\\___/_   ", " ( o    o ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const EAR_R[5]    = { "            ", "   _(\\___/) ", " ( o    o ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const SNIFF_A[5]  = { "            ", "  (\\____/)  ", " ( o    o ) ", " (  O  O  ) ", "  \\_/  \\_/w " };
  static const char* const SNIFF_B[5]  = { "            ", "  (\\____/)  ", " ( o    o ) ", " (  O  O  ) ", "  \\_/  \\_/ww" };
  static const char* const WIGGLE[5]   = { "            ", "  (\\____/)  ", " ( o    o ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const HAPPY[5]    = { "            ", "  (\\____/)  ", " ( ^    ^ ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const TAIL[5]     = { "            ", "  (\\____/)  ", " ( o    o ) ", " (  O  O  ) ", " @\\_/  \\_/  " };

  const char* const* P[12] = { REST, LOOK_L, LOOK_R, LOOK_U, BLINK, EAR_L, EAR_R, SNIFF_A, SNIFF_B, WIGGLE, HAPPY, TAIL };
  static const uint8_t SEQ[] = {
    0,0,0,4,0,1,0,2,0,
    5,0,6,0,
    7,8,7,8,
    0,0,3,3,0,4,
    10,10,0,0,
    11,11,0,4,
    9,9,9,0,
    0,5,6,0
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFD20);
}

// ─── BUSY ─── 埋头苦干，鼻子喷气
static void doBusy(uint32_t t) {
  static const char* const PONDER[5]   = { "      ?     ", "  (\\____/)  ", " ( ^    ^ ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const CALC_A[5]   = { "            ", "  (\\____/)  ", " ( v    v ) ", " (  O  O  ) ", " /\\_/  \\_/\\ " };
  static const char* const CALC_B[5]   = { "            ", "  (\\____/)  ", " ( v    v ) ", " (  O  O  ) ", " \\/\\_/  \\_/ " };
  static const char* const SCRIBE[5]   = { "    ___     ", "  (\\___/)/  ", " ( o    o-- ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const HUFF_A[5]   = { "    ~~      ", "  (\\____/)  ", " ( o    o ) ", " (  O  O  )~", "  \\_/  \\_/  " };
  static const char* const HUFF_B[5]   = { "    ~~      ", "  (\\____/)  ", " ( o    o ) ", " (  O  O  )~", " /\\_/  \\_/\\ " };
  static const char* const AHA[5]      = { "      *     ", "  (\\____/)  ", " ( O    O ) ", " (  O  O  ) ", " /\\_/  \\_/\\ " };
  static const char* const SIGH[5]     = { "    ~~~     ", "  (\\____/)  ", " ( -    - ) ", " (  O  O  ) ", "  \\_/  \\_/  " };

  const char* const* P[8] = { PONDER, CALC_A, CALC_B, SCRIBE, HUFF_A, HUFF_B, AHA, SIGH };
  static const uint8_t SEQ[] = {
    1,2,1,2,1,2, 0,0, 1,2,1,2, 3,3,3, 4,5,4,5, 6,6, 1,2,1,2, 7,7
  };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFD20);

  static const char* const COGS[] = { "+  ", "x  ", "*  ", "x  " };
  buddySetColor(BUDDY_CYAN);
  buddySetCursor(BUDDY_X_CENTER + 24, BUDDY_Y_OVERLAY + 12);
  buddyPrint(COGS[t % 4]);
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER + 30, BUDDY_Y_OVERLAY + 18);
  buddyPrint(COGS[(t + 2) % 4]);

  if (SEQ[beat] == 4 || SEQ[beat] == 5) {
    int puff = (t * 2) % 8;
    buddySetColor(BUDDY_DIM);
    buddySetCursor(BUDDY_X_CENTER + 20 + puff, BUDDY_Y_OVERLAY + 16 - puff);
    buddyPrint("~");
  }
}

// ─── ATTENTION ─── 思考中（手托下巴 + 头顶问号 + 三点递增）
static void doAttention(uint32_t t) {
  static const char* const THINK_A[5]   = { "            ", "  (\\____/)  ", " ( o    - ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const THINK_B[5]   = { "            ", "  (\\____/)  ", " ( -    o ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const LOOK_UP[5]   = { "            ", "  (\\____/)  ", " ( '    ' ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const SCRATCH[5]   = { "            ", "  (\\____/)  ", " ( o    o ) ", " (  O  O  )/", "  \\_/  \\_/  " };
  static const char* const AHA[5]       = { "            ", "  (\\____/)  ", " ( O    O ) ", " (  O  O  ) ", " /\\_/  \\_/\\ " };

  const char* const* P[5] = { THINK_A, THINK_B, LOOK_UP, SCRATCH, AHA };
  static const uint8_t SEQ[] = {
    0,0,1,0,1,
    2,2,
    3,3,
    0,1,0,1,
    4,4,
    0,0
  };
  uint8_t beat = (t / 4) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFD20);

  if ((t / 3) & 1) {
    buddySetColor(BUDDY_CYAN);
    buddySetCursor(BUDDY_X_CENTER + 22, BUDDY_Y_OVERLAY + 2);
    buddyPrint("?");
  }

  int p = (t / 2) % 6;
  buddySetColor(BUDDY_DIM);
  if (p >= 0) { buddySetCursor(BUDDY_X_CENTER + 16, BUDDY_Y_OVERLAY + 10); buddyPrint("."); }
  if (p >= 2) { buddySetCursor(BUDDY_X_CENTER + 20, BUDDY_Y_OVERLAY + 10); buddyPrint("."); }
  if (p >= 4) { buddySetCursor(BUDDY_X_CENTER + 24, BUDDY_Y_OVERLAY + 10); buddyPrint("."); }
}

// ─── CELEBRATE ─── 跳起来撒欢
static void doCelebrate(uint32_t t) {
  static const char* const CROUCH[5]   = { "            ", "  (\\____/)  ", " ( ^    ^ ) ", " (  O  O  ) ", " /\\_/  \\_/\\ " };
  static const char* const JUMP[5]     = { "  \\(    )/  ", "  (\\____/)  ", " ( ^    ^ ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const PEAK[5]     = { "  \\^    ^/  ", "  (\\____/)  ", " ( ^    ^ ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const SPIN_L[5]   = { "            ", "  (\\____/)  ", "( <    < ) /", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const SPIN_R[5]   = { "            ", "  (\\____/)  ", "\\( >    > )", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const POSE[5]     = { "    \\__/    ", "  (\\____/)  ", " ( ^    ^ ) ", "/(  O  O  )\\", "  \\_/  \\_/  " };

  const char* const* P[6] = { CROUCH, JUMP, PEAK, SPIN_L, SPIN_R, POSE };
  static const uint8_t SEQ[] = { 0,1,2,1,0, 3,4,3,4, 0,1,2,1,0, 5,5 };
  static const int8_t Y_SHIFT[] = { 0,-3,-7,-3,0, 0,0,0,0, 0,-3,-7,-3,0, 0,0 };
  uint8_t beat = (t / 3) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_SHIFT[beat], 0xFD20);

  static const uint16_t cols[] = { BUDDY_YEL, BUDDY_HEART, BUDDY_CYAN, BUDDY_WHITE, BUDDY_GREEN, BUDDY_PURPLE };
  for (int i = 0; i < 7; i++) {
    int phase = (t * 2 + i * 9) % 24;
    int x = BUDDY_X_CENTER - 42 + i * 14;
    int y = BUDDY_Y_OVERLAY - 8 + phase;
    if (y > BUDDY_Y_BASE + 22 || y < 0) continue;
    buddySetColor(cols[i % 6]);
    buddySetCursor(x, y);
    buddyPrint((i + (int)(t/2)) & 1 ? "*" : "o");
  }
}

// ─── DIZZY ─── 左右晃悠，头顶转星星
static void doDizzy(uint32_t t) {
  static const char* const TILT_L[5]   = { "            ", " (\\____/)   ", "( @    @ )  ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const TILT_R[5]   = { "            ", "   (\\____/) ", "  ( @    @ )", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const WOOZY[5]    = { "            ", "  (\\____/)  ", " ( x    @ ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const WOOZY2[5]   = { "            ", "  (\\____/)  ", " ( @    x ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const TUMBLE[5]   = { "            ", "  (\\____/)  ", " ( @    @ ) ", "(   O  O   )", " /\\_/  \\_/\\ " };

  const char* const* P[5] = { TILT_L, TILT_R, WOOZY, WOOZY2, TUMBLE };
  static const uint8_t SEQ[] = { 0,1,0,1, 2,3, 0,1,0,1, 4,4, 2,3 };
  static const int8_t X_SHIFT[] = { -4,4,-4,4, 0,0, -4,4,-4,4, 0,0, 0,0 };
  uint8_t beat = (t / 4) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, 0, 0xFD20, X_SHIFT[beat]);

  static const int8_t OX[] = { 0, 6, 9, 6, 0, -6, -9, -6 };
  static const int8_t OY[] = { -6, -4, 0, 4, 6, 4, 0, -4 };
  uint8_t p1 = t % 8;
  uint8_t p2 = (t + 3) % 8;
  uint8_t p3 = (t + 5) % 8;
  buddySetColor(BUDDY_CYAN);
  buddySetCursor(BUDDY_X_CENTER + OX[p1] - 2, BUDDY_Y_OVERLAY + 6 + OY[p1]);
  buddyPrint("*");
  buddySetColor(BUDDY_YEL);
  buddySetCursor(BUDDY_X_CENTER + OX[p2] - 2, BUDDY_Y_OVERLAY + 6 + OY[p2]);
  buddyPrint("*");
  buddySetColor(BUDDY_WHITE);
  buddySetCursor(BUDDY_X_CENTER + OX[p3] - 2, BUDDY_Y_OVERLAY + 6 + OY[p3]);
  buddyPrint("+");
}

// ─── HEART ─── 冒爱心，眼睛变心形
static void doHeart(uint32_t t) {
  static const char* const DREAMY[5]   = { "            ", "  (\\____/)  ", " ( ^    ^ ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const BLUSH[5]    = { "            ", "  (\\____/)  ", " (#^    ^#) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const EYES_C[5]   = { "            ", "  (\\____/)  ", " ( <3  <3 ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const KISS_A[5]   = { "            ", "  (\\____/)  ", " ( ^    ^ ) ", " (  <> <> ) ", "  \\_/  \\_/  " };
  static const char* const KISS_B[5]   = { "            ", "  (\\____/)  ", " ( ^    ^ ) ", " (  == == ) ", "  \\_/  \\_/  " };
  static const char* const TWIRL[5]    = { "            ", "  (\\____/)  ", " ( @    @ ) ", "(   O  O   )", " /\\_/  \\_/\\ " };
  static const char* const SIGH[5]     = { "            ", "  (\\____/)  ", " ( -    - ) ", " (  O  O  ) ", "  \\_/  \\_/  " };
  static const char* const HUG[5]      = { "  v      v  ", "  (\\____/)  ", " ( ^    ^ ) ", "/(  O  O  )\\", "  \\_/  \\_/  " };

  const char* const* P[8] = { DREAMY, BLUSH, EYES_C, KISS_A, KISS_B, TWIRL, SIGH, HUG };
  static const uint8_t SEQ[] = {
    0,0,1,0, 2,2,0, 1,0,6, 0,0,5,5, 3,4,3,4, 0,1,0,2, 7,7,0, 1,0
  };
  static const int8_t Y_BOB[] = { 0,-1,0,-1, 0,-1,0, -1,0,0, -1,0,0,0, -1,0,-1,0, -1,0,-1,0, 0,-1,0, -1,0 };
  uint8_t beat = (t / 5) % sizeof(SEQ);
  buddyPrintSprite(P[SEQ[beat]], 5, Y_BOB[beat], 0xFD20);

  buddySetColor(BUDDY_HEART);
  for (int i = 0; i < 6; i++) {
    int phase = (t + i * 3) % 18;
    int y = BUDDY_Y_OVERLAY + 16 - phase;
    if (y < -2 || y > BUDDY_Y_BASE) continue;
    int x = BUDDY_X_CENTER - 22 + i * 8 + ((phase / 3) & 1) * 2 - 1;
    buddySetCursor(x, y);
    buddyPrint("v");
  }

  if ((t / 6) & 1) {
    buddySetColor(BUDDY_RED);
    buddySetCursor(BUDDY_X_CENTER - 34, BUDDY_Y_OVERLAY + 20 + (t % 6));
    buddyPrint("<3");
  }
}

}  // namespace chonk

extern const Species CHONK_SPECIES = {
  "chonk",
  0xFD20,
  {
    chonk::doSleep,
    chonk::doIdle,
    chonk::doBusy,
    chonk::doAttention,
    chonk::doCelebrate,
    chonk::doDizzy,
    chonk::doHeart
  }
};