#pragma once
#include <string>
#include "../MCDU/Field.h"

class MenuPage : public Page {
public:
    MenuPage() = default;

    const ClickHandler* getClickHandler(int side, int lskIdx) override {
        (void)side; (void)lskIdx;
        return nullptr;
    }

    void buildScreen(ScreenBuffer& buf) override {
        buf.setString(0, 8, "MCDU MENU", CellColor::WHITE);

        buf.setString(2, 0, "<FMGC (REQ)", CellColor::GREEN);
        buf.setString(4, 0, "<ATSU",       CellColor::WHITE);
        buf.setString(6, 0, "<AIDS",       CellColor::WHITE);
        buf.setString(8, 0, "<CFDS",       CellColor::WHITE);

        buf.setString(1, 15, "SELECT",     CellColor::WHITE, 14);
        buf.setString(2, 15, "NAV B/UP>",  CellColor::WHITE);
    }
};
