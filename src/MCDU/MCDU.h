#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <iostream>
#include <string>
#include <memory>
#include "BezelLayout.h"
#include "MCDUButton.h"
#include "ScreenBuffer.h"
#include "Scratchpad.h"
#include "PageStateMachine.h"
#include "../Pages/InitPage.h"
#include "../Pages/FplnPage.h"
#include "../Pages/MenuPage.h"
#include "../Core/FMGC.h"

/*
   MCDU - pure display/input component. Does NOT own the window or event loop.
     render(target)      draw onto any sf::RenderTarget
     handleButton(btn)   route a bezel button press
     inputChar(c)        type into scratchpad (auto-uppercase)
     hitTestLsk(x, y)    check click against bezel button zones
     rebuildScreen()     redraw current page + scratchpad
*/
class MCDU {
public:
    static constexpr MCDUButton NO_HIT = static_cast<MCDUButton>(999);

    MCDU(FMGC& fmgc)
        : m_bgSprite(m_bgTex)
        , m_fmgc(fmgc)
    {
        bool bg_ok = m_bgTex.loadFromFile("assets/MCDU.png");
        if (bg_ok)
            m_bgSprite.setTexture(m_bgTex, true);
        else
            std::cerr << "WARN: Could not load assets/MCDU.png\n";

        bool font_ok = m_font.openFromFile("assets/B612-Regular.ttf");
        if (!font_ok)
            std::cerr << "WARN: Could not load assets/B612-Regular.ttf\n";

        m_lcdBg.setSize({LCD_W, LCD_H});
        m_lcdBg.setPosition({LCD_X, LCD_Y});
        m_lcdBg.setFillColor(sf::Color(0, 0, 0, 180));

        m_stateMachine.registerPage("INIT", std::make_unique<InitPage>(fmgc.dataStore(), fmgc));
        m_stateMachine.registerPage("FPLN", std::make_unique<FplnPage>(fmgc.dataStore(), fmgc.flightPlan(), fmgc.navDatabase()));
        m_stateMachine.registerPage("MENU", std::make_unique<MenuPage>());
        m_stateMachine.switchTo("INIT");
    }

    void handleButton(MCDUButton btn) {
        switch (btn) {
            case MCDUButton::NUM_0: inputChar('0'); return;
            case MCDUButton::NUM_1: inputChar('1'); return;
            case MCDUButton::NUM_2: inputChar('2'); return;
            case MCDUButton::NUM_3: inputChar('3'); return;
            case MCDUButton::NUM_4: inputChar('4'); return;
            case MCDUButton::NUM_5: inputChar('5'); return;
            case MCDUButton::NUM_6: inputChar('6'); return;
            case MCDUButton::NUM_7: inputChar('7'); return;
            case MCDUButton::NUM_8: inputChar('8'); return;
            case MCDUButton::NUM_9: inputChar('9'); return;
            case MCDUButton::NUM_DOT: inputChar('.'); return;
            case MCDUButton::NUM_SIGN: inputChar('-'); return;
            default: break;
        }

        // CLR mode flow: empty -> put "CLR", "CLR"+CLR -> clear
        if (btn == MCDUButton::CLR) {
            if (m_scratchpad.isMessage())
                m_scratchpad.clear();
            else if (m_scratchpad.isEmpty())
                m_scratchpad.setText("CLR");
            else if (m_scratchpad.text() == "CLR")
                m_scratchpad.clear();
            else
                m_scratchpad.deleteChar();
            rebuildScratchpadLine();
            return;
        }

        std::string err{""};
        m_stateMachine.handleButton(btn, m_scratchpad, err);
        if (!err.empty())
            m_scratchpad.showMessage(err);
        rebuildScreen();
    }

    void inputChar(char c) {
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 32);
        if (c >= 32 && c <= 126) {
            m_scratchpad.inputChar(c);
            rebuildScratchpadLine();
        }
    }

    MCDUButton hitTestLsk(float mx, float my) const {
        // LSK/RSK side keys
        {
            static constexpr MCDUButton left[] = {
                MCDUButton::L1, MCDUButton::L2, MCDUButton::L3,
                MCDUButton::L4, MCDUButton::L5, MCDUButton::L6
            };
            static constexpr MCDUButton right[] = {
                MCDUButton::R1, MCDUButton::R2, MCDUButton::R3,
                MCDUButton::R4, MCDUButton::R5, MCDUButton::R6
            };
            for (int i = 0; i < 6; ++i) {
                float y = LSK1_Y + i * LSK_PITCH;
                auto inRect = [&](float rx, float rw) {
                    return mx >= rx && mx < rx + rw && my >= y && my < y + LSK_H;
                };
                if (inRect(LSK_X, LSK_W))  return left[i];
                if (inRect(RSK_X, LSK_W))  return right[i];
            }
        }

        // Function key grid: 2 rows x 6 cols
        {
            static constexpr MCDUButton fnGrid[2][6] = {
                {MCDUButton::DIR, MCDUButton::PROG, MCDUButton::PERF,
                 MCDUButton::INIT, MCDUButton::DATA, NO_HIT},
                {MCDUButton::FPLN, MCDUButton::RAD_NAV, MCDUButton::FUEL_PRED,
                 MCDUButton::SEC_F_PLN, MCDUButton::ATC_COMM, MCDUButton::MCDU_MENU}
            };
            for (int r = 0; r < FN_ROWS; ++r) {
                for (int c = 0; c < FN_COLS; ++c) {
                    float bx = FN_GRID_X + c * FN_PITCH_X;
                    float by = FN_GRID_Y + r * FN_PITCH_Y;
                    if (mx >= bx && mx < bx + FN_W && my >= by && my < by + FN_H) {
                        MCDUButton btn = fnGrid[r][c];
                        if (btn == NO_HIT) continue;
                        return btn;
                    }
                }
            }
        }

        // Action key grid: 3 rows x 2 cols
        {
            static constexpr MCDUButton actGrid[3][2] = {
                {MCDUButton::AIRPORT, NO_HIT},
                {MCDUButton::SCROLL_LEFT, MCDUButton::SCROLL_UP},
                {MCDUButton::SCROLL_RIGHT, MCDUButton::SCROLL_DOWN}
            };
            for (int r = 0; r < ACT_ROWS; ++r) {
                for (int c = 0; c < ACT_COLS; ++c) {
                    float bx = ACT_GRID_X + c * ACT_PITCH_X;
                    float by = ACT_GRID_Y + r * ACT_PITCH_Y;
                    if (mx >= bx && mx < bx + ACT_W && my >= by && my < by + ACT_H) {
                        MCDUButton btn = actGrid[r][c];
                        if (btn == NO_HIT) continue;
                        return btn;
                    }
                }
            }
        }

        // Numeric keypad: 3 cols x 4 rows
        {
            static constexpr MCDUButton kpad[4][3] = {
                {MCDUButton::NUM_1, MCDUButton::NUM_2, MCDUButton::NUM_3},
                {MCDUButton::NUM_4, MCDUButton::NUM_5, MCDUButton::NUM_6},
                {MCDUButton::NUM_7, MCDUButton::NUM_8, MCDUButton::NUM_9},
                {MCDUButton::NUM_DOT, MCDUButton::NUM_0, MCDUButton::NUM_SIGN}
            };
            for (int r = 0; r < KPAD_ROWS; ++r) {
                for (int c = 0; c < KPAD_COLS; ++c) {
                    float bx = KPAD_X + c * KPAD_PITCH_X;
                    float by = KPAD_Y + r * KPAD_PITCH_Y;
                    if (mx >= bx && mx < bx + KPAD_W && my >= by && my < by + KPAD_H)
                        return kpad[r][c];
                }
            }
        }

        return NO_HIT;
    }

    void rebuildScreen() {
        m_screen.clearAll();
        if (m_stateMachine.currentPage())
            m_stateMachine.currentPage()->buildScreen(m_screen);
        rebuildScratchpadLine();

        if (m_stateMachine.currentPage() &&
            m_stateMachine.currentPage()->needsScrollIndicators()) {
            // up/down arrows on bottom-right
            m_screen.setCell(13, 22, 0x2191, CellColor::WHITE);
            m_screen.setCell(13, 23, 0x2193, CellColor::WHITE);
        }
    }

    void render(sf::RenderTarget& target) {
        if (m_bgTex.getNativeHandle() != 0) {
            m_bgSprite.setPosition({0, 0});
            target.draw(m_bgSprite);
        }

        target.draw(m_lcdBg);

        for (int r = 0; r < ScreenBuffer::ROWS; ++r) {
            for (int c = 0; c < ScreenBuffer::COLS; ++c) {
                uint32_t ch = m_screen.at(r, c);
                if (ch == ' ') continue;

                sf::Text cell_text = makeCellText(m_font, ch, m_screen.fontSizeAt(r, c));
                cell_text.setFillColor(cellColorToSfml(m_screen.colorAt(r, c)));

                sf::FloatRect tb = cell_text.getLocalBounds();
                float px = LCD_X + c * CELL_W + (CELL_W - tb.size.x) / 2.f;
                float py = LCD_Y + r * CELL_H + (CELL_H - tb.size.y) / 2.f - tb.position.y;
                cell_text.setPosition({px, py});
                target.draw(cell_text);
            }
        }

        // Debug bar at top
        {
            std::string spInfo = m_scratchpad.isEmpty()
                ? "SCRATCHPAD EMPTY"
                : m_scratchpad.text() + (m_scratchpad.isMessage() ? " [MSG]" : "");
            sf::Text info(m_font,
                "INIT PAGE  |  " + spInfo + "  |  CLICK BEZEL BUTTONS",
                11);
            info.setFillColor(sf::Color::White);
            info.setPosition({5.f, 5.f});
            target.draw(info);
        }
    }

    const ScreenBuffer& screen() const { return m_screen; }
    ScreenBuffer& screen() { return m_screen; }
    Scratchpad& scratchpad() { return m_scratchpad; }
    FMGC& fmgc() { return m_fmgc; }

private:
    static sf::Text makeCellText(const sf::Font& font, uint32_t codepoint, uint8_t size) {
        std::wstring ws;
        if (codepoint <= 0xFFFF) {
            ws += static_cast<wchar_t>(codepoint);
        } else {
            codepoint -= 0x10000;
            ws += static_cast<wchar_t>(0xD800 | (codepoint >> 10));
            ws += static_cast<wchar_t>(0xDC00 | (codepoint & 0x3FF));
        }
        return sf::Text(font, sf::String(ws), size);
    }

    static sf::Color cellColorToSfml(CellColor c) {
        switch (c) {
            case CellColor::GREEN: return sf::Color(0, 230, 0);
            case CellColor::AMBER: return sf::Color(230, 150, 0);
            case CellColor::WHITE: return sf::Color(210, 210, 210);
            case CellColor::BLUE:  return sf::Color(0, 150, 255);
            case CellColor::CYAN:  return sf::Color(0, 255, 255);
            case CellColor::DIM:   return sf::Color(0, 100, 0);
            case CellColor::YELLOW: return sf::Color(255, 220, 0);
        }
        return sf::Color(0, 230, 0);
    }

    void rebuildScratchpadLine() {
        m_screen.clearRow(13);
        if (!m_scratchpad.isEmpty()) {
            CellColor col = m_scratchpad.isMessage() ? CellColor::AMBER : CellColor::WHITE;
            std::string txt = m_scratchpad.text();
            if (txt.size() > 22)
                txt.resize(22);
            m_screen.setString(13, 0, txt, col);
        }
    }

    sf::Font m_font;
    sf::Texture m_bgTex;
    sf::Sprite m_bgSprite;
    sf::RectangleShape m_lcdBg;

    ScreenBuffer m_screen;
    Scratchpad m_scratchpad;
    PageStateMachine m_stateMachine;

    FMGC& m_fmgc;
};
