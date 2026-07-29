#pragma once
#include <string>
#include "../MCDU/Field.h"
#include "../MCDU/McduDisplayState.h"

/*
   LatRevPage - LAT REV FROM page.
   Shows lateral revision options for the departure airport.
   Opened by clicking the departure airport on the F-PLN page.
   LSK6 -> RETURN to F-PLN page.
   Reads departure airport from McduDisplayState::fplnSnapshot (bus copy).
*/
class LatRevPage : public Page {
public:
    LatRevPage(McduDisplayState& display) : m_disp(display) {}

    const ClickHandler* getClickHandler(int side, int lskIdx) override {
        if (lskIdx == 5 && side == 0)
            return &m_returnHandler;
        return nullptr;
    }

    void buildScreen(ScreenBuffer& buf) override {
        std::string apt = getDeparture();
        int col = (24 - 17) / 2;  // "LAT REV FROM XXXX" ≈ 17 avg
        if (col < 0) col = 0;
        FieldRenderer::text(buf, 0, col,      "LAT REV FROM", CellColor::WHITE, 14);
        FieldRenderer::text(buf, 0, col + 13, apt,            CellColor::GREEN);

        // Left side
        FieldRenderer::text(buf, 2, 0, "<DEPARTURE", CellColor::WHITE);
        FieldRenderer::text(buf, 4, 0, "<OFFSET",    CellColor::WHITE);
        FieldRenderer::text(buf, 12, 0, "<RETURN",   CellColor::WHITE);

        // Right side
        FieldRenderer::text(buf, 2,  15, "FIX INFO>",    CellColor::WHITE);
        FieldRenderer::text(buf, 3,  8,  "LL XING/INCR/NO", CellColor::WHITE, 14);
        FieldRenderer::text(buf, 4,  10, "[   ]°/[ ]°/[]",  CellColor::CYAN);
        FieldRenderer::text(buf, 5,  15, "NEXT WPT",       CellColor::WHITE, 14);
        FieldRenderer::text(buf, 6,  16, "[     ]",        CellColor::CYAN, 14);
        FieldRenderer::text(buf, 7,  15, "NEW DEST",       CellColor::WHITE, 14);
        FieldRenderer::text(buf, 8,  20, "[  ]",            CellColor::CYAN, 14);

    }

private:
    McduDisplayState& m_disp;
    ClickHandler      m_returnHandler{0, false, nullptr, "FPLN"};

    std::string getDeparture() const {
        for (const auto& w : m_disp.fplnCache)
            if (!w.isEndOfPlan && !w.isDiscontinuity)
                return w.id;
        return {};
    }
};
