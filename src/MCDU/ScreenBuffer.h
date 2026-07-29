#pragma once
#include <string>
#include <cstdint>

enum class CellColor {
    GREEN,
    AMBER,
    WHITE,
    BLUE,
    CYAN,
    DIM,
    YELLOW
};

// Box marker char (U+25AF) - added to B612 font
constexpr uint32_t BOX_MARKER = 0x25AF;

struct Cell {
    uint32_t ch = ' ';
    CellColor color = CellColor::GREEN;
    uint8_t fontSize = 22;  // 22 = BIG, 14 = SMALL
};

// 14x24 character grid buffer. Renderer walks this per frame.
class ScreenBuffer {
public:
    static constexpr int ROWS = 14;
    static constexpr int COLS = 24;

    uint32_t& at(int row, int col) { return m_cells[row][col].ch; }
    uint32_t at(int row, int col) const { return m_cells[row][col].ch; }

    CellColor colorAt(int row, int col) const { return m_cells[row][col].color; }
    void setColor(int row, int col, CellColor c) { m_cells[row][col].color = c; }
    void setChar(int row, int col, uint32_t ch) { m_cells[row][col].ch = ch; }

    void setCell(int row, int col, uint32_t ch, CellColor color, uint8_t fontSize = 22) {
        m_cells[row][col].ch = ch;
        m_cells[row][col].color = color;
        m_cells[row][col].fontSize = fontSize;
    }

    void setFontSize(int row, int col, uint8_t size) {
        m_cells[row][col].fontSize = size;
    }

    uint8_t fontSizeAt(int row, int col) const {
        return m_cells[row][col].fontSize;
    }

    // Write a string, decoding UTF-8 multi-byte sequences into single codepoints.
    // This lets you use chars like \u2190 ("←") directly in string literals.
    void setString(int row, int col, const std::string& str,
                   CellColor color = CellColor::GREEN, uint8_t fontSize = 22) {
        size_t ci = static_cast<size_t>(col);
        size_t i = 0;
        while (i < str.size() && ci < COLS) {
            uint32_t cp;
            unsigned char b = static_cast<unsigned char>(str[i]);
            if (b < 0x80) {
                cp = b; i += 1;
            } else if ((b & 0xE0) == 0xC0 && i + 1 < str.size()) {
                cp = (static_cast<uint32_t>(b & 0x1F) << 6)
                   | static_cast<uint32_t>(str[i+1] & 0x3F);
                i += 2;
            } else if ((b & 0xF0) == 0xE0 && i + 2 < str.size()) {
                cp = (static_cast<uint32_t>(b & 0x0F) << 12)
                   | (static_cast<uint32_t>(str[i+1] & 0x3F) << 6)
                   | static_cast<uint32_t>(str[i+2] & 0x3F);
                i += 3;
            } else if ((b & 0xF8) == 0xF0 && i + 3 < str.size()) {
                cp = (static_cast<uint32_t>(b & 0x07) << 18)
                   | (static_cast<uint32_t>(str[i+1] & 0x3F) << 12)
                   | (static_cast<uint32_t>(str[i+2] & 0x3F) << 6)
                   | static_cast<uint32_t>(str[i+3] & 0x3F);
                i += 4;
            } else {
                i += 1;  // invalid leading byte, skip
                continue;
            }
            setCell(row, static_cast<int>(ci), cp, color, fontSize);
            ci++;
        }
    }

    void clearRow(int row) {
        for (int c = 0; c < COLS; c++)
            m_cells[row][c] = Cell{};
    }

    void clearAll() {
        for (int r = 0; r < ROWS; r++)
            clearRow(r);
    }

    const Cell* data() const { return &m_cells[0][0]; }
    Cell* data() { return &m_cells[0][0]; }

private:
    Cell m_cells[ROWS][COLS]{};
};
