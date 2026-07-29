#pragma once
#include <string>
#include <vector>
#include "../MCDU/Field.h"
#include "../Core/FlightPlan.h"
#include "../Core/NavDatabase.h"
#include "../Core/DataBus.h"

/*
   FplnPage - F-PLN page.
   Renders up to 5 waypoint rows (slot 0..4 = rows 2,4,6,8,10).
   Click handlers send bus messages for flight plan edits:
     WAYPOINT_INSERT  -> insert waypoint before discontinuity
     FPLN_COMMIT      -> commit temp flight plan
     FPLN_CANCEL      -> cancel temp flight plan
*/
class FplnPage : public Page {
public:
    FplnPage(FlightPlan& plan, const NavDatabase& navDb, DataBus& bus)
        : m_plan(plan), m_navDb(navDb), m_bus(bus)
    {
        // DISCO handler: has bus label, no read-back (scratchpad data is waypoint ID)
        m_discoHandler  = {ArincLabel::WAYPOINT_INSERT, false, nullptr};
        // Direct action handlers:
        m_eraseHandler  = {ArincLabel::FPLN_CANCEL, true, nullptr};
        m_insertHandler = {ArincLabel::FPLN_COMMIT, true, nullptr};
    }

    const ClickHandler* getClickHandler(int side, int lskIdx) override {
        bool editing = m_plan.isEditing();
        size_t total = editing ? m_plan.editSize() : m_plan.size();

        // LSK6 (idx=5): TMPY controls in edit mode, nothing otherwise
        if (lskIdx == 5) {
            if (editing)
                return (side == 0) ? &m_eraseHandler : &m_insertHandler;
            return nullptr;
        }

        if (total == 0 || lskIdx > 4) return nullptr;

        // Unified: scrollOffset = which plan item is at slot 0
        size_t planIdx = m_scrollOffset + lskIdx;
        if (total >= 5)
            planIdx %= total;
        if (planIdx >= total) return nullptr;

        const FlightWaypoint* wpt = editing
            ? walkList(m_plan.editFirst(), planIdx)
            : m_plan.at(planIdx);
        if (!wpt) return nullptr;
        if (wpt->isEndOfPlan) return nullptr;

        // Discontinuity: insert handler (LSK only, not RSK)
        if (wpt->isDiscontinuity)
            return (side == 0) ? &m_discoHandler : nullptr;

        // First waypoint (departure) -> open LAT REV page
        if (planIdx == 0)
            return &m_latRevHandler;

        // Normal waypoint: read-back only (no bus label)
        return prepareWaypointHandler(lskIdx, wpt->id);
    }

    void buildScreen(ScreenBuffer& buf) override {
        bool editing = m_plan.isEditing();

        // Row 0: flight number + arrows
        FieldRenderer::character(buf, 0, 22, 0x2190, CellColor::WHITE, 14);
        FieldRenderer::character(buf, 0, 23, 0x2192, CellColor::WHITE, 14);

        size_t total = editing ? m_plan.editSize() : m_plan.size();
        if (total == 0) return;

        // Row 1: headers — FROM only when departure airport is at LSK1 (slot 0)
        if (m_scrollOffset == 0)
            FieldRenderer::text(buf, 1, 1,  "FROM",    CellColor::WHITE, 14);
        FieldRenderer::text(buf, 1, 9,  "TIME",    CellColor::WHITE, 14);
        FieldRenderer::text(buf, 1, 15, "SPD/ALT", CellColor::WHITE, 14);

        // Unified scroll: scrollOffset = which plan item is at slot 0
        // For total >= 5: circular modulo (wraps around)
        // For total < 5: linear (items shift within viewport, no wrap)
        for (int slot = 0; slot < 5; slot++) {
            size_t idx = m_scrollOffset + slot;
            if (total >= 5)
                idx %= total;
            if (idx < total)
                renderWaypoint(buf, 2 + slot * 2, slot, idx, editing);
        }

        // Rows 11-12: DEST or TMPY/ERASE/INSERT
        if (editing) {
            FieldRenderer::text(buf, 11, 1,  "TMPY",    CellColor::AMBER, 14);
            FieldRenderer::text(buf, 11, 18, "TMPY",    CellColor::AMBER, 14);
            FieldRenderer::character(buf, 12, 0,  0x2190, CellColor::AMBER);
            FieldRenderer::text(buf, 12, 1,  "ERASE",   CellColor::AMBER);
            FieldRenderer::text(buf, 12, 17, "INSERT*", CellColor::AMBER);
        } else {
            FieldRenderer::text(buf, 11, 1,  "DEST",     CellColor::WHITE, 14);
            FieldRenderer::text(buf, 11, 9,  "TIME",     CellColor::WHITE, 14);
            FieldRenderer::text(buf, 11, 15, "DIST",     CellColor::WHITE, 14);
            FieldRenderer::text(buf, 11, 20, "EFOB",     CellColor::WHITE, 14);
            const FlightWaypoint* dest = findDestination();
            if (dest) {
                FieldRenderer::text(buf, 12, 1,  dest->id, CellColor::GREEN);
                FieldRenderer::text(buf, 12, 9,  "----",    CellColor::GREEN);
                FieldRenderer::text(buf, 12, 15, "---/----", CellColor::GREEN);
            }
        }
    }

    bool needsScrollIndicators() const override { return true; }

    bool onScroll(int delta) override {
        size_t total = m_plan.isEditing() ? m_plan.editSize() : m_plan.size();
        if (total == 0) return false;
        // delta=+1 (UP), delta=-1 (DOWN)
        // -delta: UP -> offset-1 (earlier items), DOWN -> offset+1 (later items)
        int offset = static_cast<int>(m_scrollOffset) - delta;
        int modBase = static_cast<int>(total);
        offset %= modBase;
        if (offset < 0) offset += modBase;
        m_scrollOffset = static_cast<size_t>(offset);
        return true;
    }

private:
    FlightPlan&      m_plan;
    const NavDatabase& m_navDb;
    DataBus&         m_bus;
    size_t           m_scrollOffset = 0;

    ClickHandler  m_discoHandler;
    ClickHandler  m_latRevHandler{0, false, nullptr, "LAT_REV"};
    ClickHandler  m_eraseHandler;
    ClickHandler  m_insertHandler;
    std::string   m_wptNames[5];
    ClickHandler  m_wptHandlers[5];

    // Prepares a read-back ClickHandler for a waypoint at given slot.
    const ClickHandler* prepareWaypointHandler(int slot, const std::string& name) {
        m_wptNames[slot] = name;
        // busLabel=0, isDirectAction=false, valuePtr set -> read-back only
        m_wptHandlers[slot] = {0, false, &m_wptNames[slot]};
        return &m_wptHandlers[slot];
    }

    const FlightWaypoint* findDestination() const {
        // Find first valid waypoint (departure)
        const FlightWaypoint* first = nullptr;
        for (auto* cur = m_plan.first(); cur; cur = cur->next)
            if (!cur->isEndOfPlan && !cur->isDiscontinuity) { first = cur; break; }
        if (!first) return nullptr;

        // Find last valid waypoint going backwards
        for (auto* cur = m_plan.last(); cur; cur = cur->prev)
            if (!cur->isEndOfPlan && !cur->isDiscontinuity) {
                // If same as departure, there's no real destination
                return (cur == first) ? nullptr : cur;
            }
        return nullptr;
    }

    void renderWaypoint(ScreenBuffer& buf, int dataRow, int slot, size_t planIdx, bool editing) {
        size_t limit = editing ? m_plan.editSize() : m_plan.size();
        if (planIdx >= limit) return;
        const FlightWaypoint* wpt = editing
            ? walkList(m_plan.editFirst(), planIdx)
            : m_plan.at(planIdx);
        if (!wpt) return;

        CellColor normalCol = editing ? CellColor::YELLOW : CellColor::WHITE;
        CellColor dataCol   = editing ? CellColor::YELLOW : CellColor::GREEN;

        if (wpt->isEndOfPlan) {
            FieldRenderer::text(buf, dataRow, 0, "------END OF F-PLN------", normalCol);
        } else if (wpt->isDiscontinuity) {
            FieldRenderer::text(buf, dataRow, 0, "---F-PLN DISCONTINUITY---", normalCol);
        } else {
            FieldRenderer::text(buf, dataRow, 1,  wpt->id,   dataCol);
            FieldRenderer::text(buf, dataRow, 9,  "----",    dataCol);
            FieldRenderer::text(buf, dataRow, 15, "---/----", dataCol);
        }
    }

    static const FlightWaypoint* walkList(const FlightWaypoint* head, size_t idx) {
        const FlightWaypoint* cur = head;
        for (size_t i = 0; cur && i < idx; i++) cur = cur->next;
        return cur;
    }
};
