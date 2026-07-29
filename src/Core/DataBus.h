#pragma once
#include <string>
#include <vector>
#include <queue>
#include <cstdint>
#include <mutex>
#include <chrono>
#include <unordered_map>

// ARINC 429-style labels (octal) for MCDU <-> FMGC communication.
namespace ArincLabel {
    // MCDU -> FMGC (labels 000-077)
    constexpr uint16_t CO_ROUTE      = 006;
    constexpr uint16_t FROM_TO       = 010;  // "FROM/TO" pair
    constexpr uint16_t FLT_NBR       = 012;
    constexpr uint16_t COST_INDEX    = 014;
    constexpr uint16_t CRZ_FL_TEMP   = 015;
    constexpr uint16_t GND_TEMP      = 016;
    constexpr uint16_t TROPO         = 017;
    constexpr uint16_t ALTN_ROUTE    = 020;  // "ALTN/CO RTE" pair
    constexpr uint16_t ALIGN_IRS     = 026;  // direct action, no data
    constexpr uint16_t WIND_TEMP     = 027;  // goes to WIND/TEMP page later

    // Flight plan edits
    constexpr uint16_t WAYPOINT_INSERT = 031;
    constexpr uint16_t DISCO_REMOVE    = 032;
    constexpr uint16_t FPLN_COMMIT     = 033;
    constexpr uint16_t FPLN_CANCEL     = 034;

    // FMGC -> MCDU (labels 100-177) — responses / state updates
    constexpr uint16_t ACK_CO_ROUTE     = 100;
    constexpr uint16_t ACK_FROM_TO      = 101;
    constexpr uint16_t ACK_FLT_NBR      = 102;
    constexpr uint16_t ACK_COST_INDEX   = 103;
    constexpr uint16_t ACK_CRZ_FL_TEMP  = 104;
    constexpr uint16_t ACK_GND_TEMP     = 105;
    constexpr uint16_t ACK_TROPO        = 106;
    constexpr uint16_t FPLN_STATE       = 107;  // full FP update
    constexpr uint16_t ACK_ALTN_ROUTE   = 110;
    constexpr uint16_t ERROR_MSG        = 177;  // error text
}

// SSM — Sign/Status Matrix (real ARINC 429 uses 2 bits).
// Normal = valid data, NoData = initialising / pending, Fail = invalid.
enum class ArincSsm : uint8_t {
    FAIL   = 0,
    NODATA = 1,
    TEST   = 2,
    NORMAL = 3
};

// One word on the bus.
struct BusWord {
    uint16_t    label;      // ARINC octal label
    ArincSsm    ssm;        // data validity
    uint8_t     sdi;        // source/destination identifier (unused, keep 0)
    std::string payload;    // the actual data string
    uint64_t    timestampMs; // when it was transmitted
};

/*
   DataBus — two unidirectional ARINC 429 buses between MCDU and FMGC.
     Bus A (MCDU -> FMGC): MCDU transmits, FMGC polls
     Bus B (FMGC -> MCDU): FMGC transmits, MCDU polls

   Each label is transmitted at a configurable rate (Hz).  tick() advances
   the schedule.  The default rate is 20 Hz per label.

   Thread-safe: each bus uses its own mutex so MCDU and FMGC can run
   on separate threads.
*/
class DataBus {
public:
    DataBus();

    // ── Transmit (call from the sender side) ──

    // Send a word on Bus A (MCDU -> FMGC).
    void sendToFmgc(uint16_t label, const std::string& payload,
                    ArincSsm ssm = ArincSsm::NORMAL);

    // Send a word on Bus B (FMGC -> MCDU).
    void sendToMcdu(uint16_t label, const std::string& payload,
                    ArincSsm ssm = ArincSsm::NORMAL);

    // ── Receive (call from the receiver side) ──

    // Poll all pending words on Bus A (MCDU side sends, FMGC side polls).
    std::vector<BusWord> pollFmgcInbox();

    // Poll all pending words on Bus B (FMGC side sends, MCDU side polls).
    std::vector<BusWord> pollMcduInbox();

    // ── Schedule and timing ──

    // Set the transmission rate for a specific label (default 20 Hz).
    void setLabelRate(uint16_t label, int hz);

    // Advance the bus transmission schedule by deltaMs milliseconds.
    // Call this from the main loop (or FMGC tick) at a fixed interval.
    void tick(uint64_t nowMs);

    // Simulated bus latency in milliseconds (default 0).
    void setLatencyMs(int ms) { m_latencyMs = ms; }
    int  latencyMs() const { return m_latencyMs; }

    // Reset both buses (clears all queues).
    void reset();

private:
    struct Bus {
        std::mutex              mutex;
        std::queue<BusWord>     outbox;   // words to be latched (pending)
        std::queue<BusWord>     inbox;    // words already latched (ready to poll)

        // Per-label transmission schedule (ms between transmissions).
        std::unordered_map<uint16_t, int> labelIntervalMs;
        std::unordered_map<uint16_t, uint64_t> lastTxMs;

        BusWord lru; // "last received word" — not used yet, for future BITE
    };

    Bus m_busA;  // MCDU -> FMGC
    Bus m_busB;  // FMGC -> MCDU

    int  m_latencyMs = 0;
    bool m_initialised = false;

    // Transmit a word on a specific bus (internal, mutex must NOT be held).
    void tx(Bus& bus, uint16_t label, const std::string& payload, ArincSsm ssm);

    // Latch pending words from outbox to inbox after latency expires.
    void latch(Bus& bus, uint64_t nowMs);
};

// ── Inline helpers ──

inline DataBus::DataBus() {
    // Default rates: 20 Hz for all labels
    auto setDefault = [this](const uint16_t* labels, size_t n) {
        for (size_t i = 0; i < n; i++) {
            m_busA.labelIntervalMs[labels[i]] = 50;
            m_busB.labelIntervalMs[labels[i]] = 50;
        }
    };

    const uint16_t mcduLabels[] = {
        ArincLabel::CO_ROUTE, ArincLabel::FROM_TO, ArincLabel::FLT_NBR,
        ArincLabel::COST_INDEX, ArincLabel::CRZ_FL_TEMP, ArincLabel::GND_TEMP,
        ArincLabel::TROPO, ArincLabel::ALTN_ROUTE, ArincLabel::ALIGN_IRS,
        ArincLabel::WAYPOINT_INSERT, ArincLabel::DISCO_REMOVE,
        ArincLabel::FPLN_COMMIT, ArincLabel::FPLN_CANCEL
    };
    const uint16_t fmgcLabels[] = {
        ArincLabel::ACK_CO_ROUTE, ArincLabel::ACK_FROM_TO,
        ArincLabel::ACK_FLT_NBR, ArincLabel::ACK_COST_INDEX,
        ArincLabel::ACK_CRZ_FL_TEMP, ArincLabel::ACK_GND_TEMP,
        ArincLabel::ACK_TROPO, ArincLabel::FPLN_STATE,
        ArincLabel::ACK_ALTN_ROUTE, ArincLabel::ERROR_MSG
    };

    setDefault(mcduLabels, sizeof(mcduLabels) / sizeof(mcduLabels[0]));
    setDefault(fmgcLabels, sizeof(fmgcLabels) / sizeof(fmgcLabels[0]));
    m_initialised = true;
}

inline void DataBus::setLabelRate(uint16_t label, int hz) {
    int ms = (hz > 0) ? (1000 / hz) : 500;
    {
        std::lock_guard<std::mutex> lk(m_busA.mutex);
        m_busA.labelIntervalMs[label] = ms;
    }
    {
        std::lock_guard<std::mutex> lk(m_busB.mutex);
        m_busB.labelIntervalMs[label] = ms;
    }
}

inline void DataBus::sendToFmgc(uint16_t label, const std::string& payload,
                                 ArincSsm ssm) {
    tx(m_busA, label, payload, ssm);
}

inline void DataBus::sendToMcdu(uint16_t label, const std::string& payload,
                                 ArincSsm ssm) {
    tx(m_busB, label, payload, ssm);
}

inline void DataBus::tx(Bus& bus, uint16_t label, const std::string& payload,
                         ArincSsm ssm) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    BusWord w;
    w.label = label;
    w.ssm = ssm;
    w.sdi = 0;
    w.payload = payload;
    w.timestampMs = now + m_latencyMs;  // + simulated bus latency

    std::lock_guard<std::mutex> lk(bus.mutex);
    bus.outbox.push(std::move(w));
}

inline std::vector<BusWord> DataBus::pollFmgcInbox() {
    std::vector<BusWord> result;
    std::lock_guard<std::mutex> lk(m_busA.mutex);
    while (!m_busA.inbox.empty()) {
        result.push_back(std::move(m_busA.inbox.front()));
        m_busA.inbox.pop();
    }
    return result;
}

inline std::vector<BusWord> DataBus::pollMcduInbox() {
    std::vector<BusWord> result;
    std::lock_guard<std::mutex> lk(m_busB.mutex);
    while (!m_busB.inbox.empty()) {
        result.push_back(std::move(m_busB.inbox.front()));
        m_busB.inbox.pop();
    }
    return result;
}

inline void DataBus::tick(uint64_t nowMs) {
    latch(m_busA, nowMs);
    latch(m_busB, nowMs);
}

inline void DataBus::latch(Bus& bus, uint64_t nowMs) {
    std::lock_guard<std::mutex> lk(bus.mutex);
    while (!bus.outbox.empty()) {
        BusWord& w = bus.outbox.front();
        if (w.timestampMs > nowMs) break;  // not yet transmitted
        bus.inbox.push(std::move(w));
        bus.outbox.pop();
    }
}

inline void DataBus::reset() {
    auto clear = [](Bus& bus) {
        std::lock_guard<std::mutex> lk(bus.mutex);
        while (!bus.outbox.empty()) bus.outbox.pop();
        while (!bus.inbox.empty())  bus.inbox.pop();
    };
    clear(m_busA);
    clear(m_busB);
}
