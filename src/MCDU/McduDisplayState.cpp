#include "McduDisplayState.h"

void McduDisplayState::applyUpdate(uint16_t label, const BusPayload& payload,
                                   ArincSsm ssm) {
    if (ssm == ArincSsm::NODATA || ssm == ArincSsm::FAIL) {
        switch (label) {
            case ArincLabel::ACK_CO_ROUTE:    m_pending[ArincLabel::CO_ROUTE] = false; coRoute.clear(); break;
            case ArincLabel::ACK_FROM_TO:     m_pending[ArincLabel::FROM_TO] = false; fromAirport.clear(); toAirport.clear(); break;
            case ArincLabel::ACK_FLT_NBR:     m_pending[ArincLabel::FLT_NBR] = false; fltNbr.clear(); break;
            case ArincLabel::ACK_COST_INDEX:  m_pending[ArincLabel::COST_INDEX] = false; costIndex.clear(); break;
            case ArincLabel::ACK_CRZ_FL_TEMP: m_pending[ArincLabel::CRZ_FL_TEMP] = false; crzFlTemp.clear(); break;
            case ArincLabel::ACK_GND_TEMP:    m_pending[ArincLabel::GND_TEMP] = false; gndTemp.clear(); break;
            case ArincLabel::ACK_TROPO:       m_pending[ArincLabel::TROPO] = false; tropo.clear(); break;
            case ArincLabel::ACK_ALTN_ROUTE:  m_pending[ArincLabel::ALTN_ROUTE] = false; altnCoRteLeft.clear(); altnCoRteRight.clear(); break;
            default: break;
        }
        return;
    }
    setValue(label, payload);
}

void McduDisplayState::setValue(uint16_t label, const BusPayload& val) {
    switch (label) {
        case ArincLabel::ACK_CO_ROUTE:
            m_pending[ArincLabel::CO_ROUTE] = false;
            coRoute = (val.type == BusValueType::STRING) ? val.strVal : std::string();
            break;

        case ArincLabel::ACK_FROM_TO: {
            m_pending[ArincLabel::FROM_TO] = false;
            std::string raw = (val.type == BusValueType::STRING) ? val.strVal : std::string();
            auto slash = raw.find('/');
            fromAirport = (slash == std::string::npos) ? raw : raw.substr(0, slash);
            toAirport   = (slash == std::string::npos) ? ""  : raw.substr(slash + 1);
            break;
        }

        case ArincLabel::ACK_FLT_NBR:
            m_pending[ArincLabel::FLT_NBR] = false;
            fltNbr = (val.type == BusValueType::STRING) ? val.strVal : std::string();
            break;

        case ArincLabel::ACK_COST_INDEX:
            m_pending[ArincLabel::COST_INDEX] = false;
            if (val.type == BusValueType::INT && val.intVal >= 0)
                costIndex = std::to_string(val.intVal);
            else
                costIndex.clear();
            break;

        case ArincLabel::ACK_CRZ_FL_TEMP:
            m_pending[ArincLabel::CRZ_FL_TEMP] = false;
            crzFlTemp = (val.type == BusValueType::STRING) ? val.strVal : std::string();
            break;

        case ArincLabel::ACK_GND_TEMP:
            m_pending[ArincLabel::GND_TEMP] = false;
            if (val.type == BusValueType::INT && val.intVal >= -99)
                gndTemp = std::to_string(val.intVal);
            else
                gndTemp.clear();
            break;

        case ArincLabel::ACK_TROPO:
            m_pending[ArincLabel::TROPO] = false;
            if (val.type == BusValueType::INT && val.intVal >= 0)
                tropo = std::to_string(val.intVal);
            else
                tropo.clear();
            break;

        case ArincLabel::ACK_ALTN_ROUTE: {
            m_pending[ArincLabel::ALTN_ROUTE] = false;
            std::string raw = (val.type == BusValueType::STRING) ? val.strVal : std::string();
            auto slash = raw.find('/');
            altnCoRteLeft  = (slash == std::string::npos) ? raw : raw.substr(0, slash);
            altnCoRteRight = (slash == std::string::npos) ? ""  : raw.substr(slash + 1);
            break;
        }

        case ArincLabel::FPLN_STATE:
            if (val.type == BusValueType::FPLN_SNAPSHOT) {
                fplnCache = val.fplnSnapshot.waypoints;
                fplnIsEditing = val.fplnSnapshot.isEditing;
            }
            break;

        default:
            break;
    }
}

void McduDisplayState::clear() {
    coRoute.clear();       m_pending[ArincLabel::CO_ROUTE] = false;
    fromAirport.clear();   m_pending[ArincLabel::FROM_TO] = false;
    toAirport.clear();
    altnCoRteLeft.clear(); m_pending[ArincLabel::ALTN_ROUTE] = false;
    altnCoRteRight.clear();
    fltNbr.clear();        m_pending[ArincLabel::FLT_NBR] = false;
    costIndex.clear();     m_pending[ArincLabel::COST_INDEX] = false;
    crzFlTemp.clear();     m_pending[ArincLabel::CRZ_FL_TEMP] = false;
    gndTemp.clear();       m_pending[ArincLabel::GND_TEMP] = false;
    tropo.clear();         m_pending[ArincLabel::TROPO] = false;
    fplnCache.clear();
    fplnIsEditing = false;
}
