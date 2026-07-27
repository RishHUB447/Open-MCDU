#pragma once
#include <string>

// Shared data between FMGC (logic) and MCDU (display).
// Pages reference this single instance.
class FMSDataStore {
public:
    std::string coRoute;
    std::string fromAirport;
    std::string toAirport;
    std::string altnCoRte;
    std::string altnCoRteRight;
    std::string fltNbr;
    std::string tropoStr;
    std::string costIdxStr;
    std::string gndTempStr;
    std::string crzFlTemp;

    int transAlt = 0;
    int v1 = 0, vr = 0, v2 = 0;

    double fuelRemaining = 0;
    double tripFuel = 0;
    double fuelFlow = 0;
};
