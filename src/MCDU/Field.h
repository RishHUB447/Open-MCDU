#pragma once
#include <string>
#include <cstdint>
#include "ScreenBuffer.h"

#ifndef DEG
#define DEG "\xB0"
#endif

enum class Align { LEFT, RIGHT };

// Clean field-drawing API. Each function does one thing with explicit parameters.
class FieldRenderer {
public:
    // Plain text at (row, col). Font size is explicit — no more LABEL_SMALL mystery.
    static void text(ScreenBuffer& buf, int row, int col,
                     const std::string& text, CellColor color, uint8_t fontSize = 22)
    {
        buf.setString(row, col, text, color, fontSize);
    }

    // Box field: empty shows ▯▯▯, filled shows content with alignment.
    static void box(ScreenBuffer& buf, int row, int col, int width,
                    const std::string& content,
                    CellColor filledColor = CellColor::GREEN,
                    CellColor emptyColor = CellColor::AMBER,
                    Align align = Align::LEFT,
                    uint8_t fontSize = 22)
    {
        if (width <= 0) return;
        if (content.empty()) {
            fillBoxes(buf, row, col, width, emptyColor, fontSize);
        } else {
            std::string s = content.substr(0, static_cast<size_t>(width));
            if (align == Align::LEFT)
                s.resize(static_cast<size_t>(width), ' ');
            else
                s = std::string(static_cast<size_t>(width - static_cast<int>(s.size())), ' ') + s;
            buf.setString(row, col, s, filledColor, fontSize);
        }
    }

    // Slash field: left-box / right-box (e.g. FROM/TO, ALTN)
    static void slash(ScreenBuffer& buf, int row, int col, int leftW, int rightW,
                      const std::string& leftContent, const std::string& rightContent,
                      CellColor filledColor = CellColor::GREEN,
                      CellColor emptyColor = CellColor::AMBER,
                      uint8_t fontSize = 22)
    {
        bool hasData = !leftContent.empty() || !rightContent.empty();
        CellColor color = hasData ? filledColor : emptyColor;

        if (leftContent.empty())
            fillBoxes(buf, row, col, leftW, color, fontSize);
        else
            buf.setString(row, col, pad(leftContent, leftW), color, fontSize);

        buf.setCell(row, col + leftW, '/', color, fontSize);

        if (rightContent.empty())
            fillBoxes(buf, row, col + leftW + 1, rightW, color, fontSize);
        else
            buf.setString(row, col + leftW + 1, pad(rightContent, rightW, true), color, fontSize);
    }

    // Single character at (row, col) — arrows, degree, box markers, etc.
    static void character(ScreenBuffer& buf, int row, int col, uint32_t codepoint,
                          CellColor color, uint8_t fontSize = 22)
    {
        buf.setCell(row, col, codepoint, color, fontSize);
    }

    // Row of dashes
    static void separator(ScreenBuffer& buf, int row, int col, int width,
                          uint8_t fontSize = 22)
    {
        if (width > 0)
            buf.setString(row, col, std::string(static_cast<size_t>(width), '-'),
                          CellColor::DIM, fontSize);
    }

    // Row of dashes with custom color (for fields that want dashes instead of boxes)
    static void dashLine(ScreenBuffer& buf, int row, int col, int width,
                         CellColor color, uint8_t fontSize = 22)
    {
        if (width > 0)
            buf.setString(row, col, std::string(static_cast<size_t>(width), '-'),
                          color, fontSize);
    }

private:
    static void fillBoxes(ScreenBuffer& buf, int row, int col, int w,
                          CellColor color, uint8_t fontSize)
    {
        for (int i = 0; i < w; i++)
            buf.setCell(row, col + i, BOX_MARKER, color, fontSize);
    }

    static std::string pad(const std::string& s, int w, bool right = false) {
        if (static_cast<int>(s.size()) >= w)
            return s.substr(0, static_cast<size_t>(w));
        std::string r = s;
        if (right)
            return std::string(static_cast<size_t>(w - s.size()), ' ') + r;
        r.resize(static_cast<size_t>(w), ' ');
        return r;
    }
};

// Describes what happens when a bezel-side button is pressed next to this field.
struct ClickHandler {
    uint16_t     busLabel = 0;
    bool         isDirectAction = false;
    std::string* valuePtr = nullptr;
    std::string  navTarget;   // non-empty -> page-state-machine switches to this page

    bool isEditable() const { return busLabel != 0 || isDirectAction || valuePtr != nullptr; }
    std::string currentValue() const { return valuePtr ? *valuePtr : ""; }
};

// Base class for MCDU pages.
class Page {
public:
    virtual ~Page() = default;
    virtual void buildScreen(ScreenBuffer& buf) = 0;
    virtual const ClickHandler* getClickHandler(int side, int lskIdx) = 0;
    virtual bool onScroll(int delta) { (void)delta; return false; }
    virtual bool needsScrollIndicators() const { return false; }
};
