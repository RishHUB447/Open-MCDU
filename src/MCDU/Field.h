#pragma once
#include <string>
#include <cstdint>
#include "ScreenBuffer.h"

#ifndef DEG
#define DEG "\xB0"
#endif

struct Field {
    enum Type {
        BOX,
        BOX_SMALL,
        LABEL,
        LABEL_SMALL,
        SLASH,      // left/right pair with /
        SEPARATOR
    } type;

    int row, col;
    int width;
    int widthRight;
    std::string label;
};

enum class Align { LEFT, RIGHT };

// Draws fields onto a ScreenBuffer.
class FieldRenderer {
public:
    static void render(ScreenBuffer& buf, Field::Type type,
                       int row, int col, int width, int widthRight,
                       const std::string& leftContent,
                       CellColor filledColor = CellColor::GREEN,
                       const std::string& rightContent = "",
                       Align align = Align::LEFT,
                       CellColor emptyColor = CellColor::AMBER)
    {
        bool isSmall = (type == Field::BOX_SMALL || type == Field::LABEL_SMALL);
        uint8_t fontSize = isSmall ? 14 : 22;

        switch (type) {
            case Field::BOX:
            case Field::BOX_SMALL:
                renderTextField(buf, row, col, width, leftContent, filledColor, align, fontSize, emptyColor);
                break;
            case Field::SLASH:
                renderSlashField(buf, row, col, width, widthRight, leftContent, rightContent, filledColor, align, fontSize, emptyColor);
                break;
            case Field::LABEL:
            case Field::LABEL_SMALL:
                buf.setString(row, col, leftContent, filledColor, fontSize);
                break;
            case Field::SEPARATOR: {
                int n = width > 0 ? width : 0;
                buf.setString(row, col, std::string(static_cast<size_t>(n), '-'), CellColor::DIM, fontSize);
                break;
            }
        }
    }

private:
    static void fillBoxes(ScreenBuffer& buf, int row, int col, int w,
                          CellColor color, uint8_t fontSize = 22)
    {
        for (int i = 0; i < w; i++)
            buf.setCell(row, col + i, BOX_MARKER, color, fontSize);
    }

    static void renderTextField(ScreenBuffer& buf, int row, int col, int w,
                                const std::string& content, CellColor filledColor,
                                Align align, uint8_t fontSize = 22,
                                CellColor emptyColor = CellColor::AMBER)
    {
        if (w <= 0) return;
        if (content.empty()) {
            fillBoxes(buf, row, col, w, emptyColor, fontSize);
        } else {
            std::string trimmed = content.substr(0, static_cast<size_t>(w));
            if (align == Align::LEFT) {
                trimmed.resize(static_cast<size_t>(w), ' ');
            } else {
                int pad = w - static_cast<int>(trimmed.size());
                if (pad > 0)
                    trimmed = std::string(static_cast<size_t>(pad), ' ') + trimmed;
            }
            buf.setString(row, col, trimmed, filledColor, fontSize);
        }
    }

    static void renderSlashField(ScreenBuffer& buf, int row, int col,
                                 int leftW, int rightW,
                                 const std::string& left, const std::string& right,
                                 CellColor filledColor, Align align,
                                 uint8_t fontSize = 22,
                                 CellColor emptyColor = CellColor::AMBER)
    {
        bool hasData = !left.empty() || !right.empty();
        CellColor color = hasData ? filledColor : emptyColor;

        if (left.empty()) {
            fillBoxes(buf, row, col, leftW, color, fontSize);
        } else {
            std::string s = left.substr(0, static_cast<size_t>(leftW));
            s.resize(static_cast<size_t>(leftW), ' ');
            buf.setString(row, col, s, color, fontSize);
        }

        buf.setCell(row, col + leftW, '/', color, fontSize);

        if (right.empty()) {
            fillBoxes(buf, row, col + leftW + 1, rightW, color, fontSize);
        } else {
            std::string s = right.substr(0, static_cast<size_t>(rightW));
            int pad = rightW - static_cast<int>(s.size());
            if (pad > 0)
                s = std::string(static_cast<size_t>(pad), ' ') + s;
            buf.setString(row, col + leftW + 1, s, color, fontSize);
        }
    }
};

// Describes what happens when a bezel-side button is pressed next to this field.
// busLabel = ARINC label to send on the bus (0 = no bus action)
// isDirectAction = true -> scratchpad not consumed, just send label as-is
// valuePtr = pointer to read-back value for scratchpad (may point to temp storage)
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
