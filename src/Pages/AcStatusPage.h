#pragma once
#include <string>
#include "../MCDU/Field.h"

/*
   A/C STATUS  —  Aircraft Status / Data Page

   Title:     A321-200

   LSK1:  ENG                    (14)      RSK1-RSK4: (empty)
          PHI133G                (22)
   LSK2:  ACTIVE NAV DATA BASE   (14)      RSK5: SOFTWARE   (14)
          22FEB20-21MAR TS22402001              STATUS/XLOAD> (22)
   LSK3:  SECOND NAV DATA BASE   (14)
          <25JAN2024-22FEB2024   (22)
   LSK4:  (empty)
   LSK5:  CHG CODE               (14)
          [     ]  CYAN
   LSK6:  IDLE/PERF               (14)
          +0.0/ +0.3  (14pt / 22pt, both GREEN)

   Scratchpad: GPS PRIMARY LOST (MCDU message, not page content)
*/
class AcStatusPage : public Page {
public:
    AcStatusPage() = default;

    const ClickHandler* getClickHandler(int side, int lskIdx) override {
        (void)side; (void)lskIdx;
        return nullptr;
    }

    bool needsScrollIndicators() const override { return false; }

    void buildScreen(ScreenBuffer& buf) override {
        // Title
        FieldRenderer::render(buf, Field::LABEL, 0, 8, 0, 0,
            "A321-200", CellColor::WHITE);

        // LSK1: ENG / PHI133G
        FieldRenderer::render(buf, Field::LABEL_SMALL, 1, 1, 0, 0,
            "ENG", CellColor::WHITE);
        FieldRenderer::render(buf, Field::LABEL, 2, 0, 0, 0,
            "PHI133G", CellColor::GREEN);

        // LSK2: ACTIVE NAV DATA BASE / 22FEB20-21MAR + TS22402001
        FieldRenderer::render(buf, Field::LABEL_SMALL, 3, 1, 0, 0,
            "ACTIVE NAV DATA BASE", CellColor::WHITE);
        buf.setString(4, 1, "22FEB20-21MAR", CellColor::CYAN, 14U);
        buf.setString(4, 15, "TS22402001", CellColor::GREEN, 14U);

        // LSK3: SECOND NAV DATA BASE / <25JAN2024-22FEB2024
        FieldRenderer::render(buf, Field::LABEL_SMALL, 5, 1, 0, 0,
            "SECOND NAV DATA BASE", CellColor::WHITE);
        FieldRenderer::render(buf, Field::LABEL_SMALL, 6, 0, 0, 0,
            "<25JAN2024-22FEB2024", CellColor::CYAN);

        // LSK4: (empty) — rows 7-8 stay blank on the left

        // LSK5: CHG CODE / [     ]
        FieldRenderer::render(buf, Field::LABEL_SMALL, 9, 0, 0, 0,
            "CHG CODE", CellColor::WHITE);
        buf.setString(10, 0, "[ ]", CellColor::CYAN, 14U);

        // LSK6: IDLE/PERF / +0.0 (14pt) / +0.3 (22pt) both GREEN
        FieldRenderer::render(buf, Field::LABEL_SMALL, 11, 0, 0, 0,
            "IDLE/PERF", CellColor::WHITE);
        buf.setString(12, 0, "+0.0", CellColor::GREEN, 14);
        buf.setString(12, 5, "/+0.3", CellColor::GREEN, 22);

        // RSK5: SOFTWARE (14pt) / STATUS/XLOAD> (22pt) — shares rows 9-10 with LSK5
        FieldRenderer::render(buf, Field::LABEL_SMALL, 9, 12, 0, 0,
            "SOFTWARE", CellColor::WHITE);
        FieldRenderer::render(buf, Field::LABEL, 10, 9, 0, 0,
            "STATUS/XLOAD>", CellColor::WHITE);
    }
};
