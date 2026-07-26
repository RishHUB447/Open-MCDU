#pragma once
// ---------- Bezel image bounds ----------
constexpr float BEZEL_W = 648.f;
constexpr float BEZEL_H = 1000.f;

// ---------- LCD screen area (pixel coords relative to bezel top-left) ----------
constexpr float LCD_X = 109.f;
constexpr float LCD_Y = 68.f;
constexpr float LCD_W = 429.f;
constexpr float LCD_H = 373.f;

// ---------- Character grid ----------
constexpr int LCD_COLS = 24;
constexpr int LCD_ROWS = 14;

// Derived cell dimensions
constexpr float CELL_W = LCD_W / LCD_COLS;  // 17.875 px
constexpr float CELL_H = LCD_H / LCD_ROWS;  // ~26.64 px

// ---------- LSK buttons (left side) ----------
// Each: 43w x 30h. First at Y=127, 20px gap between buttons.
// R1-R6 share the same Y positions, just different X.
constexpr float LSK_X    = 15.f;
constexpr float LSK_W    = 43.f;
constexpr float LSK_H    = 30.f;
constexpr float LSK_GAP  = 20.f;
constexpr float LSK1_Y   = 127.f;

// ---------- RSK buttons (right side) ----------
constexpr float RSK_X    = 588.f;
// RSK_W/H/GAP are same as LSK

// ---------- Button Y helpers ----------
// L1 = LSK1_Y, L2 = LSK1_Y + 1*50, L3 = LSK1_Y + 2*50, etc.
// (Each button row = 30h + 20 gap = 50 px pitch)
constexpr float LSK_PITCH = LSK_H + LSK_GAP;  // 50 px

// ---------- Function keys (YET TO DO) ----------
// DIR, PROG, PERF, INIT, FPLN, CLR, DEL, ENT, MENU, etc.
// COMING SOON
