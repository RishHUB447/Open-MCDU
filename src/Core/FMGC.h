#pragma once
#include <string>
#include <vector>
#include "FMSDataStore.h"
#include "NavDatabase.h"
#include "FlightPlan.h"

/*
   FMGC - Flight Management Guidance Computer.
   Owns all flight data and navigation state. MCDU reads/writes through this.
*/
class FMGC {
public:
    FMSDataStore& dataStore() { return m_data; }
    const FMSDataStore& dataStore() const { return m_data; }

    int loadXPlaneFix(const std::string& path) { return m_navDb.loadXPlaneFix(path); }
    int loadXPlaneNav(const std::string& path) { return m_navDb.loadXPlaneNav(path); }
    int loadAirportsCSV(const std::string& path) { return m_navDb.loadAirportsCSV(path); }

    void loadNavDatabase(const std::string& path) { m_navDb.loadCSV(path); }
    const WaypointRecord* findWaypoint(const std::string& id) const { return m_navDb.find(id); }
    bool validateWaypoint(const std::string& id) const { return m_navDb.find(id) != nullptr; }
    std::vector<const WaypointRecord*> searchWaypoints(const std::string& prefix, int max = 10) const {
        return m_navDb.searchByPrefix(prefix, max);
    }
    const NavDatabase& navDatabase() const { return m_navDb; }

    /* ISA standard atmosphere: linear interpolation from table.
       Below FL360: lapse ~1.98C per 1000ft. At/above: constant -56.5C. */
    static double isaTempAt(int flightLevel) {
        static constexpr int ISA_FL[] = {0, 50, 100, 150, 200, 250, 300, 350, 360, 400, 450, 500};
        static constexpr double ISA_TEMP[] = {15.0, 5.0, -5.0, -15.0, -25.0, -34.0, -44.0, -54.0, -56.5, -56.5, -56.5, -56.5};
        static constexpr int N = 12;

        if (flightLevel <= ISA_FL[0]) return ISA_TEMP[0];
        if (flightLevel >= ISA_FL[N - 1]) return ISA_TEMP[N - 1];

        for (int i = 0; i < N - 1; i++) {
            if (flightLevel >= ISA_FL[i] && flightLevel <= ISA_FL[i + 1]) {
                double t = static_cast<double>(flightLevel - ISA_FL[i])
                          / static_cast<double>(ISA_FL[i + 1] - ISA_FL[i]);
                return ISA_TEMP[i] + t * (ISA_TEMP[i + 1] - ISA_TEMP[i]);
            }
        }
        return ISA_TEMP[N - 1];
    }

    static double isaTempForFl(const std::string& formattedFl) {
        if (formattedFl.size() < 3 || formattedFl[0] != 'F' || formattedFl[1] != 'L')
            return isaTempAt(0);
        try {
            int fl = std::stoi(formattedFl.substr(2));
            return isaTempAt(fl);
        } catch (...) {
            return isaTempAt(0);
        }
    }

    bool validateCrzFlTemp(const std::string& fl, const std::string& temp) const {
        (void)fl; (void)temp;
        return true;
    }

    bool validateRoute(const std::string& from, const std::string& to) const {
        if (from.empty() || to.empty()) return false;
        return m_navDb.find(from) != nullptr && m_navDb.find(to) != nullptr;
    }

    /* Set departure/destination route.
       If plan empty: creates dep -> DISCO -> dest -> END.
       If plan exists: updates endpoints, keeps intermediate waypoints. */
    void setRoute(const std::string& departure, const std::string& destination) {
        size_t sz = m_flightPlan.size();
        if (sz == 0 || sz == 1) {
            if (sz == 1) m_flightPlan.clearActive();
            m_flightPlan.append(departure);
            m_flightPlan.insertDiscontinuity(m_flightPlan.last());
            m_flightPlan.append(destination);
            m_flightPlan.appendEndOfPlan();
        } else {
            FlightWaypoint* first = m_flightPlan.first();
            FlightWaypoint* last  = m_flightPlan.last();
            if (first) first->id = departure;

            FlightWaypoint* dest = (last && last->isEndOfPlan) ? last->prev : last;
            if (dest && !dest->isDiscontinuity)
                dest->id = destination;

            if (!m_flightPlan.findDiscontinuity() && dest && !dest->isDiscontinuity) {
                auto* disco = m_flightPlan.insertBefore(dest, "DISCONTINUITY");
                disco->isDiscontinuity = true;
                disco->name = "---F-PLN DISCONTINUITY---";
            }
        }
    }

    FlightPlan& flightPlan() { return m_flightPlan; }
    const FlightPlan& flightPlan() const { return m_flightPlan; }

private:
    FMSDataStore m_data;
    NavDatabase m_navDb;
    FlightPlan m_flightPlan;
};
