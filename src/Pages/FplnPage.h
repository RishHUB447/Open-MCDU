#pragma once
#include <string>
#include <vector>
#include "../MCDU/Field.h"
#include "../MCDU/McduDisplayState.h"
#include "../Core/DataBus.h"

/*
   FplnPage - F-PLN page.
   Reads flight plan from McduDisplayState::fplnSnapshot (bus copy, no shared memory).
   Click handlers send bus messages for flight plan edits.
*/
class FplnPage : public Page {
public:
    FplnPage(McduDisplayState& display, DataBus& bus)
        : m_disp(display), m_bus(bus)
    {
        m_discoHandler  = {ArincLabel::WAYPOINT_INSERT, false, nullptr};
        m_eraseHandler  = {ArincLabel::FPLN_CANCEL, true, nullptr};
        m_insertHandler = {ArincLabel::FPLN_COMMIT, true, nullptr};
    }

    const ClickHandler* getClickHandler(int side, int lskIdx) override {
        const auto& cache = m_disp.fplnCache;
        bool editing = m_disp.fplnIsEditing;
        size_t total = cache.size();

        if (lskIdx == 5)
            return editing ? ((side == 0) ? &m_eraseHandler : &m_insertHandler) : nullptr;

        if (total == 0 || lskIdx > 4) return nullptr;

        size_t idx = m_scrollOffset + lskIdx;
        if (total >= 5) idx %= total;
        if (idx >= total) return nullptr;

        const FlightWaypoint& wpt = cache[idx];
        if (wpt.isEndOfPlan) return nullptr;
        if (wpt.isDiscontinuity) return (side == 0) ? &m_discoHandler : nullptr;
        if (idx == 0) return &m_latRevHandler;

        return prepareWaypointHandler(lskIdx, wpt.id);
    }

    void buildScreen(ScreenBuffer& buf) override {
        const auto& cache = m_disp.fplnCache;
        bool editing = m_disp.fplnIsEditing;
        size_t total = cache.size();

        // Row 0: flight number + arrows
        if (!m_disp.fltNbr.empty())
            FieldRenderer::text(buf, 0, 22 - m_disp.fltNbr.length(), m_disp.fltNbr, CellColor::WHITE, 14);
        FieldRenderer::character(buf, 0, 22, 0x2190, CellColor::WHITE, 14);
        FieldRenderer::character(buf, 0, 23, 0x2192, CellColor::WHITE, 14);

        if (total == 0) return;

        // Row 1: headers
        if (m_scrollOffset == 0)
            FieldRenderer::text(buf, 1, 1,  "FROM",    CellColor::WHITE, 14);
        FieldRenderer::text(buf, 1, 9,  "TIME",    CellColor::WHITE, 14);
        FieldRenderer::text(buf, 1, 15, "SPD/ALT", CellColor::WHITE, 14);

        // Waypoints at slots 0..4 → rows 2,4,6,8,10
        for (int slot = 0; slot < 5; slot++) {
            size_t idx = m_scrollOffset + slot;
            if (total >= 5) idx %= total;
            if (idx < total)
                renderWaypoint(buf, 2 + slot * 2, cache[idx], editing);
        }

        // Rows 11-12: DEST or TMPY
        if (editing) {
            FieldRenderer::text(buf, 11, 1,  "TMPY",     CellColor::AMBER, 14);
            FieldRenderer::text(buf, 11, 18, "TMPY",     CellColor::AMBER, 14);
            FieldRenderer::character(buf, 12, 0,  0x2190, CellColor::AMBER);
            FieldRenderer::text(buf, 12, 1,  "ERASE",    CellColor::AMBER);
            FieldRenderer::text(buf, 12, 17, "INSERT*",  CellColor::AMBER);
        } else {
            FieldRenderer::text(buf, 11, 1,  "DEST",  CellColor::WHITE, 14);
            FieldRenderer::text(buf, 11, 9,  "TIME",  CellColor::WHITE, 14);
            FieldRenderer::text(buf, 11, 15, "DIST",  CellColor::WHITE, 14);
            FieldRenderer::text(buf, 11, 20, "EFOB",  CellColor::WHITE, 14);
            const FlightWaypoint* dest = findDestination(cache);
            if (dest) {
                FieldRenderer::text(buf, 12, 1,  dest->id,     CellColor::GREEN);
                FieldRenderer::text(buf, 12, 9,  "----",       CellColor::GREEN);
                FieldRenderer::text(buf, 12, 15, "---/----",   CellColor::GREEN);
            }
        }
    }

    bool needsScrollIndicators() const override { return true; }

    bool onScroll(int delta) override {
        size_t total = m_disp.fplnCache.size();
        if (total == 0) return false;
        int offset = static_cast<int>(m_scrollOffset) - delta;
        int modBase = static_cast<int>(total);
        offset %= modBase;
        if (offset < 0) offset += modBase;
        m_scrollOffset = static_cast<size_t>(offset);
        return true;
    }

private:
    McduDisplayState& m_disp;
    DataBus&          m_bus;
    size_t            m_scrollOffset = 0;

    ClickHandler  m_discoHandler;
    ClickHandler  m_latRevHandler{0, false, nullptr, "LAT_REV"};
    ClickHandler  m_eraseHandler;
    ClickHandler  m_insertHandler;
    std::string   m_wptNames[5];
    ClickHandler  m_wptHandlers[5];

    const ClickHandler* prepareWaypointHandler(int slot, const std::string& name) {
        m_wptNames[slot] = name;
        m_wptHandlers[slot] = {0, false, &m_wptNames[slot]};
        return &m_wptHandlers[slot];
    }

    static const FlightWaypoint* findDestination(const std::vector<FlightWaypoint>& cache) {
        const FlightWaypoint* first = nullptr;
        for (const auto& w : cache)
            if (!w.isEndOfPlan && !w.isDiscontinuity) { first = &w; break; }
        if (!first) return nullptr;

        for (auto it = cache.rbegin(); it != cache.rend(); ++it)
            if (!it->isEndOfPlan && !it->isDiscontinuity) {
                if (&*it == first) return nullptr;
                return &*it;
            }
        return nullptr;
    }

    void renderWaypoint(ScreenBuffer& buf, int dataRow,
                        const FlightWaypoint& wpt, bool editing) {
        CellColor normalCol = editing ? CellColor::YELLOW : CellColor::WHITE;
        CellColor dataCol   = editing ? CellColor::YELLOW : CellColor::GREEN;

        if (wpt.isEndOfPlan)
            FieldRenderer::text(buf, dataRow, 0, "------END OF F-PLN------", normalCol);
        else if (wpt.isDiscontinuity)
            FieldRenderer::text(buf, dataRow, 0, "---F-PLN DISCONTINUITY---", normalCol);
        else {
            FieldRenderer::text(buf, dataRow, 1,  wpt.id,      dataCol);
            FieldRenderer::text(buf, dataRow, 9,  "----",      dataCol);
            FieldRenderer::text(buf, dataRow, 15, "---/----",  dataCol);
        }
    }
};
