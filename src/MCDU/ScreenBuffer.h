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

    void setString(int row, int col, const std::string& str,
                   CellColor color = CellColor::GREEN, uint8_t fontSize = 22) {
        for (size_t i = 0; i < str.size() && col + static_cast<int>(i) < COLS; i++)
            setCell(row, col + static_cast<int>(i), static_cast<uint8_t>(str[i]), color, fontSize);
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
