#pragma once
#include <string>
#include <cstdint>
#include <unordered_map>
#include "../Core/DataBus.h"

/*
   McduDisplayState — MCDU's local copy of everything shown on screen.
   Updated via event-driven bus polling (pollEvents).
   Pending state is stored in a map, flight plan in fplnCache.
*/
class McduDisplayState {
public:
    std::string coRoute;
    std::string fromAirport;
    std::string toAirport;
    std::string altnCoRteLeft;
    std::string altnCoRteRight;
    std::string fltNbr;
    std::string costIndex;
    std::string crzFlTemp;
    std::string gndTemp;
    std::string tropo;

    std::vector<FlightWaypoint> fplnCache;
    bool fplnIsEditing = false;

    void markPending(uint16_t label);
    bool isPending(uint16_t label) const;
    void applyUpdate(uint16_t label, const BusPayload& payload, ArincSsm ssm);
    std::string display(const std::string& value, bool pending) const;
    void clear();

private:
    std::unordered_map<uint16_t, bool> m_pending;
    void setValue(uint16_t label, const BusPayload& val);
};

// Small helpers — keep inline in header for performance
inline void McduDisplayState::markPending(uint16_t label) { m_pending[label] = true; }
inline bool McduDisplayState::isPending(uint16_t label) const {
    auto it = m_pending.find(label);
    return it != m_pending.end() && it->second;
}
inline std::string McduDisplayState::display(const std::string& value, bool pending) const {
    return pending ? "----" : value;
}
