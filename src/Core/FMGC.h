#pragma once
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include "DataBus.h"
#include "NavDatabase.h"
#include "FlightPlan.h"

/*
   FMGC — Flight Management Guidance Computer.
   Now bus-driven: receives messages from MCDU via DataBus, processes
   asynchronously, and sends responses back.

   Owns all flight data, nav database, and the flight plan.
   MCDU does NOT share memory with FMGC — they communicate via bus messages.
*/
class FMGC {
public:
    explicit FMGC(DataBus& bus) : m_bus(bus) {}

    // ── Nav database (can be called from any thread after load) ──
    int loadXPlaneFix(const std::string& path)   { return m_navDb.loadXPlaneFix(path); }
    int loadXPlaneNav(const std::string& path)    { return m_navDb.loadXPlaneNav(path); }
    int loadAirportsCSV(const std::string& path)  { return m_navDb.loadAirportsCSV(path); }

    const WaypointRecord* findWaypoint(const std::string& id) const { return m_navDb.find(id); }
    bool validateWaypoint(const std::string& id) const               { return m_navDb.find(id) != nullptr; }
    const NavDatabase& navDatabase() const { return m_navDb; }

    // ── Flight plan — shared structure (MCDU reads for FPLN display) ──
    FlightPlan&       flightPlan()       { return m_flightPlan; }
    const FlightPlan& flightPlan() const { return m_flightPlan; }

    // ── Thread management — 20 Hz fixed-rate update loop ──
    void start();
    void stop();
    bool isRunning() const { return m_running; }

    // ── Bus processing — call from FMGC thread at fixed rate ──
    void processMessages();

    // ── ISA atmosphere helpers ──
    static double isaTempAt(int flightLevel) {
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

    static double isaTempForFl(const std::string& formattedFl) {
        if (formattedFl.size() < 3 || formattedFl[0] != 'F' || formattedFl[1] != 'L')
            return isaTempAt(0);
        try {
            return isaTempAt(std::stoi(formattedFl.substr(2)));
        } catch (...) {
            return isaTempAt(0);
        }
    }

private:
    DataBus&       m_bus;
    NavDatabase    m_navDb;
    FlightPlan     m_flightPlan;

    std::thread          m_thread;
    std::atomic<bool>    m_running{false};

    void run();

    // No shared data store — FMGC owns all data privately.
    std::string m_coRoute;
    std::string m_fromAirport;
    std::string m_toAirport;
    std::string m_altnCoRteLeft;
    std::string m_altnCoRteRight;
    std::string m_fltNbr;
    std::string m_tropo;
    std::string m_costIndex;
    std::string m_gndTemp;
    std::string m_crzFlTemp;

    // Internal label handlers
    void handleCoRoute(const std::string& data);
    void handleFromTo(const std::string& data);
    void handleFltNbr(const std::string& data);
    void handleCostIndex(const std::string& data);
    void handleCrzFlTemp(const std::string& data);
    void handleGndTemp(const std::string& data);
    void handleTropo(const std::string& data);
    void handleAltnRoute(const std::string& data);
    void handleAlignIrs();

    // Flight plan edit handlers
    void handleWaypointInsert(const std::string& data);
    void handleDiscoRemove();
    void handleFplnCommit();
    void handleFplnCancel();

    // Helper: format FL from raw altitude
    static std::string formatFlightLevel(const std::string& input);
    static bool        isValidTemp(const std::string& input);
};

// ── Inline implementations ──

inline void FMGC::processMessages() {
    auto msgs = m_bus.pollFmgcInbox();
    for (auto& msg : msgs) {
        if (msg.ssm != ArincSsm::NORMAL) continue;

        switch (msg.label) {
            case ArincLabel::CO_ROUTE:     handleCoRoute(msg.payload); break;
            case ArincLabel::FROM_TO:      handleFromTo(msg.payload); break;
            case ArincLabel::FLT_NBR:      handleFltNbr(msg.payload); break;
            case ArincLabel::COST_INDEX:   handleCostIndex(msg.payload); break;
            case ArincLabel::CRZ_FL_TEMP:  handleCrzFlTemp(msg.payload); break;
            case ArincLabel::GND_TEMP:     handleGndTemp(msg.payload); break;
            case ArincLabel::TROPO:        handleTropo(msg.payload); break;
            case ArincLabel::ALTN_ROUTE:   handleAltnRoute(msg.payload); break;
            case ArincLabel::ALIGN_IRS:    handleAlignIrs(); break;
            case ArincLabel::WAYPOINT_INSERT: handleWaypointInsert(msg.payload); break;
            case ArincLabel::DISCO_REMOVE: handleDiscoRemove(); break;
            case ArincLabel::FPLN_COMMIT:  handleFplnCommit(); break;
            case ArincLabel::FPLN_CANCEL:  handleFplnCancel(); break;
            default: break;
        }
    }
}

// ── Label handlers ──

inline void FMGC::handleCoRoute(const std::string& data) {
    m_coRoute = data;
    m_bus.sendToMcdu(ArincLabel::ACK_CO_ROUTE, data);
}

inline void FMGC::handleFromTo(const std::string& data) {
    auto slash = data.find('/');
    if (slash == std::string::npos) {
        m_bus.sendToMcdu(ArincLabel::ERROR_MSG, "INVALID FORMAT");
        return;
    }
    std::string from = data.substr(0, slash);
    std::string to   = data.substr(slash + 1);

    // Validate against nav database
    if (!m_navDb.find(from) || !m_navDb.find(to)) {
        m_bus.sendToMcdu(ArincLabel::ERROR_MSG, "NOT IN DATABASE");
        m_bus.sendToMcdu(ArincLabel::ACK_FROM_TO, from + "/" + to, ArincSsm::NODATA);
        return;
    }

    m_fromAirport = from;
    m_toAirport   = to;

    // Set route in flight plan
    size_t sz = m_flightPlan.size();
    if (sz == 0 || sz == 1) {
        if (sz == 1) m_flightPlan.clearActive();
        m_flightPlan.append(from);
        m_flightPlan.insertDiscontinuity(m_flightPlan.last());
        m_flightPlan.append(to);
        m_flightPlan.appendEndOfPlan();
    } else {
        FlightWaypoint* first = m_flightPlan.first();
        FlightWaypoint* last  = m_flightPlan.last();
        if (first) first->id = from;
        FlightWaypoint* dest = (last && last->isEndOfPlan) ? last->prev : last;
        if (dest && !dest->isDiscontinuity) dest->id = to;
        if (!m_flightPlan.findDiscontinuity() && dest && !dest->isDiscontinuity) {
            auto* disco = m_flightPlan.insertBefore(dest, "DISCONTINUITY");
            disco->isDiscontinuity = true;
            disco->name = "---F-PLN DISCONTINUITY---";
        }
    }

    // Auto-fill NONE on CO ROUTE and ALTN if empty
    if (m_coRoute.empty()) {
        m_coRoute = "NONE";
        m_bus.sendToMcdu(ArincLabel::ACK_CO_ROUTE, "NONE");
    }
    if (m_altnCoRteLeft.empty() && m_altnCoRteRight.empty()) {
        m_altnCoRteLeft = "NONE";
        m_altnCoRteRight.clear();
        m_bus.sendToMcdu(ArincLabel::ACK_ALTN_ROUTE, "NONE/");
    }

    // Send flight plan state update
    m_bus.sendToMcdu(ArincLabel::FPLN_STATE, "");
    m_bus.sendToMcdu(ArincLabel::ACK_FROM_TO, from + "/" + to);
}

inline void FMGC::handleFltNbr(const std::string& data) {
    m_fltNbr = data;
    m_bus.sendToMcdu(ArincLabel::ACK_FLT_NBR, data);
}

inline void FMGC::handleCostIndex(const std::string& data) {
    m_costIndex = data;
    m_bus.sendToMcdu(ArincLabel::ACK_COST_INDEX, data);
}

inline void FMGC::handleCrzFlTemp(const std::string& data) {
    auto slash = data.find('/');
    std::string flPart = (slash == std::string::npos) ? data : data.substr(0, slash);
    std::string tempPart = (slash == std::string::npos) ? "" : data.substr(slash + 1);

    std::string formattedFl = formatFlightLevel(flPart);
    if (formattedFl.empty()) {
        m_bus.sendToMcdu(ArincLabel::ERROR_MSG, "INVALID FORMAT");
        return;
    }
    if (!tempPart.empty() && !isValidTemp(tempPart)) {
        m_bus.sendToMcdu(ArincLabel::ERROR_MSG, "INVALID FORMAT");
        return;
    }
    if (tempPart.empty()) {
        double isaT = isaTempForFl(formattedFl);
        int tempInt = static_cast<int>(isaT + (isaT >= 0 ? 0.5 : -0.5));
        tempPart = (tempInt >= 0 ? "+" : "") + std::to_string(tempInt);
    }
    m_crzFlTemp = formattedFl + "/" + tempPart;
    m_bus.sendToMcdu(ArincLabel::ACK_CRZ_FL_TEMP, m_crzFlTemp);
}

inline void FMGC::handleGndTemp(const std::string& data) {
    m_gndTemp = data;
    m_bus.sendToMcdu(ArincLabel::ACK_GND_TEMP, data);
}

inline void FMGC::handleTropo(const std::string& data) {
    m_tropo = data;
    m_bus.sendToMcdu(ArincLabel::ACK_TROPO, data);
}

inline void FMGC::handleAltnRoute(const std::string& data) {
    auto slash = data.find('/');
    m_altnCoRteLeft  = (slash == std::string::npos) ? data : data.substr(0, slash);
    m_altnCoRteRight = (slash == std::string::npos) ? ""   : data.substr(slash + 1);
    m_bus.sendToMcdu(ArincLabel::ACK_ALTN_ROUTE, data);
}

inline void FMGC::handleAlignIrs() {
    // Direct action — no data expected. Just acknowledge.
    m_bus.sendToMcdu(ArincLabel::ERROR_MSG, "IRS ALIGNMENT...");
}

inline void FMGC::handleWaypointInsert(const std::string& data) {
    if (data == "CLR") {
        handleDiscoRemove();
        return;
    }
    if (!m_navDb.find(data)) {
        m_bus.sendToMcdu(ArincLabel::ERROR_MSG, "NOT IN DATABASE");
        return;
    }
    if (!m_flightPlan.isEditing()) m_flightPlan.beginEdit();
    FlightWaypoint* disco = m_flightPlan.editFindDiscontinuity();
    if (!disco) {
        m_bus.sendToMcdu(ArincLabel::ERROR_MSG, "NO DISCONTINUITY");
        return;
    }
    const WaypointRecord* rec = m_navDb.find(data);
    m_flightPlan.editInsertBefore(disco, rec->id, rec->lat, rec->lon, rec->name);
    m_bus.sendToMcdu(ArincLabel::FPLN_STATE, "");
}

inline void FMGC::handleDiscoRemove() {
    FlightWaypoint* disco = m_flightPlan.isEditing()
        ? m_flightPlan.editFindDiscontinuity()
        : m_flightPlan.findDiscontinuity();
    if (!disco) return;
    if (m_flightPlan.isEditing()) m_flightPlan.editRemove(disco);
    else m_flightPlan.remove(disco);
    m_bus.sendToMcdu(ArincLabel::FPLN_STATE, "");
}

inline void FMGC::handleFplnCommit() {
    m_flightPlan.commitEdit();
    m_bus.sendToMcdu(ArincLabel::FPLN_STATE, "");
}

inline void FMGC::handleFplnCancel() {
    m_flightPlan.cancelEdit();
    m_bus.sendToMcdu(ArincLabel::FPLN_STATE, "");
}

// ── Helpers ──

inline std::string FMGC::formatFlightLevel(const std::string& input) {
    if (input.empty()) return "";
    if (input.size() >= 2 && input[0] == 'F' && input[1] == 'L') {
        if (input.size() == 2) return "";
        for (size_t i = 2; i < input.size(); i++)
            if (input[i] < '0' || input[i] > '9') return "";
        return input;
    }
    for (char c : input)
        if (c < '0' || c > '9') return "";
    try {
        int val = std::stoi(input);
        if (val >= 1000) return "FL" + std::to_string(val / 100);
        if (val >= 10 && val < 1000) return "FL" + std::to_string(val);
    } catch (...) {}
    return "";
}

inline bool FMGC::isValidTemp(const std::string& input) {
    if (input.empty()) return false;
    size_t start = 0;
    if (input[0] == '-' || input[0] == 'M' || input[0] == '+') start = 1;
    if (start >= input.size()) return false;
    for (size_t i = start; i < input.size(); i++)
        if (input[i] < '0' || input[i] > '9') return false;
    return true;
}

// ── Thread implementation ──

inline void FMGC::start() {
    if (m_running) return;
    m_running = true;
    m_thread = std::thread(&FMGC::run, this);
}

inline void FMGC::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_thread.joinable())
        m_thread.join();
}

inline void FMGC::run() {
    // Send initial defaults on first tick
    m_tropo = "36090";
    m_gndTemp = "25";
    m_bus.sendToMcdu(ArincLabel::ACK_TROPO, m_tropo);
    m_bus.sendToMcdu(ArincLabel::GND_TEMP, m_gndTemp);
   

    while (m_running) {
        processMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20 Hz
    }
}
