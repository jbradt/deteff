#ifndef PAD_MAP_H
#define PAD_MAP_H

#include <iostream>
#include <fstream>
#include <unordered_map>

class PadMap
{
public:
    PadMap(const std::string& path);

    uint16_t find(int cobo, int asad, int aget, int channel) const;

    bool empty() const;

    uint16_t missingValue {20000};

protected:
    static int CalculateHash(int cobo, int asad, int aget, int channel);

    std::unordered_map<int,uint16_t> table;  // The hashtable, maps hash:value
};

#endif /* defined(PAD_MAP_H) */
