#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <mutex>

/*
   NavDatabase: in-memory waypoint/navaid database.
   Supports CSV and X-Plane .dat formats (earth_fix.dat, earth_nav.dat).

   X-Plane format refs:
     earth_fix.dat: lat lon name (whitespace delimited)
     earth_nav.dat: type lat lon elev freq range var ident name
   Both start with "I" header line, terminate with "99".
*/

enum class NavType { FIX, VOR, NDB };

struct WaypointRecord {
    std::string id;
    double lat = 0;
    double lon = 0;
    std::string name;
    std::string region;
    NavType type = NavType::FIX;
    int freqKhz = 0;
};

class NavDatabase {
public:
    int loadCSV(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "NavDatabase: Could not open " << path << "\n";
            return -1;
        }

        auto saved = m_data.size();
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;

            std::istringstream ss(line);
            WaypointRecord rec;

            if (!std::getline(ss, rec.id, ',')) continue;
            std::string token;
            if (!std::getline(ss, token, ',')) continue;
            try { rec.lat = std::stod(token); } catch (...) { continue; }
            if (!std::getline(ss, token, ',')) continue;
            try { rec.lon = std::stod(token); } catch (...) { continue; }
            std::getline(ss, rec.name, ',');
            std::getline(ss, rec.region, ',');

            if (!rec.id.empty())
                m_data[rec.id] = rec;
        }

        int count = (int)(m_data.size() - saved);
        std::cout << "NavDatabase: Loaded " << count << " waypoints from " << path << "\n";
        return count;
    }

    // X-Plane earth_fix.dat parser (thread-safe)
    int loadXPlaneFix(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "NavDatabase: Could not open " << path << "\n";
            return -1;
        }

        std::unordered_map<std::string, WaypointRecord> local;
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == 'I') continue;
            if (line == "99") break;

            std::istringstream ss(line);
            WaypointRecord rec;
            rec.type = NavType::FIX;

            std::string latStr, lonStr;
            if (!(ss >> latStr >> lonStr >> rec.id)) continue;

            try { rec.lat = std::stod(latStr); } catch (...) { continue; }
            try { rec.lon = std::stod(lonStr); } catch (...) { continue; }
            rec.name = rec.id;

            if (!rec.id.empty())
                local[rec.id] = rec;
        }

        int count = (int)local.size();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& [id, rec] : local)
                m_data[id] = std::move(rec);
        }

        std::cout << "NavDatabase: Loaded " << count << " fixes from " << path << "\n";
        return count;
    }

    // X-Plane earth_nav.dat parser - VORs (3) & NDBs (2) only (thread-safe)
    int loadXPlaneNav(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "NavDatabase: Could not open " << path << "\n";
            return -1;
        }

        std::unordered_map<std::string, WaypointRecord> local;
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == 'I') continue;
            if (line == "99") break;

            std::istringstream ss(line);
            int typeCode = 0;
            ss >> typeCode;

            if (typeCode != 2 && typeCode != 3) continue;

            WaypointRecord rec;
            rec.type = (typeCode == 2) ? NavType::NDB : NavType::VOR;

            std::string latStr, lonStr, elevStr, freqStr, rangeStr, varStr;
            if (!(ss >> latStr >> lonStr >> elevStr >> freqStr >> rangeStr >> varStr >> rec.id)) continue;

            try { rec.lat = std::stod(latStr); } catch (...) { continue; }
            try { rec.lon = std::stod(lonStr); } catch (...) { continue; }
            try { rec.freqKhz = std::stoi(freqStr); } catch (...) { rec.freqKhz = 0; }

            std::string rest;
            std::getline(ss, rest);
            rec.name = rec.id;
            if (!rest.empty()) {
                size_t start = rest.find_first_not_of(" \t");
                if (start != std::string::npos)
                    rec.name = rest.substr(start);
            }

            if (!rec.id.empty())
                local[rec.id] = rec;
        }

        int count = (int)local.size();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& [id, rec] : local)
                m_data[id] = std::move(rec);
        }

        std::cout << "NavDatabase: Loaded " << count << " navaids from " << path << "\n";
        return count;
    }

    // OurAirports CSV loader (ICAO codes for FROM/TO, thread-safe)
    int loadAirportsCSV(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "NavDatabase: Could not open " << path << "\n";
            return -1;
        }

        std::unordered_map<std::string, WaypointRecord> local;
        std::string line;
        bool header = true;
        while (std::getline(file, line)) {
            if (header) { header = false; continue; }
            if (line.empty()) continue;

            std::vector<std::string> fields;
            bool inQuotes = false;
            std::string field;
            for (char c : line) {
                if (c == '"') { inQuotes = !inQuotes; continue; }
                if (c == ',' && !inQuotes) {
                    fields.push_back(field);
                    field.clear();
                    continue;
                }
                field += c;
            }
            fields.push_back(field);

            if (fields.size() < 13) continue;

            std::string icao = fields[12];
            if (icao.size() < 3) continue;

            WaypointRecord rec;
            rec.id = icao;
            rec.type = NavType::FIX;
            try { rec.lat = std::stod(fields[4]); } catch (...) { continue; }
            try { rec.lon = std::stod(fields[5]); } catch (...) { continue; }
            rec.name = fields[3];
            rec.region = fields[9];

            local[rec.id] = rec;
        }

        int count = (int)local.size();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& [id, rec] : local)
                m_data[id] = std::move(rec);
        }

        std::cout << "NavDatabase: Loaded " << count << " airports from " << path << "\n";
        return count;
    }

    const WaypointRecord* find(const std::string& id) const {
        auto it = m_data.find(id);
        return it != m_data.end() ? &it->second : nullptr;
    }

    std::vector<const WaypointRecord*> searchByPrefix(const std::string& prefix, size_t maxResults = 10) const {
        std::vector<const WaypointRecord*> results;
        for (const auto& [id, rec] : m_data) {
            if (id.find(prefix) == 0) {
                results.push_back(&rec);
                if (results.size() >= maxResults) break;
            }
        }
        return results;
    }

    size_t size() const { return m_data.size(); }

private:
    std::unordered_map<std::string, WaypointRecord> m_data;
    std::mutex m_mutex;
};
