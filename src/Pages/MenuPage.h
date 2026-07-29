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
        FieldRenderer::text(buf, 0, 8, "MCDU MENU",  CellColor::WHITE);
        FieldRenderer::text(buf, 2, 0, "<FMGC (REQ)", CellColor::GREEN);
        FieldRenderer::text(buf, 4, 0, "<ATSU",       CellColor::WHITE);
        FieldRenderer::text(buf, 6, 0, "<AIDS",       CellColor::WHITE);
        FieldRenderer::text(buf, 8, 0, "<CFDS",       CellColor::WHITE);
        FieldRenderer::text(buf, 1, 15, "SELECT",     CellColor::WHITE, 14);
        FieldRenderer::text(buf, 2, 15, "NAV B/UP>",  CellColor::WHITE);
    }
};
