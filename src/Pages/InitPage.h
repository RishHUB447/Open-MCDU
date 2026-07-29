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
        buf.setString(0, 10, "INIT", CellColor::WHITE);

        // L1 / R1: simple transfer — no pending overlay, keep showing current value
        FieldRenderer::render(buf, Field::LABEL_SMALL, 1, 1,  0, 0, "CO RTE",  CellColor::WHITE);
        FieldRenderer::render(buf, Field::LABEL_SMALL, 1, 15, 0, 0, "FROM/TO", CellColor::WHITE);
        FieldRenderer::render(buf, Field::BOX, 2, 0, 10, 0, m_disp.coRoute, CellColor::CYAN);
        FieldRenderer::render(buf, Field::SLASH, 2, 15, 4, 4,
            m_disp.fromAirport, CellColor::CYAN, m_disp.toAirport);

        // L2 / R2
        FieldRenderer::render(buf, Field::LABEL_SMALL, 3, 0, 0, 0, "ALTN/CO RTE", CellColor::WHITE);
        {
            CellColor lc = m_disp.altnCoRteLeft.empty() ? CellColor::WHITE : CellColor::CYAN;
            CellColor rc = m_disp.altnCoRteRight.empty() ? CellColor::WHITE : CellColor::CYAN;
            CellColor sc = (lc == CellColor::CYAN || rc == CellColor::CYAN) ? CellColor::CYAN : CellColor::WHITE;
            if (m_disp.altnCoRteLeft.empty())
                for (int i = 0; i < 4; i++) buf.setCell(4, i, '-', lc);
            else {
                std::string s = m_disp.altnCoRteLeft.substr(0, 4);
                s.resize(4, ' ');
                buf.setString(4, 0, s, lc);
            }
            buf.setCell(4, 4, '/', sc);
            if (m_disp.altnCoRteRight.empty())
                buf.setString(4, 5, std::string(10, '-'), rc);
            else {
                std::string s = m_disp.altnCoRteRight.substr(0, 10);
                s.resize(10, ' ');
                buf.setString(4, 5, s, rc);
            }
        }

        // L3 / R3
        FieldRenderer::render(buf, Field::LABEL_SMALL, 5, 0, 0, 0, "FLT NBR",  CellColor::WHITE);
        buf.setString(6, 15, "IRS INIT>", CellColor::AMBER);
        FieldRenderer::render(buf, Field::BOX, 6, 0, 7, 0, m_disp.fltNbr, CellColor::CYAN);

        // L4 / R4
        buf.setString(8, 14, "WIND/TEMP>", CellColor::WHITE);

        // L5 / R5: COST INDEX (simple transfer), TROPO (simple transfer)
        FieldRenderer::render(buf, Field::LABEL_SMALL, 9, 0,  0, 0, "COST INDEX", CellColor::WHITE);
        FieldRenderer::render(buf, Field::LABEL_SMALL, 9, 19, 0, 0, "TROPO",      CellColor::WHITE);
        FieldRenderer::render(buf, Field::BOX, 10, 0, 3, 0, m_disp.costIndex, CellColor::CYAN);
        FieldRenderer::render(buf, Field::BOX, 10, 19, 5, 0,
            m_disp.tropo, CellColor::CYAN, "", Align::RIGHT, CellColor::CYAN);

        // L6 / R6: CRZ FL/TEMP (COMPUTED — shows ---- while FMGC calculates),
        //          GND TEMP (simple transfer)
        FieldRenderer::render(buf, Field::LABEL_SMALL, 11, 0,  0, 0, "CRZ FL/TEMP", CellColor::WHITE);
        FieldRenderer::render(buf, Field::LABEL_SMALL, 11, 16, 0, 0, "GND TEMP",    CellColor::WHITE);

        // CRZ FL/TEMP — FMGC computes ISA temperature, show ---- while computing
        {
            CellColor cc = m_disp.crzFlTempPending ? CellColor::AMBER :
                (m_disp.crzFlTemp.empty() ? CellColor::AMBER : CellColor::CYAN);
            std::string crz = m_disp.crzFlTempPending ? "----/----" :
                (m_disp.crzFlTemp.empty() ? "-----/---" : m_disp.crzFlTemp);
            buf.setString(12, 0, crz + DEG, cc);
        }

        // GND TEMP — simple transfer, no pending overlay
        {
            CellColor gc = m_disp.gndTemp.empty() ? CellColor::WHITE : CellColor::CYAN;
            std::string gnd = m_disp.gndTemp.empty() ? "---" : m_disp.gndTemp;
            buf.setString(12, 20, padLeft(gnd + DEG, 4), gc, 14);
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

    static std::string padLeft(const std::string& s, int w) {
        if (static_cast<int>(s.size()) >= w) return s.substr(0, static_cast<size_t>(w));
        return std::string(static_cast<size_t>(w - s.size()), ' ') + s;
    }
};
