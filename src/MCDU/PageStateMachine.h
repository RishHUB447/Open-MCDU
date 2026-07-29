#pragma once
#include <string>
#include <map>
#include <memory>
#include <algorithm>
#include "Field.h"
#include "ScreenBuffer.h"
#include "Scratchpad.h"
#include "MCDUButton.h"
#include "McduDisplayState.h"
#include "../Core/DataBus.h"

// Page management + button routing via DataBus.
// When an LSK/RSK is pressed, looks up the ClickHandler for that position,
// sends the scratchpad data as a bus message, and marks the field as pending.
class PageStateMachine {
public:
    PageStateMachine(DataBus& bus, McduDisplayState& display)
        : m_bus(bus), m_display(display) {}

    void registerPage(const std::string& name, std::unique_ptr<Page> page) {
        m_pages[name] = std::move(page);
        if (!m_current) m_current = m_pages[name].get();
    }

    bool switchTo(const std::string& name) {
        auto it = m_pages.find(name);
        if (it == m_pages.end()) return false;
        m_current = it->second.get();
        m_subPageIndex = 0;
        return true;
    }

    Page* currentPage() { return m_current; }
    const Page* currentPage() const { return m_current; }

    // delta = +1 (SCROLL_UP), delta = -1 (SCROLL_DOWN)
    // The page's onScroll() can invert this if needed
    void scrollUp() {
        if (m_current && m_current->onScroll(1)) return;
        m_subPageIndex = std::max(0, m_subPageIndex - 1);
    }

    void scrollDown() {
        if (m_current && m_current->onScroll(-1)) return;
        m_subPageIndex++;
    }

    // Returns the error message string if one was generated, empty otherwise.
    // The caller (MCDU) should show it on the scratchpad.
    bool handleButton(MCDUButton btn, Scratchpad& scratchpad, std::string& outMessage) {
        outMessage.clear();

        // Hard function keys -> page switch
        switch (btn) {
            case MCDUButton::FPLN:      switchTo("FPLN");  return true;
            case MCDUButton::DIR:       switchTo("DIR");   return true;
            case MCDUButton::PROG:      switchTo("PROG");  return true;
            case MCDUButton::PERF:      switchTo("PERF");  return true;
            case MCDUButton::INIT:      switchTo("INIT");  return true;
            case MCDUButton::DATA:      switchTo("DATA");  return true;
            case MCDUButton::RAD_NAV:   switchTo("RAD_NAV"); return true;
            case MCDUButton::FUEL_PRED: switchTo("FUEL");  return true;
            case MCDUButton::SEC_F_PLN: switchTo("SEC_F_PLN"); return true;
            case MCDUButton::ATC_COMM:  switchTo("ATC");   return true;
            case MCDUButton::MCDU_MENU: switchTo("MENU");  return true;
            case MCDUButton::AIRPORT:   switchTo("AIRPORT"); return true;
            case MCDUButton::SCROLL_LEFT:
            case MCDUButton::SCROLL_RIGHT: return true;
            case MCDUButton::SCROLL_UP:    scrollUp();      return true;
            case MCDUButton::SCROLL_DOWN:  scrollDown();    return true;
            default: break;
        }

        if (!m_current) return false;

        // Map button to side/idx
        int side = -1, idx = -1;
        switch (btn) {
            case MCDUButton::L1: side = 0; idx = 0; break;
            case MCDUButton::L2: side = 0; idx = 1; break;
            case MCDUButton::L3: side = 0; idx = 2; break;
            case MCDUButton::L4: side = 0; idx = 3; break;
            case MCDUButton::L5: side = 0; idx = 4; break;
            case MCDUButton::L6: side = 0; idx = 5; break;
            case MCDUButton::R1: side = 1; idx = 0; break;
            case MCDUButton::R2: side = 1; idx = 1; break;
            case MCDUButton::R3: side = 1; idx = 2; break;
            case MCDUButton::R4: side = 1; idx = 3; break;
            case MCDUButton::R5: side = 1; idx = 4; break;
            case MCDUButton::R6: side = 1; idx = 5; break;
            default: break;
        }

        if (side >= 0 && idx >= 0) {
            const ClickHandler* handler = m_current->getClickHandler(side, idx);
            if (!handler || !handler->isEditable()) {
                if (handler && !handler->navTarget.empty())
                    switchTo(handler->navTarget);
                return true;
            }

            std::string spData = scratchpad.text();

            // Direct action: fire bus message, don't consume scratchpad
            if (handler->isDirectAction) {
                m_bus.sendToFmgc(handler->busLabel, spData);
                if (handler->busLabel != 0)
                    m_display.markPending(handler->busLabel);
                if (!handler->navTarget.empty())
                    switchTo(handler->navTarget);
                return true;
            }

            // Scratchpad empty -> read back current value into scratchpad
            if (spData.empty()) {
                std::string cur = handler->currentValue();
                if (!cur.empty())
                    scratchpad.setText(cur);
                if (!handler->navTarget.empty())
                    switchTo(handler->navTarget);
                return true;
            }

            // No bus label but valuePtr set -> read-back only, discard scratchpad
            if (handler->busLabel == 0) {
                if (!handler->navTarget.empty())
                    switchTo(handler->navTarget);
                return true;
            }

            // Scratchpad has data -> send to FMGC, clear pad, mark pending
            scratchpad.clear();
            m_bus.sendToFmgc(handler->busLabel, spData);
            m_display.markPending(handler->busLabel);
            if (!handler->navTarget.empty())
                switchTo(handler->navTarget);
            return true;
        }

        return false;
    }

    // Poll the FMGC->MCDU bus for responses and update display state.
    // Returns an error message if FMGC sent one (caller shows on scratchpad).
    std::string pollBusForUpdates() {
        auto msgs = m_bus.pollMcduInbox();
        std::string errorMsg;
        for (auto& msg : msgs) {
            if (msg.label == ArincLabel::ERROR_MSG) {
                errorMsg = msg.payload;
            } else {
                m_display.applyUpdate(msg.label, msg.payload, msg.ssm);
            }
        }
        return errorMsg;
    }

    // Get the Bus tick for latency simulation
    void tickBus(uint64_t nowMs) { m_bus.tick(nowMs); }

private:
    DataBus&          m_bus;
    McduDisplayState& m_display;
    std::map<std::string, std::unique_ptr<Page>> m_pages;
    Page* m_current = nullptr;
    int   m_subPageIndex = 0;
};
