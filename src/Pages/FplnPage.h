#pragma once
#include <string>
#include <vector>
#include "../MCDU/Field.h"
#include "../Core/FMSDataStore.h"
#include "../Core/FlightPlan.h"
#include "../Core/NavDatabase.h"

/*
   FplnPage - F-PLN page of the MCDU.
   Renders up to 5 waypoint rows (slot 0..4 = rows 2,4,6,8,10).
   getClickHandler(side, idx) returns the handler for whatever item
   is currently at that slot based on scrollOffset.
*/

struct DiscoInsertCtx {
    FlightPlan* plan;
    const NavDatabase* navDb;
};

static bool insertAtDiscontinuity(void* ctx, const std::string& input, std::string& err) {
    auto* c = static_cast<DiscoInsertCtx*>(ctx);
    if (input == "CLR") {
        FlightWaypoint* disco = c->plan->isEditing()
            ? c->plan->editFindDiscontinuity()
            : c->plan->findDiscontinuity();
        if (!disco) { err = "NO DISCONTINUITY"; return false; }
        if (c->plan->isEditing()) c->plan->editRemove(disco);
        else c->plan->remove(disco);
        return true;
    }
    if (!c->navDb->find(input)) { err = "NOT IN DATABASE"; return false; }
    if (!c->plan->isEditing()) c->plan->beginEdit();
    FlightWaypoint* disco = c->plan->editFindDiscontinuity();
    if (!disco) { err = "NO DISCONTINUITY"; return false; }
    const WaypointRecord* rec = c->navDb->find(input);
    c->plan->editInsertBefore(disco, rec->id, rec->lat, rec->lon, rec->name);
    return true;
}

struct EditCtx { FlightPlan* plan; };

static bool eraseEdit(void* ctx, const std::string& input, std::string& err) {
    (void)input; (void)err;
    static_cast<EditCtx*>(ctx)->plan->cancelEdit();
    return true;
}

static bool commitEdit(void* ctx, const std::string& input, std::string& err) {
    (void)input; (void)err;
    static_cast<EditCtx*>(ctx)->plan->commitEdit();
    return true;
}

class FplnPage : public Page {
public:
    FplnPage(FMSDataStore& store, FlightPlan& plan, const NavDatabase& navDb)
        : m_store(store), m_plan(plan)
        , m_discoCtx{&plan, &navDb}, m_editCtx{&plan}
        , m_discoHandler{nullptr, nullptr, insertAtDiscontinuity, &m_discoCtx}
        , m_eraseHandler{nullptr, nullptr, eraseEdit, &m_editCtx, true}
        , m_insertHandler{nullptr, nullptr, commitEdit, &m_editCtx, true}
    {}

    // Returns the click handler for the item currently at (side, lskIdx).
    // Calculation depends on scrollOffset and plan structure.
    const ClickHandler* getClickHandler(int side, int lskIdx) override {
        bool editing = m_plan.isEditing();
        size_t total = editing ? m_plan.editSize() : m_plan.size();

        // LSK6 (idx=5): TMPY controls in edit mode, nothing otherwise
        if (lskIdx == 5) {
            if (editing) {
                return (side == 0) ? &m_eraseHandler : &m_insertHandler;
            }
            return nullptr;
        }

        if (total == 0 || lskIdx > 4) return nullptr;

        // Calculate which plan item is at this slot
        size_t planIdx;
        if (total < 5) {
            // In <5 mode: item i at slot (offset + i) % 5
            // Given slot, find i: (offset + i) % 5 = slot
            int i = static_cast<int>(lskIdx) - static_cast<int>(m_scrollOffset);
            i %= 5;
            if (i < 0) i += 5;
            if (static_cast<size_t>(i) >= total) return nullptr;
            planIdx = static_cast<size_t>(i);
        } else {
            // >=5 mode: slot 0..4 maps to items (offset + slot) % total
            planIdx = (m_scrollOffset + lskIdx) % total;
        }

        // Get the plan node at that index
        const FlightWaypoint* wpt = editing
            ? walkList(m_plan.editFirst(), planIdx)
            : m_plan.at(planIdx);
        if (!wpt) return nullptr;

        // End of plan: no click handler
        if (wpt->isEndOfPlan) return nullptr;

        // Discontinuity: insert handler (only LSK, not RSK)
        if (wpt->isDiscontinuity) {
            return (side == 0) ? &m_discoHandler : nullptr;
        }

        // Normal waypoint: storeString handler
        return prepareWaypointHandler(lskIdx, wpt->id);
    }

    void buildScreen(ScreenBuffer& buf) override {
        bool editing = m_plan.isEditing();

        // Row 0: flight number + arrows
        if (!m_store.fltNbr.empty())
            buf.setString(0, 15, m_store.fltNbr, CellColor::WHITE, 14);
        buf.setCell(0, 22, 0x2190, CellColor::WHITE);
        buf.setCell(0, 23, 0x2192, CellColor::WHITE);

        size_t total = editing ? m_plan.editSize() : m_plan.size();
        if (total == 0) return;

        // Row 1: headers
        if (m_scrollOffset == 0)
            buf.setString(1, 1, "FROM", CellColor::WHITE, 14);
        buf.setString(1, 9,  "TIME",   CellColor::WHITE, 14);
        buf.setString(1, 15, "SPD/ALT", CellColor::WHITE, 14);

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
            buf.setString(11, 1,  "TMPY",  CellColor::AMBER, 14);
            buf.setString(11, 18, "TMPY",  CellColor::AMBER, 14);
            buf.setCell(12, 1,  0x2190, CellColor::AMBER);
            buf.setString(12, 2,  "ERASE", CellColor::AMBER);
            buf.setString(12, 17, "INSERT*", CellColor::AMBER);
        } else {
            buf.setString(11, 1,  "DEST", CellColor::WHITE, 14);
            buf.setString(11, 9,  "TIME", CellColor::WHITE, 14);
            buf.setString(11, 15, "DIST", CellColor::WHITE, 14);
            buf.setString(11, 20, "EFOB", CellColor::WHITE, 14);
            const FlightWaypoint* dest = findDestination();
            if (dest) {
                buf.setString(12, 1,  dest->id, CellColor::GREEN);
                buf.setString(12, 9,  "----",     CellColor::GREEN);
                buf.setString(12, 15, "---/-----", CellColor::GREEN);
            }
        }
    }

    bool needsScrollIndicators() const override { return true; }

    bool onScroll(int delta) override {
        size_t total = m_plan.isEditing() ? m_plan.editSize() : m_plan.size();
        if (total == 0) return false;
        size_t modBase = (total < 5) ? 5 : total;
        int offset = static_cast<int>(m_scrollOffset) + delta;
        offset %= static_cast<int>(modBase);
        if (offset < 0) offset += static_cast<int>(modBase);
        m_scrollOffset = static_cast<size_t>(offset);
        return true;
    }

private:
    FMSDataStore& m_store;
    FlightPlan& m_plan;
    DiscoInsertCtx m_discoCtx;
    EditCtx m_editCtx;
    size_t m_scrollOffset = 0;

    // Reusable ClickHandlers. m_wptHandlers[n] is filled per-slot in prepareWaypointHandler().
    ClickHandler m_discoHandler;
    ClickHandler m_eraseHandler;
    ClickHandler m_insertHandler;
    std::string m_wptNames[5];
    ClickHandler m_wptHandlers[5];

    // Prepares a ClickHandler for a normal waypoint at given slot.
    const ClickHandler* prepareWaypointHandler(int slot, const std::string& name) {
        m_wptNames[slot] = name;
        m_wptHandlers[slot] = {&m_wptNames[slot], nullptr, storeString, &m_wptNames[slot]};
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
        CellColor dataCol  = editing ? CellColor::YELLOW : CellColor::GREEN;

        if (wpt->isEndOfPlan) {
            buf.setString(dataRow, 0, "------END OF F-PLN------", normalCol);
        } else if (wpt->isDiscontinuity) {
            buf.setString(dataRow, 0, "---F-PLN DISCONTINUITY---", normalCol);
        } else {
            buf.setString(dataRow, 1,  wpt->id, dataCol);
            buf.setString(dataRow, 9,  "----",     dataCol);
            buf.setString(dataRow, 15, "---/-----", dataCol);
        }
    }

    static const FlightWaypoint* walkList(const FlightWaypoint* head, size_t idx) {
        const FlightWaypoint* cur = head;
        for (size_t i = 0; cur && i < idx; i++) cur = cur->next;
        return cur;
    }
};
