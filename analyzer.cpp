#include "analyzer.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_map>

//Helper to parse hour from a string
static int parseHour(const std::string& line, size_t start, size_t end) {
    size_t spacePos = std::string::npos;
    for (size_t i = start; i < end; ++i) 
    {
        if (line[i] == ' ') {
            spacePos = i;
            break;
        }
    }

    if (spacePos == std::string::npos) return -1;
    if (spacePos + 2 >= end) return -1;

    char h1 = line[spacePos + 1];
    char h2 = line[spacePos + 2];

    if (!isdigit(static_cast<unsigned char>(h1)) || !isdigit(static_cast<unsigned char>(h2)))
    {
        return -1;
    }

    int hour = (h1 - '0') * 10 + (h2 - '0');
    if (hour < 0 || hour > 23) return -1;
    return hour;
}

void TripAnalyzer::ingestFile(const std::string& csvPath) {
	cachedZones.clear();
    cachedSlots.clear();
	
    std::ifstream file(csvPath);
	
    if (!file.is_open())
	{
	   return;
	}

    std::string line;
	// Use unordered_map for O(1) average time complexity aggregation
	std::unordered_map<std::string, long long> zMap;
    std::unordered_map<std::string, std::vector<long long>> zSlots;

	zMap.reserve(16384); 
    zSlots.reserve(16384);
	
    while (std::getline(file, line)) {
		
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        //Parsing logic
        //We need TripID Col 0 PickupZone Col 1 and Time Col 2 OR Col 3
        size_t c1 = line.find(',');
        if (c1 == std::string::npos)
        {
            continue;   //Missing columns
        } 

        //Check strictly numeric ID
        bool idIsNumeric = true;
        if (c1 == 0) idIsNumeric = false;
        for (size_t i = 0; i < c1; ++i) 
        {
            if (!isdigit(static_cast<unsigned char>(line[i]))) 
            {
                idIsNumeric = false;
                break;
            }
        }
        if (!idIsNumeric) 
        {
            continue;
        }

        //Parse PickupZoneID
        size_t c2 = line.find(',', c1 + 1);
        size_t zoneEnd = (c2 == std::string::npos) ? line.length() : c2;
        if (zoneEnd <= c1 + 1)
        {
            continue; //Empty zones
        }
        std::string zone = line.substr(c1 + 1, zoneEnd - c1 - 1);
        
        //Find Time Col 2 or Col 3
        if (c2 == std::string::npos)
        {
            continue;
        }
        int hour = -1;

        //Try Column 2 
        size_t c3 = line.find(',', c2 + 1);
        size_t col2End = (c3 == std::string::npos) ? line.length() : c3;

        if (col2End > c2 + 1) 
        {
            hour = parseHour(line, c2 + 1, col2End);
        }

        //If not found in Col 2 try Col 3
        if (hour == -1 && c3 != std::string::npos) 
        {
            size_t c4 = line.find(',', c3 + 1);
            size_t col3End = (c4 == std::string::npos) ? line.length() : c4;
            if (col3End > c3 + 1) 
            {
                hour = parseHour(line, c3 + 1, col3End);
            }
        }

        //If hour is still invalid this row is dirty
        if (hour == -1) continue;
        zMap[zone]++;
        
        //Insert/Update slot
        std::vector<long long>& slots = zSlots[zone];
        if (slots.empty()) 
        {
            slots.resize(24, 0);
        }
        slots[hour]++;
    }
		
    //Transform the output format
    cachedZones.reserve(zMap.size());
    for (const auto& kv : zMap) 
    {
        cachedZones.push_back({kv.first, kv.second});
    }
    //Calculate total slots count heuristic
    size_t totalSlots = 0;
    for (const auto& kv : zSlots) 
    {
        for (long long c : kv.second)
        {
            if (c > 0) totalSlots++;
        }
    }
    cachedSlots.reserve(totalSlots);

    for (const auto& kv : zSlots) 
    {
        const std::string& z = kv.first;
        const std::vector<long long>& hours = kv.second;
        for (int h = 0; h < 24; ++h) 
        {
            if (hours[h] > 0) 
            {
                cachedSlots.push_back({z, h, hours[h]});
            }
        }
    }
}

std::vector<ZoneCount> TripAnalyzer::topZones(int k) const
{
    if (k <= 0)
    {
        return {};
    }
    std::vector<ZoneCount> result = cachedZones;

    // Sort: Count DESC, then Zone ASC
    std::sort(result.begin(), result.end(), [](const ZoneCount& a, const ZoneCount& b) 
        {
            if (a.count != b.count)
                return a.count > b.count;
            return a.zone < b.zone;
        });

    if ((int)result.size() > k)
        result.resize(k);

    return result;
}

std::vector<SlotCount> TripAnalyzer::topBusySlots(int k) const
{
    if (k <= 0) 
    {
        return {};
    }

    std::vector<SlotCount> result = cachedSlots;

    // Sort: Count DESC, then Zone ASC, then Hour ASC
    std::sort(result.begin(), result.end(), [](const SlotCount& a, const SlotCount& b) 
        {
            if (a.count != b.count)
                return a.count > b.count;
            if (a.zone != b.zone)
                return a.zone < b.zone;
            return a.hour < b.hour;
        });

    if ((int)result.size() > k)
        result.resize(k);

    return result;
}

