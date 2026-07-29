#pragma once
#include <string>
#include "../MCDU/Field.h"
#include "../Core/FlightPlan.h"

/*
   LatRevPage - LAT REV FROM page.
   Shows lateral revision options for the departure airport.
   Opened by clicking the departure airport on the F-PLN page.
   LSK6 -> RETURN to F-PLN page.
*/
class LatRevPage : public Page {
public:
    LatRevPage(FlightPlan& plan) : m_plan(plan) {}

    const ClickHandler* getClickHandler(int side, int lskIdx) override {
        // LSK6: return to F-PLN
        if (lskIdx == 5 && side == 0)
            return &m_returnHandler;
        return nullptr;
    }

    void buildScreen(ScreenBuffer& buf) override {
        std::string apt = getDeparture();
        std::string title = "LAT REV";

        // Row 0: title centered
        int title_size = title.size()+5;
        int col = (24 - static_cast<int>(title_size + apt.size())) / 2;
        if (col < 0) col = 0;
        FieldRenderer::text(buf, 0, col, title, CellColor::WHITE);
        FieldRenderer::text(buf, 0, col + 8, "FROM", CellColor::WHITE, 14);
        FieldRenderer::text(buf, 0, col + title_size + 1, apt, CellColor::GREEN);

        // ── Left side ──

        // LSK1
        FieldRenderer::text(buf, 2, 0, "<DEPARTURE", CellColor::WHITE, 22);
        // data row empty

        // LSK2
        FieldRenderer::text(buf, 4, 0, "<OFFSET",    CellColor::WHITE, 22);

        // LSK6
        FieldRenderer::text(buf, 12, 0, "<RETURN",   CellColor::WHITE, 22);

        // ── Right side ──

        // RSK1
        FieldRenderer::text(buf, 2, 15, "FIX INFO>", CellColor::WHITE, 22);

        // RSK2: LL XING/INCR/NO  (16 chars, at col 8 to avoid left overlap)
        FieldRenderer::text(buf, 3, 8, "LL XING/INCR/NO", CellColor::WHITE, 14);
        FieldRenderer::text(buf, 4, 10, "[  ]°/[ ]°/[]", CellColor::CYAN);

        // RSK3: NEXT WPT
        FieldRenderer::text(buf, 5, 15, "NEXT WPT", CellColor::WHITE, 14);
        FieldRenderer::text(buf, 6, 16, "[     ]", CellColor::CYAN, 14);

        // RSK4: NEW DEST
        FieldRenderer::text(buf, 7, 15, "NEW DEST", CellColor::WHITE, 14);
        FieldRenderer::text(buf, 8, 20, "[  ]",      CellColor::CYAN, 14);
    }

private:
    FlightPlan&  m_plan;
    ClickHandler m_returnHandler{0, false, nullptr, "FPLN"};

    std::string getDeparture() const {
        // First valid waypoint (not end-of-plan, not discontinuity) = departure
        for (auto* cur = m_plan.first(); cur; cur = cur->next)
            if (!cur->isEndOfPlan && !cur->isDiscontinuity)
                return cur->id;
        return {};
    }
};
