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
        FieldRenderer::render(buf, Field::LABEL, 0, 7,  0, 0, "DATA INDEX", CellColor::WHITE);
        std::string pageNo = (m_subPage == 0) ? "1/2" : "2/2";
        FieldRenderer::render(buf, Field::LABEL, 0, 19, 0, 0, pageNo, CellColor::WHITE);
        buf.setCell(0, 22, 0x2190, CellColor::WHITE, 14);
        buf.setCell(0, 23, 0x2192, CellColor::WHITE, 14);

        if (m_subPage == 0)
            buildPage1(buf);
        else
            buildPage2(buf);
    }

private:
    int m_subPage = 0;
    ClickHandler m_chAcStatus{0, false, nullptr, "AC_STATUS"};

    void buildPage1(ScreenBuffer& buf) {
        FieldRenderer::render(buf, Field::LABEL_SMALL, 1,  0, 0, 0, "POSITION",    CellColor::WHITE);
        FieldRenderer::render(buf, Field::LABEL,      2,  0, 0, 0, "<MONITOR",     CellColor::WHITE);

        FieldRenderer::render(buf, Field::LABEL_SMALL, 3,  0, 0, 0, "IRS",         CellColor::WHITE);
        FieldRenderer::render(buf, Field::LABEL,      4,  0, 0, 0, "<MONITOR",     CellColor::WHITE);

        FieldRenderer::render(buf, Field::LABEL_SMALL, 5,  0, 0, 0, "GPS",         CellColor::WHITE);
        FieldRenderer::render(buf, Field::LABEL,      6,  0, 0, 0, "<MONITOR",     CellColor::WHITE);

        // LSK4: A/C STATUS
        FieldRenderer::render(buf, Field::LABEL_SMALL, 7,  0, 0, 0, "A/C STATUS", CellColor::WHITE);
        FieldRenderer::render(buf, Field::LABEL,      8,  0, 0, 0, "<A/C STATUS", CellColor::WHITE);

        FieldRenderer::render(buf, Field::LABEL_SMALL, 7, 13, 0, 0, "ACARS/PRINT", CellColor::WHITE);
        FieldRenderer::render(buf, Field::LABEL,      8, 15, 0, 0, "FUNCTION>",   CellColor::WHITE);

        FieldRenderer::render(buf, Field::LABEL_SMALL, 9,  0, 0, 0, "CLOSEST",     CellColor::WHITE);
        FieldRenderer::render(buf, Field::LABEL,     10,  0, 0, 0, "AIRPORTS",    CellColor::WHITE);

        FieldRenderer::render(buf, Field::LABEL_SMALL, 11, 0, 0, 0, "EQUIP",       CellColor::WHITE);
        FieldRenderer::render(buf, Field::LABEL,      12, 0, 0, 0, "POINT",       CellColor::WHITE);
    }

    void buildPage2(ScreenBuffer& buf) {
        FieldRenderer::render(buf, Field::LABEL, 6, 9, 0, 0, "PAGE 2", CellColor::WHITE);
    }
};
