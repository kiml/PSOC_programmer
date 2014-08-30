#ifndef __HIERINIREADER_H__
#define __HIERINIREADER_H__

#include "INIReader.h"

class HierINIReader: public INIReader
{
public:
    HierINIReader(std::string filename) : INIReader(filename) {};

    // Get a string value from INI file, returning default_value if not found.
    // Hierarchy is dotted. Eg:  [a.b.c].name -> [a.b].name -> [a].name -> [""].name
    // where [""] is any initial unnamed section if it exists.
    std::string Get(std::string section, std::string name,
                    std::string default_value); // , bool search_parent=true);

    uint32_t GetUint32(std::string section, std::string name, uint32_t default_value);
};

#endif  // __HIERINIREADER_H__
