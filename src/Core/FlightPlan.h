#pragma once
#include <string>
#include <vector>
#include <cstddef>

/*
   FlightWaypoint - one element in the flight plan.
   No pointers — stored in vector, accessed by index.
*/
struct FlightWaypoint {
    std::string id;
    double lat = 0.0;
    double lon = 0.0;
    std::string name;
    int altConstraint = 0;
    int speedConstraint = 0;
    bool isDiscontinuity = false;
    bool isEndOfPlan = false;
};

/*
   FlightPlanSnapshot — bus-friendly copy of a flight plan.
   Sent by FMGC on FPLN_STATE bus message.
*/
struct FlightPlanSnapshot {
    std::vector<FlightWaypoint> waypoints;
    bool isEditing = false;
};

/*
   FlightPlan — vector-based flight plan with edit/cancel/commit.
   beginEdit() = 1 vector copy (not N pointer allocations).
   Owned exclusively by FMGC — no mutex needed.
*/
class FlightPlan {
public:
    FlightPlan() = default;

    // ── Active plan ──
    size_t size() const { return m_active.size(); }
    bool empty() const { return m_active.empty(); }

    const FlightWaypoint& at(size_t idx) const { return m_active.at(idx); }
    FlightWaypoint& at(size_t idx) { return m_active.at(idx); }

    bool append(const std::string& id, double lat = 0.0, double lon = 0.0,
                const std::string& name = "") {
        FlightWaypoint w;
        w.id = id;
        w.lat = lat;
        w.lon = lon;
        w.name = name.empty() ? id : name;
        m_active.push_back(std::move(w));
        return true;
    }

    void append(const FlightWaypoint& wpt) { m_active.push_back(wpt); }
    void insertAt(size_t idx, const FlightWaypoint& wpt) {
        m_active.insert(m_active.begin() + static_cast<ptrdiff_t>(idx), wpt);
    }
    void removeAt(size_t idx) {
        m_active.erase(m_active.begin() + static_cast<ptrdiff_t>(idx));
    }
    void clearActive() { m_active.clear(); }

    // Insert a discontinuity after the last non-end non-disco waypoint
    void insertDiscontinuity() {
        // Find the last waypoint that isn't end-of-plan or discontinuity
        int insertAfter = -1;
        for (int i = 0; i < static_cast<int>(m_active.size()); i++)
            if (!m_active[i].isEndOfPlan && !m_active[i].isDiscontinuity)
                insertAfter = i;
        FlightWaypoint disco;
        disco.id = "DISCONTINUITY";
        disco.isDiscontinuity = true;
        disco.name = "---F-PLN DISCONTINUITY---";
        m_active.insert(m_active.begin() + insertAfter + 1, std::move(disco));
    }

    // Returns index of first discontinuity, or -1
    int findDiscontinuity() const {
        for (int i = 0; i < static_cast<int>(m_active.size()); i++)
            if (m_active[i].isDiscontinuity) return i;
        return -1;
    }

    void appendEndOfPlan() {
        FlightWaypoint eop;
        eop.id = "ENDOFFPLN";
        eop.isEndOfPlan = true;
        eop.name = "------END OF F-PLN------";
        m_active.push_back(std::move(eop));
    }

    // ── Edit mode ──
    bool isEditing() const { return m_editing; }

    bool beginEdit() {
        if (m_editing) return false;
        m_edit = m_active;  // 1 allocation — not N
        m_editing = true;
        return true;
    }

    void commitEdit() {
        if (!m_editing) return;
        m_active = std::move(m_edit);
        m_edit.clear();
        m_editing = false;
    }

    void cancelEdit() {
        if (!m_editing) return;
        m_edit.clear();
        m_editing = false;
    }

    // ── Edit plan accessors (only valid when editing) ──
    size_t editSize() const { return m_editing ? m_edit.size() : 0; }

    const FlightWaypoint& editAt(size_t idx) const { return m_edit.at(idx); }
    FlightWaypoint& editAt(size_t idx) { return m_edit.at(idx); }

    void editAppend(const FlightWaypoint& wpt) {
        if (m_editing) m_edit.push_back(wpt);
    }

    void editInsertAt(size_t idx, const FlightWaypoint& wpt) {
        if (m_editing)
            m_edit.insert(m_edit.begin() + static_cast<ptrdiff_t>(idx), wpt);
    }

    void editRemoveAt(size_t idx) {
        if (m_editing)
            m_edit.erase(m_edit.begin() + static_cast<ptrdiff_t>(idx));
    }

    int editFindDiscontinuity() const {
        if (!m_editing) return -1;
        for (int i = 0; i < static_cast<int>(m_edit.size()); i++)
            if (m_edit[i].isDiscontinuity) return i;
        return -1;
    }

    // ── Snapshot for bus transfer ──
    FlightPlanSnapshot toSnapshot() const {
        FlightPlanSnapshot snap;
        snap.isEditing = m_editing;
        snap.waypoints = m_editing ? m_edit : m_active;
        return snap;
    }

private:
    std::vector<FlightWaypoint> m_active;
    std::vector<FlightWaypoint> m_edit;
    bool m_editing = false;
};
