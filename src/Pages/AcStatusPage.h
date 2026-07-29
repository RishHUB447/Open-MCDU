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
        FieldRenderer::text(buf, 0, 8,  "A321-200", CellColor::WHITE);

        // LSK1: ENG / PHI133G
        FieldRenderer::text(buf, 1, 1, "ENG",     CellColor::WHITE, 14);
        FieldRenderer::text(buf, 2, 0, "PW1133GA", CellColor::GREEN);

        // LSK2: ACTIVE NAV DATA BASE / 22FEB20-21MAR TS22402001
        FieldRenderer::text(buf, 3, 1, "ACTIVE NAV DATA BASE", CellColor::WHITE, 14);
        FieldRenderer::text(buf, 4, 1,  "XXXXXXX-XXXXX", CellColor::CYAN, 14);
        FieldRenderer::text(buf, 4, 15, "XXXXXXXXX",    CellColor::GREEN, 14);

        // LSK3: SECOND NAV DATA BASE / <25JAN2024-22FEB2024
        FieldRenderer::text(buf, 5, 1, "SECOND NAV DATA BASE",  CellColor::WHITE, 14);
        FieldRenderer::text(buf, 6, 0, "←XXXXXXXX-XXXXXXXXX", CellColor::CYAN, 14);

        // LSK4: (empty) — rows 7-8 blank

        // LSK5: CHG CODE / [ ]
        FieldRenderer::text(buf, 9, 0,  "CHG CODE", CellColor::WHITE, 14);
        FieldRenderer::text(buf, 10, 0, "[ ]",       CellColor::CYAN, 14);

        // LSK6: IDLE/PERF / +0.0 (14pt) / +0.3 (22pt)
        FieldRenderer::text(buf, 11, 0, "IDLE/PERF", CellColor::WHITE, 14);
        FieldRenderer::text(buf, 12, 0, "+0.0",      CellColor::GREEN, 14);
        FieldRenderer::text(buf, 12, 5, "/+0.3",     CellColor::GREEN);

        // RSK5: SOFTWARE / STATUS/XLOAD> — shares rows 9-10 with LSK5
        FieldRenderer::text(buf, 9,  12, "SOFTWARE",     CellColor::WHITE, 14);
        FieldRenderer::text(buf, 10, 9,  "STATUS/XLOAD>", CellColor::WHITE);
    }
};
