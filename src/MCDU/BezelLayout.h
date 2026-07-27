#pragma once

// Bezel image and LCD pixel coordinates (from GIMP)
// All positions relative to bezel image top-left

constexpr float BEZEL_W = 648.f;
constexpr float BEZEL_H = 1000.f;

constexpr float LCD_X = 111.f;
constexpr float LCD_Y = 78.f;
constexpr float LCD_W = 425.f;
constexpr float LCD_H = 354.f;

constexpr int LCD_COLS = 24;
constexpr int LCD_ROWS = 14;

constexpr float CELL_W = LCD_W / LCD_COLS;
constexpr float CELL_H = LCD_H / LCD_ROWS;

// LSK/RSK side keys (beside LCD)
constexpr float LSK_X    = 15.f;
constexpr float LSK_W    = 43.f;
constexpr float LSK_H    = 30.f;
constexpr float LSK_GAP  = 20.f;
constexpr float LSK1_Y   = 127.f;
constexpr float RSK_X    = 588.f;
constexpr float LSK_PITCH = 50.f;

// Function key grid (below LCD) - 2 rows x 6 cols
constexpr float FN_GRID_X = 67.f;
constexpr float FN_GRID_Y = 488.f;
constexpr float FN_W = 67.f;
constexpr float FN_H = 44.f;
constexpr float FN_GAP_X = 12.f;
constexpr float FN_GAP_Y = 9.f;
constexpr float FN_PITCH_X = FN_W + FN_GAP_X;
constexpr float FN_PITCH_Y = FN_H + FN_GAP_Y;
constexpr int FN_ROWS = 2;
constexpr int FN_COLS = 6;

// Action key grid (below function keys) - 3 rows x 2 cols
constexpr float ACT_GRID_X = 67.f;
constexpr float ACT_GRID_Y = 594.f;
constexpr float ACT_W = 67.f;
constexpr float ACT_H = 44.f;
constexpr float ACT_GAP_X = 12.f;
constexpr float ACT_GAP_Y = 9.f;
constexpr float ACT_PITCH_X = ACT_W + ACT_GAP_X;
constexpr float ACT_PITCH_Y = ACT_H + ACT_GAP_Y;
constexpr int ACT_ROWS = 3;
constexpr int ACT_COLS = 2;

// Numeric keypad (right side) - 3 cols x 4 rows
constexpr float KPAD_X = 77.f;
constexpr float KPAD_Y = 765.f;
constexpr float KPAD_W = 42.f;
constexpr float KPAD_H = 42.f;
constexpr float KPAD_GAP_X = 22.f;
constexpr float KPAD_GAP_Y = 12.f;
constexpr float KPAD_PITCH_X = KPAD_W + KPAD_GAP_X;
constexpr float KPAD_PITCH_Y = KPAD_H + KPAD_GAP_Y;
constexpr int KPAD_ROWS = 4;
constexpr int KPAD_COLS = 3;
