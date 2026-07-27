#pragma once
#include <string>
#include "../MCDU/Field.h"
#include "../Core/FMGC.h"

static bool storeSlash(void* ctx, const std::string& input, std::string& err) {
    auto* t = static_cast<SlashTarget*>(ctx);
    auto slash = input.find('/');
    if (slash == std::string::npos) { err = "INVALID FORMAT"; return false; }
    *t->left = input.substr(0, slash);
    *t->right = input.substr(slash + 1);
    return true;
}

struct FromToStoreCtx {
    SlashTarget* target;
    FMGC* fmgc;
    std::string* coRoute;
    std::string* altnLeft;
    std::string* altnRight;
};

static bool storeSlashValidated(void* ctx, const std::string& input, std::string& err) {
    auto* c = static_cast<FromToStoreCtx*>(ctx);
    auto slash = input.find('/');
    if (slash == std::string::npos) { err = "INVALID FORMAT"; return false; }
    std::string from = input.substr(0, slash);
    std::string to = input.substr(slash + 1);
    if (!c->fmgc->validateRoute(from, to)) { err = "NOT IN DATABASE"; return false; }
    c->fmgc->setRoute(from, to);
    *c->target->left = from;
    *c->target->right = to;
    if (c->coRoute && c->coRoute->empty()) *c->coRoute = "NONE";
    if (c->altnLeft && c->altnLeft->empty()) *c->altnLeft = "NONE";
    if (c->altnRight && c->altnRight->empty()) *c->altnRight = "NONE";
    return true;
}

static std::string formatFlightLevel(const std::string& input) {
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

static bool isValidTemp(const std::string& input) {
    if (input.empty()) return false;
    size_t start = 0;
    if (input[0] == '-' || input[0] == 'M' || input[0] == '+') start = 1;
    if (start >= input.size()) return false;
    for (size_t i = start; i < input.size(); i++)
        if (input[i] < '0' || input[i] > '9') return false;
    return true;
}

struct CrzFlStoreCtx {
    std::string* out;
    FMGC* fmgc;
};

static bool storeCrzFlTemp(void* ctx, const std::string& input, std::string& err) {
    auto* c = static_cast<CrzFlStoreCtx*>(ctx);
    auto slash = input.find('/');
    std::string flPart, tempPart;
    if (slash == std::string::npos) { flPart = input; tempPart = ""; }
    else { flPart = input.substr(0, slash); tempPart = input.substr(slash + 1); }
    std::string formattedFl = formatFlightLevel(flPart);
    if (formattedFl.empty()) { err = "INVALID FORMAT"; return false; }
    if (!tempPart.empty()) {
        if (!isValidTemp(tempPart)) { err = "INVALID FORMAT"; return false; }
        if (c->fmgc && !c->fmgc->validateCrzFlTemp(formattedFl, tempPart)) { err = "INVALID FORMAT"; return false; }
    }
    if (tempPart.empty()) {
        double isaT = FMGC::isaTempForFl(formattedFl);
        int tempInt = static_cast<int>(isaT + (isaT >= 0 ? 0.5 : -0.5));
        tempPart = (tempInt >= 0 ? "+" : "") + std::to_string(tempInt);
    }
    *c->out = formattedFl + "/" + tempPart;
    return true;
}

/*
   InitPage layout:
     L1: CO RTE  [10]            R1: FROM/TO  [4]/[4]
     L2: ALTN/CO RTE  [4]/[10]   R2: (empty)
     L3: FLT NBR  [7]            R3: ALIGN IRS> (AMBER)
     L4: (empty)                 R4: (empty)
     L5: COST INDEX  [3]         R5: GND TEMP --- (WHITE)
     L6: CRZ FL/TEMP  [9]        R6: TROPO [5] (SMALL)
*/
class InitPage : public Page {
public:
    InitPage(FMSDataStore& store, FMGC& fmgc)
        : m_store(store)
        , m_fromToCtx{&m_fromTo, &fmgc, &store.coRoute, &store.altnCoRte, &store.altnCoRteRight}
        , m_crzCtx{&m_store.crzFlTemp, &fmgc}
    {
        m_fromTo.left       = &m_store.fromAirport;
        m_fromTo.right      = &m_store.toAirport;
        m_altnCoRte.left    = &m_store.altnCoRte;
        m_altnCoRte.right   = &m_store.altnCoRteRight;

        m_chCoRoute  = {&m_store.coRoute,    nullptr, storeString,        &m_store.coRoute};
        m_chAltn     = {nullptr,               &m_altnCoRte, storeSlash,  &m_altnCoRte};
        m_chFltNbr   = {&m_store.fltNbr,     nullptr, storeString,        &m_store.fltNbr};
        m_chCostIdx  = {&m_store.costIdxStr, nullptr, storeString,        &m_store.costIdxStr};
        m_chCrzFl    = {&m_store.crzFlTemp,  nullptr, storeCrzFlTemp,     &m_crzCtx};
        m_chFromTo   = {nullptr,               &m_fromTo,   storeSlashValidated, &m_fromToCtx};
        m_chGndTemp  = {&m_store.gndTempStr, nullptr, storeString,        &m_store.gndTempStr};
        m_chTropo    = {&m_store.tropoStr,   nullptr, storeString,        &m_store.tropoStr};
    }

    const ClickHandler* getClickHandler(int side, int lskIdx) override {
        if (side == 0) {
            switch (lskIdx) {
                case 0: return &m_chCoRoute;
                case 1: return &m_chAltn;
                case 2: return &m_chFltNbr;
                case 4: return &m_chCostIdx;
                case 5: return &m_chCrzFl;
            }
        } else {
            switch (lskIdx) {
                case 0: return &m_chFromTo;
                case 4: return &m_chGndTemp;
                case 5: return &m_chTropo;
            }
        }
        return nullptr;
    }

    void buildScreen(ScreenBuffer& buf) override {
        buf.setString(0, 10, "INIT", CellColor::WHITE);

        // L1 / R1
        FieldRenderer::render(buf, Field::LABEL_SMALL, 1, 1,  0, 0, "CO RTE",  CellColor::WHITE);
        FieldRenderer::render(buf, Field::LABEL_SMALL, 1, 15, 0, 0, "FROM/TO", CellColor::WHITE);
        FieldRenderer::render(buf, Field::BOX, 2, 0,  10, 0, m_store.coRoute, CellColor::CYAN);
        FieldRenderer::render(buf, Field::SLASH, 2, 15, 4,  4, m_store.fromAirport, CellColor::CYAN, m_store.toAirport);

        // L2 / R2
        FieldRenderer::render(buf, Field::LABEL_SMALL, 3, 0, 0, 0, "ALTN/CO RTE", CellColor::WHITE);
        {
            CellColor lc = m_store.altnCoRte.empty() ? CellColor::WHITE : CellColor::CYAN;
            if (m_store.altnCoRte.empty())
                for (int i = 0; i < 4; i++) buf.setCell(4, i, '-', lc);
            else {
                std::string s = m_store.altnCoRte.substr(0, 4);
                s.resize(4, ' ');
                buf.setString(4, 0, s, lc);
            }
            CellColor sc = (m_store.altnCoRte.empty() && m_store.altnCoRteRight.empty()) ? CellColor::WHITE : CellColor::CYAN;
            buf.setCell(4, 4, '/', sc);
            CellColor rc = m_store.altnCoRteRight.empty() ? CellColor::WHITE : CellColor::CYAN;
            if (m_store.altnCoRteRight.empty())
                buf.setString(4, 5, std::string(10, '-'), rc);
            else {
                std::string s = m_store.altnCoRteRight.substr(0, 10);
                s.resize(10, ' ');
                buf.setString(4, 5, s, rc);
            }
        }

        // L3 / R3
        FieldRenderer::render(buf, Field::LABEL_SMALL, 5, 0, 0, 0, "FLT NBR", CellColor::WHITE);
        buf.setString(6, 14, "ALIGN IRS>", CellColor::AMBER);
        FieldRenderer::render(buf, Field::BOX, 6, 0, 7, 0, m_store.fltNbr, CellColor::CYAN);

        // L4 / R4
        FieldRenderer::render(buf, Field::LABEL_SMALL, 11, 19, 0, 0, "TROPO", CellColor::WHITE);
        FieldRenderer::render(buf, Field::BOX, 12, 19, 5, 0, m_store.tropoStr, CellColor::CYAN, "", Align::RIGHT);

        // L5 / R5
        FieldRenderer::render(buf, Field::LABEL_SMALL, 9, 0,  0, 0, "COST INDEX", CellColor::WHITE);
        FieldRenderer::render(buf, Field::LABEL_SMALL, 9, 16, 0, 0, "GND TEMP",   CellColor::WHITE);
        FieldRenderer::render(buf, Field::BOX, 10, 0, 3, 0, m_store.costIdxStr, CellColor::CYAN);
        {
            CellColor gc = m_store.gndTempStr.empty() ? CellColor::WHITE : CellColor::CYAN;
            std::string gnd = m_store.gndTempStr.empty() ? "---" : m_store.gndTempStr;
            buf.setString(10, 20, padLeft(gnd + DEG, 4), gc, 14);
        }

        // L6 / R6
        FieldRenderer::render(buf, Field::LABEL_SMALL, 11, 0, 0, 0, "CRZ FL/TEMP", CellColor::WHITE);
        {
            CellColor cc = m_store.crzFlTemp.empty() ? CellColor::AMBER : CellColor::GREEN;
            std::string crz = m_store.crzFlTemp.empty() ? "-----/---" : m_store.crzFlTemp;
            buf.setString(12, 0, crz + DEG, cc);
        }
    }

private:
    FMSDataStore& m_store;
    SlashTarget m_fromTo;
    SlashTarget m_altnCoRte;
    FromToStoreCtx m_fromToCtx;
    CrzFlStoreCtx m_crzCtx;

    ClickHandler m_chCoRoute;
    ClickHandler m_chAltn;
    ClickHandler m_chFltNbr;
    ClickHandler m_chCostIdx;
    ClickHandler m_chCrzFl;
    ClickHandler m_chFromTo;
    ClickHandler m_chGndTemp;
    ClickHandler m_chTropo;

    static std::string padLeft(const std::string& s, int w) {
        if (static_cast<int>(s.size()) >= w) return s.substr(0, static_cast<size_t>(w));
        return std::string(static_cast<size_t>(w - s.size()), ' ') + s;
    }
};
