#pragma once
#include <string>
#include "../MCDU/Field.h"

/*
   DATA INDEX  —  Page 1/2

   Title:     DATA INDEX              1/2 <->

   LSK1:  POSITION (14)             RSK4:  ACARS/PRINT (14)
          <MONITOR (22)                     FUNCTION> (22)
   LSK2:  IRS      (14)
          <MONITOR (22)
   LSK3:  GPS      (14)
          <MONITOR (22)
   LSK4:  A/C STATUS (14)
          <A/C STATUS (22)
   LSK5:  CLOSEST  (14)
          AIRPORTS (22)
   LSK6:  EQUIP    (14)
          POINT    (22)

   Scratchpad: GPS PRIMARY LOST (MCDU message, not page content)
*/
class DataPage : public Page {
public:
    DataPage() = default;

    const ClickHandler* getClickHandler(int side, int lskIdx) override {
        if (side == 0 && lskIdx == 3) // LSK4 -> A/C STATUS
            return &m_chAcStatus;
        return nullptr;
    }

    bool onScroll(int delta) override {
        m_subPage = (m_subPage + delta + 2) % 2;
        return true;
    }

    bool needsScrollIndicators() const override { return false; }

    void buildScreen(ScreenBuffer& buf) override {
        FieldRenderer::text(buf, 0, 7,  "DATA INDEX", CellColor::WHITE);
        std::string pageNo = (m_subPage == 0) ? "1/2" : "2/2";
        FieldRenderer::text(buf, 0, 19, pageNo, CellColor::WHITE);
        FieldRenderer::character(buf, 0, 22, 0x2190, CellColor::WHITE, 14);
        FieldRenderer::character(buf, 0, 23, 0x2192, CellColor::WHITE, 14);

        if (m_subPage == 0)
            buildPage1(buf);
        else
            buildPage2(buf);
    }

private:
    int m_subPage = 0;
    ClickHandler m_chAcStatus{0, false, nullptr, "AC_STATUS"};

    void buildPage1(ScreenBuffer& buf) {
        FieldRenderer::text(buf, 1,  0, "POSITION",    CellColor::WHITE, 14);
        FieldRenderer::text(buf, 2,  0, "<MONITOR",     CellColor::WHITE);

        FieldRenderer::text(buf, 3,  0, "IRS",         CellColor::WHITE, 14);
        FieldRenderer::text(buf, 4,  0, "<MONITOR",     CellColor::WHITE);

        FieldRenderer::text(buf, 5,  0, "GPS",         CellColor::WHITE, 14);
        FieldRenderer::text(buf, 6,  0, "<MONITOR",     CellColor::WHITE);

        FieldRenderer::text(buf, 8,  0, "<A/C STATUS", CellColor::WHITE);

        FieldRenderer::text(buf, 7, 13, "ACARS/PRINT", CellColor::WHITE, 14);
        FieldRenderer::text(buf, 8, 15, "FUNCTION>",   CellColor::WHITE);

        FieldRenderer::text(buf, 9,  0, "CLOSEST",     CellColor::WHITE, 14);
        FieldRenderer::text(buf, 10, 0, "AIRPORTS",    CellColor::WHITE);

        FieldRenderer::text(buf, 11, 0, "EQUIP",       CellColor::WHITE, 14);
        FieldRenderer::text(buf, 12, 0, "POINT",       CellColor::WHITE);
    }

    void buildPage2(ScreenBuffer& buf) {
        FieldRenderer::text(buf, 6, 9, "PAGE 2", CellColor::WHITE);
    }
};
