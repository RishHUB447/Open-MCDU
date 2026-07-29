#include "DataBus.h"

DataBus::DataBus() {}

void DataBus::subscribe(uint16_t label) {
    std::lock_guard<std::mutex> lk(m_busB.mutex);
    m_busB.subs[label] = {label, BusPayload(), false};
}

void DataBus::sendToFmgc(uint16_t label, const BusPayload& payload, ArincSsm ssm) {
    tx(m_busA, label, payload, ssm);
}

void DataBus::sendToMcdu(uint16_t label, const BusPayload& payload, ArincSsm ssm) {
    tx(m_busB, label, payload, ssm);
}

void DataBus::tx(Bus& bus, uint16_t label, const BusPayload& payload, ArincSsm ssm) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    BusEvent ev;
    ev.label = label;
    ev.ssm = ssm;
    ev.payload = payload;
    ev.timestampMs = now + m_latencyMs;

    std::lock_guard<std::mutex> lk(bus.mutex);
    bus.outbox.push(std::move(ev));
}

std::vector<BusEvent> DataBus::pollFmgcInbox() {
    std::vector<BusEvent> result;
    std::lock_guard<std::mutex> lk(m_busA.mutex);
    while (!m_busA.inbox.empty()) {
        result.push_back(std::move(m_busA.inbox.front()));
        m_busA.inbox.pop();
    }
    return result;
}

std::vector<BusEvent> DataBus::pollEvents() {
    std::lock_guard<std::mutex> lk(m_busB.mutex);

    while (!m_busB.inbox.empty()) {
        BusEvent& ev = m_busB.inbox.front();
        auto it = m_busB.subs.find(ev.label);
        if (it != m_busB.subs.end()) {
            // ERROR_MSG always delivered — even identical text is a new event
            bool changed = (ev.label == ArincLabel::ERROR_MSG);
            if (!changed && ev.payload.type != it->second.lastValue.type)
                changed = true;
            if (!changed && (ev.ssm == ArincSsm::NODATA || ev.ssm == ArincSsm::FAIL))
                changed = true;
            if (!changed) {
                switch (ev.payload.type) {
                    case BusValueType::STRING:
                        changed = (ev.payload.strVal != it->second.lastValue.strVal);
                        break;
                    case BusValueType::INT:
                        changed = (ev.payload.intVal != it->second.lastValue.intVal);
                        break;
                    case BusValueType::DOUBLE:
                        changed = (ev.payload.doubleVal != it->second.lastValue.doubleVal);
                        break;
                    case BusValueType::FPLN_SNAPSHOT:
                        changed = true;
                        break;
                    default:
                        changed = false;
                }
            }
            if (changed || ev.ssm != ArincSsm::NORMAL) {
                it->second.lastValue = ev.payload;
                it->second.dirty = true;
            }
        }
        m_busB.inbox.pop();
    }

    std::vector<BusEvent> result;
    for (auto& [label, sub] : m_busB.subs) {
        if (sub.dirty) {
            BusEvent ev;
            ev.label = sub.label;
            ev.ssm = ArincSsm::NORMAL;
            ev.payload = sub.lastValue;
            ev.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            result.push_back(std::move(ev));
            sub.dirty = false;
        }
    }
    return result;
}

void DataBus::tick(uint64_t nowMs) {
    latch(m_busA, nowMs);
    latch(m_busB, nowMs);
}

void DataBus::latch(Bus& bus, uint64_t nowMs) {
    std::lock_guard<std::mutex> lk(bus.mutex);
    while (!bus.outbox.empty()) {
        BusEvent& ev = bus.outbox.front();
        if (ev.timestampMs > nowMs) break;
        bus.inbox.push(std::move(ev));
        bus.outbox.pop();
    }
}

void DataBus::reset() {
    auto clear = [](Bus& bus) {
        std::lock_guard<std::mutex> lk(bus.mutex);
        while (!bus.outbox.empty()) bus.outbox.pop();
        while (!bus.inbox.empty())  bus.inbox.pop();
        bus.subs.clear();
    };
    clear(m_busA);
    clear(m_busB);
}
