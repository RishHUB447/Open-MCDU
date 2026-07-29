#include "FMGC.h"

// ── ISA helpers ──

double FMGC::isaTempAt(int flightLevel) {
    static constexpr int ISA_FL[] = {0, 50, 100, 150, 200, 250, 300, 350, 360, 400, 450, 500};
    static constexpr double ISA_TEMP[] = {15.0, 5.0, -5.0, -15.0, -25.0, -34.0, -44.0, -54.0, -56.5, -56.5, -56.5, -56.5};
    static constexpr int N = 12;
    if (flightLevel <= ISA_FL[0]) return ISA_TEMP[0];
    if (flightLevel >= ISA_FL[N - 1]) return ISA_TEMP[N - 1];
    for (int i = 0; i < N - 1; i++) {
        if (flightLevel >= ISA_FL[i] && flightLevel <= ISA_FL[i + 1]) {
            double t = double(flightLevel - ISA_FL[i]) / double(ISA_FL[i + 1] - ISA_FL[i]);
            return ISA_TEMP[i] + t * (ISA_TEMP[i + 1] - ISA_TEMP[i]);
        }
    }
    return ISA_TEMP[N - 1];
}

// ── Process incoming messages ──

void FMGC::processMessages() {
    auto msgs = m_bus.pollFmgcInbox();
    for (auto& msg : msgs) {
        if (msg.ssm != ArincSsm::NORMAL) continue;
        switch (msg.label) {
            case ArincLabel::CO_ROUTE:       handleCoRoute(msg.payload); break;
            case ArincLabel::FROM_TO:        handleFromTo(msg.payload); break;
            case ArincLabel::FLT_NBR:        handleFltNbr(msg.payload); break;
            case ArincLabel::COST_INDEX:     handleCostIndex(msg.payload); break;
            case ArincLabel::CRZ_FL_TEMP:    handleCrzFlTemp(msg.payload); break;
            case ArincLabel::GND_TEMP:       handleGndTemp(msg.payload); break;
            case ArincLabel::TROPO:          handleTropo(msg.payload); break;
            case ArincLabel::ALTN_ROUTE:     handleAltnRoute(msg.payload); break;
            case ArincLabel::ALIGN_IRS:      handleAlignIrs(); break;
            case ArincLabel::WAYPOINT_INSERT: handleWaypointInsert(msg.payload); break;
            case ArincLabel::DISCO_REMOVE:   handleDiscoRemove(); break;
            case ArincLabel::FPLN_COMMIT:    handleFplnCommit(); break;
            case ArincLabel::FPLN_CANCEL:    handleFplnCancel(); break;
            default: break;
        }
    }
}

// ── Handlers ──

void FMGC::handleCoRoute(const BusPayload& data) {
    m_coRoute = (data.type == BusValueType::STRING) ? data.strVal : std::string();
    m_bus.sendToMcdu(ArincLabel::ACK_CO_ROUTE, BusPayload(m_coRoute));
}

void FMGC::handleFromTo(const BusPayload& data) {
    if (data.type != BusValueType::STRING) {
        m_bus.sendToMcdu(ArincLabel::ERROR_MSG, BusPayload(std::string("INVALID FORMAT")));
        return;
    }
    auto slash = data.strVal.find('/');
    if (slash == std::string::npos) {
        m_bus.sendToMcdu(ArincLabel::ERROR_MSG, BusPayload(std::string("INVALID FORMAT")));
        return;
    }
    std::string from = data.strVal.substr(0, slash);
    std::string to   = data.strVal.substr(slash + 1);

    if (!m_navDb.find(from) || !m_navDb.find(to)) {
        m_bus.sendToMcdu(ArincLabel::ERROR_MSG, BusPayload(std::string("NOT IN DATABASE")));
        m_bus.sendToMcdu(ArincLabel::ACK_FROM_TO, BusPayload(data.strVal), ArincSsm::NODATA);
        return;
    }

    m_fromAirport = from;
    m_toAirport   = to;

    size_t sz = m_flightPlan.size();
    if (sz == 0) {
        FlightWaypoint f, t;
        f.id = from; t.id = to;
        m_flightPlan.append(f);
        m_flightPlan.insertDiscontinuity();
        m_flightPlan.append(t);
        m_flightPlan.appendEndOfPlan();
    } else {
        int firstValid = -1, lastValid = -1;
        for (int i = 0; i < static_cast<int>(sz); i++) {
            if (!m_flightPlan.at(i).isEndOfPlan && !m_flightPlan.at(i).isDiscontinuity) {
                if (firstValid < 0) firstValid = i;
                lastValid = i;
            }
        }
        if (firstValid >= 0) m_flightPlan.at(firstValid).id = from;
        if (lastValid >= 0) {
            m_flightPlan.at(lastValid).id = to;
            if (m_flightPlan.findDiscontinuity() < 0) {
                FlightWaypoint disco;
                disco.id = "DISCONTINUITY";
                disco.isDiscontinuity = true;
                disco.name = "---F-PLN DISCONTINUITY---";
                m_flightPlan.insertAt(static_cast<size_t>(lastValid), disco);
            }
        }
    }

    if (m_coRoute.empty()) {
        m_coRoute = "NONE";
        m_bus.sendToMcdu(ArincLabel::ACK_CO_ROUTE, BusPayload(std::string("NONE")));
    }
    if (m_altnCoRteLeft.empty() && m_altnCoRteRight.empty()) {
        m_altnCoRteLeft = "NONE";
        m_altnCoRteRight.clear();
        m_bus.sendToMcdu(ArincLabel::ACK_ALTN_ROUTE, BusPayload(std::string("NONE/")));
    }

    sendFplnSnapshot();
    m_bus.sendToMcdu(ArincLabel::ACK_FROM_TO, BusPayload(data.strVal));
}

void FMGC::handleFltNbr(const BusPayload& data) {
    m_fltNbr = (data.type == BusValueType::STRING) ? data.strVal : std::string();
    m_bus.sendToMcdu(ArincLabel::ACK_FLT_NBR, BusPayload(m_fltNbr));
}

void FMGC::handleCostIndex(const BusPayload& data) {
    if (data.type == BusValueType::INT) {
        m_costIndex = data.intVal;
    } else if (data.type == BusValueType::STRING) {
        try { m_costIndex = std::stoi(data.strVal); }
        catch (...) { m_bus.sendToMcdu(ArincLabel::ERROR_MSG, BusPayload(std::string("INVALID FORMAT"))); return; }
    } else {
        m_bus.sendToMcdu(ArincLabel::ERROR_MSG, BusPayload(std::string("INVALID FORMAT")));
        return;
    }
    m_bus.sendToMcdu(ArincLabel::ACK_COST_INDEX, BusPayload(m_costIndex));
}

void FMGC::handleCrzFlTemp(const BusPayload& data) {
    std::string raw;
    if (data.type == BusValueType::STRING) {
        raw = data.strVal;
    } else {
        m_bus.sendToMcdu(ArincLabel::ERROR_MSG, BusPayload(std::string("INVALID FORMAT")));
        return;
    }
    auto slash = raw.find('/');
    std::string flPart = (slash == std::string::npos) ? raw : raw.substr(0, slash);
    std::string tempPart = (slash == std::string::npos) ? "" : raw.substr(slash + 1);

    int fl = parseFlightLevel(flPart);
    if (fl < 0) {
        m_bus.sendToMcdu(ArincLabel::ERROR_MSG, BusPayload(std::string("INVALID FORMAT")));
        return;
    }
    m_crzFl = fl;

    if (!tempPart.empty()) {
        if (!isValidTempStr(tempPart)) {
            m_bus.sendToMcdu(ArincLabel::ERROR_MSG, BusPayload(std::string("INVALID FORMAT")));
            return;
        }
        m_crzTemp = parseTemp(tempPart);
    } else {
        double isaT = isaTempForFl(fl);
        m_crzTemp = static_cast<int>(isaT + (isaT >= 0 ? 0.5 : -0.5));
    }
    m_bus.sendToMcdu(ArincLabel::ACK_CRZ_FL_TEMP,
        BusPayload(formatFlightLevel(m_crzFl) + "/" + std::to_string(m_crzTemp)));
}

void FMGC::handleGndTemp(const BusPayload& data) {
    if (data.type == BusValueType::INT) {
        m_gndTemp = data.intVal;
    } else if (data.type == BusValueType::STRING) {
        try { m_gndTemp = std::stoi(data.strVal); }
        catch (...) { m_bus.sendToMcdu(ArincLabel::ERROR_MSG, BusPayload(std::string("INVALID FORMAT"))); return; }
    } else {
        m_bus.sendToMcdu(ArincLabel::ERROR_MSG, BusPayload(std::string("INVALID FORMAT")));
        return;
    }
    m_bus.sendToMcdu(ArincLabel::ACK_GND_TEMP, BusPayload(m_gndTemp));
}

void FMGC::handleTropo(const BusPayload& data) {
    if (data.type == BusValueType::INT) {
        m_tropo = data.intVal;
    } else if (data.type == BusValueType::STRING) {
        try { m_tropo = std::stoi(data.strVal); }
        catch (...) { m_bus.sendToMcdu(ArincLabel::ERROR_MSG, BusPayload(std::string("INVALID FORMAT"))); return; }
    } else {
        m_bus.sendToMcdu(ArincLabel::ERROR_MSG, BusPayload(std::string("INVALID FORMAT")));
        return;
    }
    m_bus.sendToMcdu(ArincLabel::ACK_TROPO, BusPayload(m_tropo));
}

void FMGC::handleAltnRoute(const BusPayload& data) {
    std::string raw = (data.type == BusValueType::STRING) ? data.strVal : std::string();
    auto slash = raw.find('/');
    m_altnCoRteLeft  = (slash == std::string::npos) ? raw : raw.substr(0, slash);
    m_altnCoRteRight = (slash == std::string::npos) ? ""  : raw.substr(slash + 1);
    m_bus.sendToMcdu(ArincLabel::ACK_ALTN_ROUTE, BusPayload(raw));
}

void FMGC::handleAlignIrs() {
    m_bus.sendToMcdu(ArincLabel::ERROR_MSG, BusPayload(std::string("IRS ALIGNMENT...")));
}

void FMGC::handleWaypointInsert(const BusPayload& data) {
    std::string wptId = (data.type == BusValueType::STRING) ? data.strVal : std::string();
    if (wptId == "CLR") {
        handleDiscoRemove();
        return;
    }
    if (!m_navDb.find(wptId)) {
        m_bus.sendToMcdu(ArincLabel::ERROR_MSG, BusPayload(std::string("NOT IN DATABASE")));
        return;
    }
    if (!m_flightPlan.isEditing()) m_flightPlan.beginEdit();
    int discoIdx = m_flightPlan.editFindDiscontinuity();
    if (discoIdx < 0) {
        m_bus.sendToMcdu(ArincLabel::ERROR_MSG, BusPayload(std::string("NO DISCONTINUITY")));
        return;
    }
    const WaypointRecord* rec = m_navDb.find(wptId);
    FlightWaypoint wpt;
    wpt.id = rec->id;
    wpt.lat = rec->lat;
    wpt.lon = rec->lon;
    wpt.name = rec->name;
    m_flightPlan.editInsertAt(static_cast<size_t>(discoIdx), wpt);
    sendFplnSnapshot();
}

void FMGC::handleDiscoRemove() {
    int discoIdx = m_flightPlan.isEditing()
        ? m_flightPlan.editFindDiscontinuity()
        : m_flightPlan.findDiscontinuity();
    if (discoIdx < 0) return;
    if (m_flightPlan.isEditing())
        m_flightPlan.editRemoveAt(static_cast<size_t>(discoIdx));
    else
        m_flightPlan.removeAt(static_cast<size_t>(discoIdx));
    sendFplnSnapshot();
}

void FMGC::handleFplnCommit() {
    m_flightPlan.commitEdit();
    sendFplnSnapshot();
}

void FMGC::handleFplnCancel() {
    m_flightPlan.cancelEdit();
    sendFplnSnapshot();
}

void FMGC::sendFplnSnapshot() {
    m_bus.sendToMcdu(ArincLabel::FPLN_STATE, BusPayload(m_flightPlan.toSnapshot()));
}

// ── Helpers ──

std::string FMGC::formatFlightLevel(int fl) {
    if (fl < 10) return "";
    return "FL" + std::to_string(fl);
}

int FMGC::parseFlightLevel(const std::string& input) {
    if (input.empty()) return -1;
    if (input.size() >= 2 && input[0] == 'F' && input[1] == 'L') {
        if (input.size() == 2) return -1;
        for (size_t i = 2; i < input.size(); i++)
            if (input[i] < '0' || input[i] > '9') return -1;
        try { return std::stoi(input.substr(2)); }
        catch (...) { return -1; }
    }
    for (char c : input)
        if (c < '0' || c > '9') return -1;
    try {
        int val = std::stoi(input);
        if (val >= 1000) return val / 100;
        if (val >= 10 && val < 1000) return val;
    } catch (...) {}
    return -1;
}

int FMGC::parseTemp(const std::string& input) {
    if (input.empty()) return -999;
    size_t start = 0;
    int sign = 1;
    if (input[0] == '-') { sign = -1; start = 1; }
    else if (input[0] == '+') { start = 1; }
    else if (input[0] == 'M') { sign = -1; start = 1; }
    try { return sign * std::stoi(input.substr(start)); }
    catch (...) { return -999; }
}

bool FMGC::isValidTempStr(const std::string& input) {
    if (input.empty()) return false;
    size_t start = 0;
    if (input[0] == '-' || input[0] == 'M' || input[0] == '+') start = 1;
    if (start >= input.size()) return false;
    for (size_t i = start; i < input.size(); i++)
        if (input[i] < '0' || input[i] > '9') return false;
    return true;
}

// ── Thread ──

void FMGC::start() {
    if (m_running) return;
    m_running = true;
    m_thread = std::thread(&FMGC::run, this);
}

void FMGC::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_thread.joinable())
        m_thread.join();
}

void FMGC::run() {
    m_tropo = 36090;
    m_gndTemp = 25;
    m_bus.sendToMcdu(ArincLabel::ACK_TROPO, BusPayload(m_tropo));
    m_bus.sendToMcdu(ArincLabel::ACK_GND_TEMP, BusPayload(m_gndTemp));

    while (m_running) {
        processMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
