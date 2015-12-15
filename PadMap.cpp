#include "PadMap.h"
#include <string>
#include <sstream>

int PadMap::CalculateHash(int cobo, int asad, int aget, int channel)
{
    return channel + aget*100 + asad*10000 + cobo*1000000;
}

uint16_t PadMap::find(int cobo, int asad, int aget, int channel) const
{
    auto hash = CalculateHash(cobo, asad, aget, channel);
    auto foundItem = table.find(hash);
    if (foundItem != table.end()) {
        return foundItem->second;
    }
    else {
        return missingValue; // an invalid value
    }
}

bool PadMap::empty() const
{
    return table.empty();
}

PadMap::PadMap(const std::string& path)
{
    std::ifstream file (path, std::ios::in|std::ios::binary);

    // MUST throw out the first two junk lines in file. No headers!

    if (!file.good()) throw 0; // FIX THIS!

    if (table.size() != 0) {
        table.clear();
    }

    std::string line;

    while (!file.eof()) {
        int cobo, asad, aget, channel;
        uint16_t value;
        getline(file,line,'\n');
        std::stringstream lineStream(line);
        std::string element;

        getline(lineStream, element,',');
        if (element == "-1" || element == "") continue; // KLUDGE!
        cobo = stoi(element);

        getline(lineStream, element,',');
        asad = stoi(element);

        getline(lineStream, element,',');
        aget = stoi(element);

        getline(lineStream, element,',');
        channel = stoi(element);

        auto hash = CalculateHash(cobo, asad, aget, channel);

        getline(lineStream, element);
        value = stoi(element);

        table.emplace(hash, value);
    }
}
