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

        // Calculate which plan item is at this slot
        size_t planIdx;
        if (total < 5) {
            int i = static_cast<int>(lskIdx) - static_cast<int>(m_scrollOffset);
            i %= 5;
            if (i < 0) i += 5;
            if (static_cast<size_t>(i) >= total) return nullptr;
            planIdx = static_cast<size_t>(i);
        } else {
            planIdx = (m_scrollOffset + lskIdx) % total;
        }

        const FlightWaypoint* wpt = editing
            ? walkList(m_plan.editFirst(), planIdx)
            : m_plan.at(planIdx);
        if (!wpt) return nullptr;
        if (wpt->isEndOfPlan) return nullptr;

        // Discontinuity: insert handler (LSK only, not RSK)
        if (wpt->isDiscontinuity)
            return (side == 0) ? &m_discoHandler : nullptr;

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

        // Row 1: headers
        FieldRenderer::text(buf, 1, 1,  "FROM",    CellColor::WHITE, 14);
        FieldRenderer::text(buf, 1, 9,  "TIME",    CellColor::WHITE, 14);
        FieldRenderer::text(buf, 1, 15, "SPD/ALT", CellColor::WHITE, 14);

        // Render visible items at slots 0..4
        if (total < 5) {
            for (size_t i = 0; i < total; i++) {
                int slot = static_cast<int>((m_scrollOffset + i) % 5);
                renderWaypoint(buf, 2 + slot * 2, slot, i, editing);
            }
        } else {
            for (int slot = 0; slot < 5; slot++) {
                size_t idx = (m_scrollOffset + slot) % total;
                renderWaypoint(buf, 2 + slot * 2, slot, idx, editing);
            }
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
        size_t modBase = (total < 5) ? 5 : total;
        // Negate delta: UP arrow (-delta) shows earlier items, DOWN (+delta) shows later
        int offset = static_cast<int>(m_scrollOffset) - delta;
        offset %= static_cast<int>(modBase);
        if (offset < 0) offset += static_cast<int>(modBase);
        m_scrollOffset = static_cast<size_t>(offset);
        return true;
    }

private:
    FlightPlan&      m_plan;
    const NavDatabase& m_navDb;
    DataBus&         m_bus;
    size_t           m_scrollOffset = 0;

    ClickHandler  m_discoHandler;
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
        for (auto* cur = m_plan.last(); cur; cur = cur->prev)
            if (!cur->isEndOfPlan && !cur->isDiscontinuity) return cur;
        for (auto* cur = m_plan.first(); cur; cur = cur->next)
            if (!cur->isEndOfPlan && !cur->isDiscontinuity) return cur;
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
