#pragma once
#include <string>
#include <vector>
#include <cstddef>
#include <mutex>
#include "NavDatabase.h"

/*
   FlightWaypoint - one node in the FlightPlan linked list.
   Doubly-linked for O(1) insert/remove. Markers for DISCONTINUITY and END OF F-PLN.
*/
struct FlightWaypoint {
    std::string id;
    double lat = 0.0;
    double lon = 0.0;
    std::string name;

    FlightWaypoint* prev = nullptr;
    FlightWaypoint* next = nullptr;

    int altConstraint = 0;
    int speedConstraint = 0;
    bool isDiscontinuity = false;
    bool isEndOfPlan = false;
};

/*
   FlightPlan - doubly-linked list with EDIT mode.
   beginEdit() deep-copies active -> edit plan. commitEdit() swaps, cancelEdit() discards.
   Move-construct/assign transfers pointers; no deep copy unless beginEdit() called.
*/
class FlightPlan {
public:
    FlightPlan() = default;

    ~FlightPlan() { std::lock_guard<std::recursive_mutex> lk(m_mutex); clearActive(); clearEdit(); }

    FlightPlan(const FlightPlan&) = delete;
    FlightPlan& operator=(const FlightPlan&) = delete;

    FlightPlan(FlightPlan&& other) noexcept
        : m_activeHead(other.m_activeHead), m_activeTail(other.m_activeTail),
          m_activeSize(other.m_activeSize),
          m_editHead(other.m_editHead), m_editTail(other.m_editTail),
          m_editSize(other.m_editSize), m_editing(other.m_editing)
    {
        other.m_activeHead = nullptr; other.m_activeTail = nullptr;
        other.m_activeSize = 0;
        other.m_editHead = nullptr; other.m_editTail = nullptr;
        other.m_editSize = 0; other.m_editing = false;
    }

    FlightPlan& operator=(FlightPlan&& other) noexcept {
        if (this == &other) return *this;
        clearActive(); clearEdit();
        m_activeHead = other.m_activeHead; m_activeTail = other.m_activeTail;
        m_activeSize = other.m_activeSize;
        m_editHead = other.m_editHead; m_editTail = other.m_editTail;
        m_editSize = other.m_editSize; m_editing = other.m_editing;
        other.m_activeHead = nullptr; other.m_activeTail = nullptr;
        other.m_activeSize = 0;
        other.m_editHead = nullptr; other.m_editTail = nullptr;
        other.m_editSize = 0; other.m_editing = false;
        return *this;
    }

    // Active plan accessors
    FlightWaypoint* first() const { std::lock_guard<std::recursive_mutex> lk(m_mutex); return m_activeHead; }
    FlightWaypoint* last()  const { std::lock_guard<std::recursive_mutex> lk(m_mutex); return m_activeTail; }
    size_t size() const { std::lock_guard<std::recursive_mutex> lk(m_mutex); return m_activeSize; }

    FlightWaypoint* at(size_t index) const {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        FlightWaypoint* cur = m_activeHead;
        for (size_t i = 0; cur && i < index; i++)
            cur = cur->next;
        return cur;
    }

    std::vector<const FlightWaypoint*> toVector() const {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        std::vector<const FlightWaypoint*> result;
        result.reserve(m_activeSize);
        for (auto* cur = m_activeHead; cur; cur = cur->next)
            result.push_back(cur);
        return result;
    }

    // Active plan mutations
    bool appendFromDB(const std::string& id, const NavDatabase& db) {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        const WaypointRecord* rec = db.find(id);
        if (!rec) return false;
        return append(rec->id, rec->lat, rec->lon, rec->name);
    }

    bool append(const std::string& id, double lat = 0.0, double lon = 0.0,
                const std::string& name = "") {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        auto* node = new FlightWaypoint;
        node->id = id;
        node->lat = lat;
        node->lon = lon;
        node->name = name.empty() ? id : name;
        appendNode(node);
        return true;
    }

    void insertDiscontinuity(FlightWaypoint* after = nullptr) {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        auto* node = new FlightWaypoint;
        node->id = "DISCONTINUITY";
        node->isDiscontinuity = true;
        node->name = "---F-PLN DISCONTINUITY---";
        insertAfter(node, after);
    }

    FlightWaypoint* findDiscontinuity() {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        for (auto* cur = m_activeHead; cur; cur = cur->next)
            if (cur->isDiscontinuity) return cur;
        return nullptr;
    }

    FlightWaypoint* insertBefore(FlightWaypoint* before, const std::string& id,
                                  double lat = 0.0, double lon = 0.0,
                                  const std::string& name = "") {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        auto* node = new FlightWaypoint;
        node->id = id;
        node->lat = lat;
        node->lon = lon;
        node->name = name.empty() ? id : name;

        node->prev = before ? before->prev : nullptr;
        node->next = before;

        if (before) {
            if (before->prev) before->prev->next = node;
            else m_activeHead = node;
            before->prev = node;
        } else {
            if (m_activeTail) {
                m_activeTail->next = node;
                node->prev = m_activeTail;
            } else {
                m_activeHead = node;
            }
            m_activeTail = node;
        }
        m_activeSize++;
        return node;
    }

    FlightWaypoint* prepend(const std::string& id, double lat = 0.0, double lon = 0.0,
                             const std::string& name = "") {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        return insertBefore(m_activeHead, id, lat, lon, name);
    }

    void appendEndOfPlan() {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        auto* node = new FlightWaypoint;
        node->id = "ENDOFFPLN";
        node->isEndOfPlan = true;
        node->name = "------END OF F-PLN------";
        appendNode(node);
    }

    void remove(FlightWaypoint* node) {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        if (!node) return;
        unlinkNode(node);
        delete node;
    }

    void clearActive() {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        FlightWaypoint* cur = m_activeHead;
        while (cur) {
            FlightWaypoint* nxt = cur->next;
            delete cur;
            cur = nxt;
        }
        m_activeHead = nullptr; m_activeTail = nullptr; m_activeSize = 0;
    }

    // EDIT mode
    bool isEditing() const { std::lock_guard<std::recursive_mutex> lk(m_mutex); return m_editing; }

    bool beginEdit() {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        if (m_editing) return false;
        m_editHead = deepCopyList(m_activeHead, m_activeTail, m_editSize);
        m_editing = true;
        return true;
    }

    void commitEdit() {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        if (!m_editing || !m_editHead) return;
        clearActive();
        m_activeHead = m_editHead; m_activeTail = m_editTail;
        m_activeSize = m_editSize;
        m_editHead = nullptr; m_editTail = nullptr; m_editSize = 0;
        m_editing = false;
    }

    void cancelEdit() {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        if (!m_editing) return;
        clearEdit();
        m_editing = false;
    }

    // Edit plan accessors
    FlightWaypoint* editFirst() const { std::lock_guard<std::recursive_mutex> lk(m_mutex); return m_editing ? m_editHead : nullptr; }
    FlightWaypoint* editLast()  const { std::lock_guard<std::recursive_mutex> lk(m_mutex); return m_editing ? m_editTail : nullptr; }
    size_t editSize() const { std::lock_guard<std::recursive_mutex> lk(m_mutex); return m_editing ? m_editSize : 0; }

    bool editAppendFromDB(const std::string& id, const NavDatabase& db) {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        if (!m_editing) return false;
        const WaypointRecord* rec = db.find(id);
        if (!rec) return false;
        return editAppendRaw(rec->id, rec->lat, rec->lon, rec->name);
    }

    bool editAppendRaw(const std::string& id, double lat = 0.0, double lon = 0.0,
                       const std::string& name = "") {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        if (!m_editing) return false;
        auto* node = new FlightWaypoint;
        node->id = id;
        node->lat = lat;
        node->lon = lon;
        node->name = name.empty() ? id : name;

        node->prev = m_editTail;
        node->next = nullptr;
        if (m_editTail) m_editTail->next = node;
        else m_editHead = node;
        m_editTail = node;
        m_editSize++;
        return true;
    }

    FlightWaypoint* editFindDiscontinuity() {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        if (!m_editing) return nullptr;
        for (auto* cur = m_editHead; cur; cur = cur->next)
            if (cur->isDiscontinuity) return cur;
        return nullptr;
    }

    FlightWaypoint* editInsertBefore(FlightWaypoint* before, const std::string& id,
                                      double lat = 0.0, double lon = 0.0,
                                      const std::string& name = "") {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        if (!m_editing) return nullptr;
        auto* node = new FlightWaypoint;
        node->id = id;
        node->lat = lat;
        node->lon = lon;
        node->name = name.empty() ? id : name;

        node->prev = before ? before->prev : nullptr;
        node->next = before;

        if (before) {
            if (before->prev) before->prev->next = node;
            else m_editHead = node;
            before->prev = node;
        } else {
            if (m_editTail) {
                m_editTail->next = node;
                node->prev = m_editTail;
            } else {
                m_editHead = node;
            }
            m_editTail = node;
        }
        m_editSize++;
        return node;
    }

    std::vector<const FlightWaypoint*> editToVector() const {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        std::vector<const FlightWaypoint*> result;
        if (!m_editing) return result;
        result.reserve(m_editSize);
        for (auto* cur = m_editHead; cur; cur = cur->next)
            result.push_back(cur);
        return result;
    }

    void editRemove(FlightWaypoint* node) {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        if (!m_editing || !node) return;
        if (node->prev) node->prev->next = node->next;
        else m_editHead = node->next;
        if (node->next) node->next->prev = node->prev;
        else m_editTail = node->prev;
        delete node;
        m_editSize--;
    }

    void clearEdit() {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        FlightWaypoint* cur = m_editHead;
        while (cur) {
            FlightWaypoint* nxt = cur->next;
            delete cur;
            cur = nxt;
        }
        m_editHead = nullptr; m_editTail = nullptr; m_editSize = 0;
    }

private:
    void appendNode(FlightWaypoint* node) {
        node->prev = m_activeTail;
        node->next = nullptr;
        if (m_activeTail) m_activeTail->next = node;
        else m_activeHead = node;
        m_activeTail = node;
        m_activeSize++;
    }

    void insertAfter(FlightWaypoint* node, FlightWaypoint* after) {
        node->prev = after;
        node->next = after ? after->next : m_activeHead;
        if (after) {
            if (after->next) after->next->prev = node;
            else m_activeTail = node;
            after->next = node;
        } else {
            if (m_activeHead) m_activeHead->prev = node;
            m_activeHead = node;
            if (!m_activeTail) m_activeTail = node;
        }
        m_activeSize++;
    }

    void unlinkNode(FlightWaypoint* node) {
        if (node->prev) node->prev->next = node->next;
        else m_activeHead = node->next;
        if (node->next) node->next->prev = node->prev;
        else m_activeTail = node->prev;
        m_activeSize--;
    }

    static FlightWaypoint* deepCopyList(FlightWaypoint* srcHead,
                                         FlightWaypoint*& outTail,
                                         size_t& outSize) {
        if (!srcHead) { outTail = nullptr; outSize = 0; return nullptr; }
        FlightWaypoint* newHead = new FlightWaypoint(*srcHead);
        newHead->prev = nullptr;
        FlightWaypoint* cur = newHead;
        FlightWaypoint* src = srcHead->next;
        size_t count = 1;
        while (src) {
            cur->next = new FlightWaypoint(*src);
            cur->next->prev = cur;
            cur = cur->next;
            src = src->next;
            count++;
        }
        outTail = cur;
        outSize = count;
        return newHead;
    }

    mutable std::recursive_mutex m_mutex;
    FlightWaypoint* m_activeHead = nullptr;
    FlightWaypoint* m_activeTail = nullptr;
    size_t m_activeSize = 0;

    FlightWaypoint* m_editHead = nullptr;
    FlightWaypoint* m_editTail = nullptr;
    size_t m_editSize = 0;
    bool m_editing = false;
};
