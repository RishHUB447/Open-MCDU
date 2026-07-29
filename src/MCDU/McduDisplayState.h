#pragma once
#include <string>
#include <cstdint>
#include <unordered_map>
#include "../Core/DataBus.h"

/*
   McduDisplayState — MCDU's local copy of everything shown on screen.
   Updated when the MCDU polls the FMGC->MCDU bus for response messages.

   Each field has a value string and a pending flag.  The pending flag is
   set when the MCDU sends data to the FMGC and cleared when the FMGC
   acknowledges.

   Pages read from this for rendering, so they don't talk to the FMGC
   at all.
*/
class McduDisplayState {
public:
    // Flat fields (same as old FMSDataStore, now MCDU-owned)
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

    // Pending flags — true while waiting for FMGC ack
    bool coRoutePending       = false;
    bool fromToPending        = false;
    bool fltNbrPending        = false;
    bool costIndexPending     = false;
    bool crzFlTempPending     = false;
    bool gndTempPending       = false;
    bool tropoPending         = false;
    bool altnRoutePending     = false;
    bool fplnPending          = false;  // flight plan state

    // Mark a label as pending (MCDU just sent it).
    void markPending(uint16_t label);

    // Apply a bus response: clear pending, update value.
    void applyUpdate(uint16_t label, const std::string& data, ArincSsm ssm);

    // Get display string: "----" if pending, value otherwise.
    std::string display(const std::string& value, bool pending) const;

    // Clear everything.
    void clear();

private:
    // Internal helper to set value + clear pending by label
    void setValue(uint16_t label, const std::string& val);
};

// ── Inline implementation ──

inline void McduDisplayState::markPending(uint16_t label) {
    switch (label) {
        case ArincLabel::CO_ROUTE:    coRoutePending = true;     break;
        case ArincLabel::FROM_TO:     fromToPending = true;      break;
        case ArincLabel::FLT_NBR:     fltNbrPending = true;      break;
        case ArincLabel::COST_INDEX:  costIndexPending = true;   break;
        case ArincLabel::CRZ_FL_TEMP: crzFlTempPending = true;   break;
        case ArincLabel::GND_TEMP:    gndTempPending = true;     break;
        case ArincLabel::TROPO:       tropoPending = true;       break;
        case ArincLabel::ALTN_ROUTE:  altnRoutePending = true;   break;
        default: break;
    }
}

inline void McduDisplayState::applyUpdate(uint16_t label, const std::string& data,
                                           ArincSsm ssm) {
    if (ssm == ArincSsm::NODATA || ssm == ArincSsm::FAIL) {
        // FMGC rejected or couldn't process — keep pending but clear the data
        switch (label) {
            case ArincLabel::ACK_CO_ROUTE:    coRoutePending = false; coRoute.clear();     break;
            case ArincLabel::ACK_FROM_TO:     fromToPending = false;  fromAirport.clear(); toAirport.clear(); break;
            case ArincLabel::ACK_FLT_NBR:     fltNbrPending = false;  fltNbr.clear();      break;
            case ArincLabel::ACK_COST_INDEX:  costIndexPending = false; costIndex.clear(); break;
            case ArincLabel::ACK_CRZ_FL_TEMP: crzFlTempPending = false; crzFlTemp.clear(); break;
            case ArincLabel::ACK_GND_TEMP:    gndTempPending = false;  gndTemp.clear();    break;
            case ArincLabel::ACK_TROPO:       tropoPending = false;    tropo.clear();      break;
            case ArincLabel::ACK_ALTN_ROUTE:  altnRoutePending = false; altnCoRteLeft.clear(); altnCoRteRight.clear(); break;
            default: break;
        }
        return;
    }

    setValue(label, data);
}

inline void McduDisplayState::setValue(uint16_t label, const std::string& val) {
    switch (label) {
        case ArincLabel::ACK_CO_ROUTE:
            coRoute = val; coRoutePending = false; break;
        case ArincLabel::ACK_FROM_TO: {
            fromToPending = false;
            auto slash = val.find('/');
            fromAirport = (slash == std::string::npos) ? val : val.substr(0, slash);
            toAirport   = (slash == std::string::npos) ? ""  : val.substr(slash + 1);
            break;
        }
        case ArincLabel::ACK_FLT_NBR:
            fltNbr = val; fltNbrPending = false; break;
        case ArincLabel::ACK_COST_INDEX:
            costIndex = val; costIndexPending = false; break;
        case ArincLabel::ACK_CRZ_FL_TEMP:
            crzFlTemp = val; crzFlTempPending = false; break;
        case ArincLabel::ACK_GND_TEMP:
            gndTemp = val; gndTempPending = false; break;
        case ArincLabel::ACK_TROPO:
            tropo = val; tropoPending = false; break;
        case ArincLabel::ACK_ALTN_ROUTE: {
            altnRoutePending = false;
            auto slash = val.find('/');
            altnCoRteLeft  = (slash == std::string::npos) ? val : val.substr(0, slash);
            altnCoRteRight = (slash == std::string::npos) ? ""  : val.substr(slash + 1);
            break;
        }
        default: break;
    }
}

inline std::string McduDisplayState::display(const std::string& value, bool pending) const {
    if (pending) return "----";
    return value;
}

inline void McduDisplayState::clear() {
    coRoute.clear();       coRoutePending = false;
    fromAirport.clear();   fromToPending = false;
    toAirport.clear();
    altnCoRteLeft.clear(); altnRoutePending = false;
    altnCoRteRight.clear();
    fltNbr.clear();        fltNbrPending = false;
    costIndex.clear();     costIndexPending = false;
    crzFlTemp.clear();     crzFlTempPending = false;
    gndTemp.clear();       gndTempPending = false;
    tropo.clear();         tropoPending = false;
    fplnPending = false;
}
