#include "analyzer.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_map>

void TripAnalyzer::ingestFile(const std::string& csvPath) {
    std::ifstream file(csvPath);
	
    if (!file.is_open())
	{
	   return;
	}

    std::string line;
	
    if (!std::getline(file, line)) 
	{
		return; // Skip header
	}
	
    // Use unordered_map for O(1) average time complexity aggregation
    std::unordered_map<std::string, long long> zoneMap;
    std::unordered_map<std::string, long long> slotMap;

    while (std::getline(file, line)) {
		
        if (line.empty()) 
		{
			continue;
		}
		
        // Manually fast-parse to avoid overhead of stringstream
        size_t firstComma = line.find(',');
        size_t secondComma = line.find(',', firstComma + 1);
        size_t thirdComma = line.find(',', secondComma + 1);
        size_t fourthComma = line.find(',', thirdComma + 1);

        if (secondComma == std::string::npos || fourthComma == std::string::npos) 
		{
			continue;
		}
		
        std::string zone = line.substr(firstComma + 1, secondComma - firstComma - 1);
        std::string dateTime = line.substr(thirdComma + 1, fourthComma - thirdComma - 1);

        if (zone.empty() || dateTime.length() < 13) 
		{
			continue;
		}
		
        try {
            int hour = std::stoi(dateTime.substr(11, 2));
			
            if (hour < 0 || hour > 23)
			{
				continue;
			}

            zoneMap[zone]++;
			
            // Create a unique hashable key for the slot: "Zone:Hour"
            std::string slotKey = zone + ":" + std::to_string(hour);
            slotMap[slotKey]++;
			
        } catch (...) {
            continue;
        }
    }

    // Move data to cache for querying
    cachedZones.clear();
    for (auto const& [name, count] : zoneMap) {
        cachedZones.push_back({name, count});
    }

    cachedSlots.clear();
    for (auto const& [key, count] : slotMap) {
        size_t colon = key.find(':');
        std::string z = key.substr(0, colon);
        int h = std::stoi(key.substr(colon + 1));
        cachedSlots.push_back({z, h, count});
    }
}

// topZones
std::vector<ZoneCount> TripAnalyzer::topZones(int k) const {
    std::vector<ZoneCount> result = cachedZones;
    std::sort(result.begin(), result.end(), [](const ZoneCount& a, const ZoneCount& b) {
        if (a.count != b.count) 
		{
			return a.count > b.count;
		}
        return a.zone < b.zone;
    });
    if ((int)result.size() > k) 
	{
		result.resize(k);
	}
    return result;
}

// topBusySlots
std::vector<SlotCount> TripAnalyzer::topBusySlots(int k) const {
    std::vector<SlotCount> result = cachedSlots;
    std::sort(result.begin(), result.end(), [](const SlotCount& a, const SlotCount& b) {
        if (a.count != b.count) 
		{
			return a.count > b.count;
		}
		
        if (a.zone != b.zone)
		{ 
	        return a.zone < b.zone;
		}
        return a.hour < b.hour;
    });
    if ((int)result.size() > k)
	{
		result.resize(k);
	}
    return result;
}
