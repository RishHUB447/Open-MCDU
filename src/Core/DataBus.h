#pragma once
#include <string>
#include <vector>
#include <queue>
#include <cstdint>
#include <mutex>
#include <chrono>
#include <unordered_map>
#include "FlightPlan.h"

// ARINC 429-style octal labels for MCDU <-> FMGC communication.
namespace ArincLabel {
    // MCDU -> FMGC (labels 000-077)
    constexpr uint16_t CO_ROUTE      = 006;
    constexpr uint16_t FROM_TO       = 010;
    constexpr uint16_t FLT_NBR       = 012;
    constexpr uint16_t COST_INDEX    = 014;
    constexpr uint16_t CRZ_FL_TEMP   = 015;
    constexpr uint16_t GND_TEMP      = 016;
    constexpr uint16_t TROPO         = 017;
    constexpr uint16_t ALTN_ROUTE    = 020;
    constexpr uint16_t ALIGN_IRS     = 026;
    constexpr uint16_t WIND_TEMP     = 027;
    // Flight plan edits
    constexpr uint16_t WAYPOINT_INSERT = 031;
    constexpr uint16_t DISCO_REMOVE    = 032;
    constexpr uint16_t FPLN_COMMIT     = 033;
    constexpr uint16_t FPLN_CANCEL     = 034;
    // FMGC -> MCDU (labels 100-177)
    constexpr uint16_t ACK_CO_ROUTE     = 100;
    constexpr uint16_t ACK_FROM_TO      = 101;
    constexpr uint16_t ACK_FLT_NBR      = 102;
    constexpr uint16_t ACK_COST_INDEX   = 103;
    constexpr uint16_t ACK_CRZ_FL_TEMP  = 104;
    constexpr uint16_t ACK_GND_TEMP     = 105;
    constexpr uint16_t ACK_TROPO        = 106;
    constexpr uint16_t FPLN_STATE       = 107;
    constexpr uint16_t ACK_ALTN_ROUTE   = 110;
    constexpr uint16_t ERROR_MSG        = 177;
}

enum class ArincSsm : uint8_t {
    FAIL   = 0,
    NODATA = 1,
    TEST   = 2,
    NORMAL = 3
};

enum class BusValueType : uint8_t { NONE, STRING, INT, DOUBLE, FPLN_SNAPSHOT };

struct BusPayload {
    BusValueType type = BusValueType::NONE;
    std::string strVal;
    int intVal = 0;
    double doubleVal = 0.0;
    FlightPlanSnapshot fplnSnapshot;

    BusPayload() = default;
    explicit BusPayload(const std::string& s) : type(BusValueType::STRING), strVal(s) {}
    explicit BusPayload(int i) : type(BusValueType::INT), intVal(i) {}
    explicit BusPayload(double d) : type(BusValueType::DOUBLE), doubleVal(d) {}
    explicit BusPayload(const FlightPlanSnapshot& fp) : type(BusValueType::FPLN_SNAPSHOT), fplnSnapshot(fp) {}
    explicit BusPayload(FlightPlanSnapshot&& fp) : type(BusValueType::FPLN_SNAPSHOT), fplnSnapshot(std::move(fp)) {}
};

struct BusEvent {
    uint16_t    label;
    ArincSsm    ssm;
    BusPayload  payload;
    uint64_t    timestampMs;
};

/*
   DataBus — event-driven subscription bus between MCDU and FMGC.
   Bus A (MCDU->FMGC): MCDU sends, FMGC polls pollFmgcInbox().
   Bus B (FMGC->MCDU): FMGC sends. MCDU subscribes and polls pollEvents()
     which returns only changed subscribed labels.
   Thread-safe via mutexes.
   Implementation in DataBus.cpp.
*/
class DataBus {
public:
    DataBus();
    void subscribe(uint16_t label);
    void sendToFmgc(uint16_t label, const BusPayload& payload, ArincSsm ssm = ArincSsm::NORMAL);
    void sendToMcdu(uint16_t label, const BusPayload& payload, ArincSsm ssm = ArincSsm::NORMAL);
    std::vector<BusEvent> pollFmgcInbox();
    std::vector<BusEvent> pollEvents();
    void setLatencyMs(int ms) { m_latencyMs = ms; }
    int  latencyMs() const { return m_latencyMs; }
    void tick(uint64_t nowMs);
    void reset();

private:
    struct Sub { uint16_t label; BusPayload lastValue; bool dirty = false; };
    struct Bus {
        std::mutex mutex;
        std::queue<BusEvent> outbox;
        std::queue<BusEvent> inbox;
        std::unordered_map<uint16_t, Sub> subs;
    };
    Bus m_busA, m_busB;
    int m_latencyMs = 0;
    void tx(Bus& bus, uint16_t label, const BusPayload& payload, ArincSsm ssm);
    void latch(Bus& bus, uint64_t nowMs);
};
