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
   Bus-driven: receives typed messages from MCDU, sends typed responses back.
   Owns FlightPlan privately — MCDU receives FlightPlanSnapshot via bus.
   Implementation in FMGC.cpp.
*/
class FMGC {
public:
    explicit FMGC(DataBus& bus) : m_bus(bus) {}

    int loadXPlaneFix(const std::string& path)   { return m_navDb.loadXPlaneFix(path); }
    int loadXPlaneNav(const std::string& path)    { return m_navDb.loadXPlaneNav(path); }
    int loadAirportsCSV(const std::string& path)  { return m_navDb.loadAirportsCSV(path); }

    const WaypointRecord* findWaypoint(const std::string& id) const { return m_navDb.find(id); }
    const NavDatabase& navDatabase() const { return m_navDb; }

    void start();
    void stop();
    bool isRunning() const { return m_running; }
    void processMessages();

    static double isaTempAt(int flightLevel);      // defined in FMGC.cpp
    static double isaTempForFl(int flightLevel) { return isaTempAt(flightLevel); }

private:
    DataBus&       m_bus;
    NavDatabase    m_navDb;
    FlightPlan     m_flightPlan;

    std::thread          m_thread;
    std::atomic<bool>    m_running{false};

    void run();
    void sendFplnSnapshot();

    // Identifiers (strings for ICAO codes, route names)
    std::string m_coRoute;
    std::string m_fromAirport;
    std::string m_toAirport;
    std::string m_altnCoRteLeft;
    std::string m_altnCoRteRight;
    std::string m_fltNbr;

    // Numeric values (properly typed)
    int m_costIndex = -1, m_gndTemp = -1, m_tropo = -1;
    int m_crzFl = -1, m_crzTemp = -999;

    // Handlers (defined in FMGC.cpp)
    void handleCoRoute(const BusPayload& data);
    void handleFromTo(const BusPayload& data);
    void handleFltNbr(const BusPayload& data);
    void handleCostIndex(const BusPayload& data);
    void handleCrzFlTemp(const BusPayload& data);
    void handleGndTemp(const BusPayload& data);
    void handleTropo(const BusPayload& data);
    void handleAltnRoute(const BusPayload& data);
    void handleAlignIrs();
    void handleWaypointInsert(const BusPayload& data);
    void handleDiscoRemove();
    void handleFplnCommit();
    void handleFplnCancel();

    static std::string formatFlightLevel(int fl);
    static int         parseFlightLevel(const std::string& input);
    static int         parseTemp(const std::string& input);
    static bool        isValidTempStr(const std::string& input);
};
