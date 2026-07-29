#pragma once
#include <string>
#include "../MCDU/Field.h"
#include "../MCDU/McduDisplayState.h"
#include "../Core/NavDatabase.h"
#include "../Core/DataBus.h"

/*
   InitPage layout:
     L1: CO RTE  [10]            R1: FROM/TO  [4]/[4]
     L2: ALTN/CO RTE  [4]/[10]   R2: (empty)
     L3: FLT NBR  [7]            R3: IRS INIT> (AMBER, direct action)
     L4: (empty)                 R4: WIND/TEMP> (direct action, to separate page)
     L5: COST INDEX  [3]         R5: TROPO [5] (SMALL)
     L6: CRZ FL/TEMP  [9]        R6: GND TEMP --- (WHITE, small)

   All LSK/RSK clicks send bus messages via ClickHandler.busLabel.
*/
class InitPage : public Page {
public:
    InitPage(McduDisplayState& display, const NavDatabase& navDb, DataBus& bus)
        : m_disp(display), m_navDb(navDb)
    {
        (void)m_navDb;
        (void)bus;

        m_chCoRoute  = {ArincLabel::CO_ROUTE,  false, &m_disp.coRoute};
        m_chAltn     = {ArincLabel::ALTN_ROUTE, false, &m_curAltn};
        m_chFltNbr   = {ArincLabel::FLT_NBR,   false, &m_disp.fltNbr};
        m_chCostIdx  = {ArincLabel::COST_INDEX, false, &m_disp.costIndex};
        m_chCrzFl    = {ArincLabel::CRZ_FL_TEMP, false, &m_disp.crzFlTemp};
        m_chFromTo   = {ArincLabel::FROM_TO,   false, &m_curFromTo};
        m_chTropo    = {ArincLabel::TROPO,     false, &m_disp.tropo};
        m_chGndTemp  = {ArincLabel::GND_TEMP,  false, &m_disp.gndTemp};
        m_chIrsInit  = {ArincLabel::ALIGN_IRS, true,  nullptr};
        m_chWindTemp = {ArincLabel::WIND_TEMP, true,  nullptr};
    }

    const ClickHandler* getClickHandler(int side, int lskIdx) override {
        // Update combined strings for slash fields before returning
        m_curFromTo = m_disp.fromAirport + "/" + m_disp.toAirport;
        m_curAltn   = m_disp.altnCoRteLeft + "/" + m_disp.altnCoRteRight;
        m_chAltn.valuePtr = &m_curAltn;
        m_chFromTo.valuePtr = &m_curFromTo;

        if (side == 0) {
            switch (lskIdx) {
                case 0: return &m_chCoRoute;
                case 1: return &m_chAltn;
                case 2: return &m_chFltNbr;
                case 4: return &m_chCostIdx;
                case 5: return &m_chCrzFl;
            }
        } else {
            switch (lskIdx) {
                case 0: return &m_chFromTo;
                case 2: return &m_chIrsInit;
                case 3: return &m_chWindTemp;
                case 4: return &m_chTropo;    // R5
                case 5: return &m_chGndTemp;  // R6
            }
        }
        return nullptr;
    }

    void buildScreen(ScreenBuffer& buf) override {
        FieldRenderer::text(buf, 0, 10, "INIT", CellColor::WHITE);

        // L1 / R1
        FieldRenderer::text(buf, 1, 1,  "CO RTE",  CellColor::WHITE, 14);
        FieldRenderer::text(buf, 1, 15, "FROM/TO", CellColor::WHITE, 14);
        FieldRenderer::box(buf, 2, 0, 10, m_disp.coRoute, CellColor::CYAN);
        FieldRenderer::slash(buf, 2, 15, 4, 4,
            m_disp.fromAirport, m_disp.toAirport, CellColor::CYAN);

        // L2 / R2 — ALTN/CO RTE (left only or left/right pair)
        FieldRenderer::text(buf, 3, 0, "ALTN/CO RTE", CellColor::WHITE, 14);
        {
            bool leftEmpty = m_disp.altnCoRteLeft.empty();
            bool rightEmpty = m_disp.altnCoRteRight.empty();

            // If only left side is filled (e.g. "NONE"), show it without slash/right
            if (!rightEmpty || leftEmpty) {
                CellColor lc = leftEmpty ? CellColor::WHITE : CellColor::CYAN;
                CellColor rc = rightEmpty ? CellColor::WHITE : CellColor::CYAN;
                CellColor sc = (lc == CellColor::CYAN || rc == CellColor::CYAN) ? CellColor::CYAN : CellColor::WHITE;
                if (leftEmpty)
                    FieldRenderer::dashLine(buf, 4, 0, 4, lc);
                else
                    FieldRenderer::text(buf, 4, 0, padRight(m_disp.altnCoRteLeft, 4), lc);
                FieldRenderer::character(buf, 4, 4, '/', sc);
                if (rightEmpty)
                    FieldRenderer::dashLine(buf, 4, 5, 10, rc);
                else
                    FieldRenderer::text(buf, 4, 5, padRight(m_disp.altnCoRteRight, 10), rc);
            } else {
                // Only left side, no right — show just the left value
                FieldRenderer::text(buf, 4, 0, padRight(m_disp.altnCoRteLeft, 4),
                    CellColor::CYAN);
            }
        }

        // L3 / R3
        FieldRenderer::text(buf, 5, 0,  "FLT NBR",  CellColor::WHITE, 14);
        FieldRenderer::text(buf, 6, 15, "IRS INIT>", CellColor::AMBER);
        FieldRenderer::box(buf, 6, 0, 7, m_disp.fltNbr, CellColor::CYAN);

        // L4 / R4
        FieldRenderer::text(buf, 8, 14, "WIND/TEMP>", CellColor::WHITE);

        // L5 / R5
        {
            bool hasRoute = !m_disp.fromAirport.empty() || !m_disp.toAirport.empty();
            FieldRenderer::text(buf, 9, 0,  "COST INDEX", CellColor::WHITE, 14);
            FieldRenderer::text(buf, 9, 19, "TROPO",      CellColor::WHITE, 14);
            // COST INDEX: WHITE dashes before FROM/TO, AMBER boxes after, CYAN when filled
            if (hasRoute || !m_disp.costIndex.empty())
                FieldRenderer::box(buf, 10, 0, 3, m_disp.costIndex, CellColor::CYAN);
            else
                FieldRenderer::text(buf, 10, 0, "---", CellColor::WHITE);
            FieldRenderer::box(buf, 10, 19, 5, m_disp.tropo, CellColor::CYAN,
                CellColor::CYAN, Align::RIGHT);
        }

        // L6 / R6
        FieldRenderer::text(buf, 11, 0,  "CRZ FL/TEMP", CellColor::WHITE, 14);
        FieldRenderer::text(buf, 11, 16, "GND TEMP",    CellColor::WHITE, 14);

        // CRZ FL/TEMP: WHITE dashes before FROM/TO, AMBER boxes after, CYAN when filled
        {
            bool hasRoute = !m_disp.fromAirport.empty() || !m_disp.toAirport.empty();
            if (m_disp.isPending(ArincLabel::CRZ_FL_TEMP)) {
                FieldRenderer::text(buf, 12, 0, "---- /----°", CellColor::AMBER);
            } else if (hasRoute && m_disp.crzFlTemp.empty()) {
                // AMBER boxes
                FieldRenderer::box(buf, 12, 0, 5, "", CellColor::CYAN, CellColor::AMBER);
                FieldRenderer::character(buf, 12, 6, '/', CellColor::AMBER);
                FieldRenderer::box(buf, 12, 7, 3, "", CellColor::CYAN, CellColor::AMBER);
                FieldRenderer::text(buf, 12, 10, "°", CellColor::AMBER);
            } else if (m_disp.crzFlTemp.empty()) {
                // WHITE dashes before FROM/TO
                FieldRenderer::text(buf, 12, 0,
                    std::string("----- /---°"), CellColor::WHITE);
            } else {
                // CYAN filled data
                FieldRenderer::text(buf, 12, 0, m_disp.crzFlTemp + DEG, CellColor::CYAN);
            }
        }

        // GND TEMP — simple transfer
        {
            CellColor gc = m_disp.gndTemp.empty() ? CellColor::WHITE : CellColor::CYAN;
            std::string gnd = m_disp.gndTemp.empty() ? "---°" : m_disp.gndTemp + "°";
            FieldRenderer::text(buf, 12, 19, gnd, gc, 14);
        }
    }

private:
    McduDisplayState& m_disp;
    const NavDatabase& m_navDb;

    // Combined values for slash field read-back
    std::string m_curFromTo;
    std::string m_curAltn;

    ClickHandler m_chCoRoute;
    ClickHandler m_chAltn;
    ClickHandler m_chFltNbr;
    ClickHandler m_chCostIdx;
    ClickHandler m_chCrzFl;
    ClickHandler m_chFromTo;
    ClickHandler m_chTropo;
    ClickHandler m_chGndTemp;
    ClickHandler m_chIrsInit;
    ClickHandler m_chWindTemp;

    // Right-align: pads spaces on the left
    static std::string padLeft(const std::string& s, int w) {
        if (static_cast<int>(s.size()) >= w) return s.substr(0, static_cast<size_t>(w));
        return std::string(static_cast<size_t>(w - s.size()), ' ') + s;
    }
    // Left-align: pads spaces on the right
    static std::string padRight(const std::string& s, int w) {
        if (static_cast<int>(s.size()) >= w) return s.substr(0, static_cast<size_t>(w));
        std::string r = s;
        r.resize(static_cast<size_t>(w), ' ');
        return r;
    }
};
