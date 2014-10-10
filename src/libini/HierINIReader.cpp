// Copyright (C) 2014 Kim Lester
// All rights reserved.
// This code is released under same license (New BSD) as the inih reader it is based upon.

#include <stdlib.h>

#include "HierINIReader.h"

using std::string;


string HierINIReader::Get(string section, string name, string default_value)/*, bool search_parent)*/
{
    // Hierarchy is dotted. Eg:  [a.b.c].name -> [a.b].name -> [a].name -> [""].name
    // where [""] is any initial unnamed section if it exists.

    string key = MakeKey(section, name);

    if (_values.count(key))
        return _values[key];

//    if (!search_parent)
//        return default_value;
    if (section.length() == 0) // already looked at top level - give up
        return default_value;

    size_t pos = section.rfind(".");
    section = (pos == string::npos) ? "" : section.substr(0,pos-1);

    //return Get(section, name, default_value, search_parent);
    return Get(section, name, default_value);
}


uint32_t HierINIReader::GetUint32(string section, string name, uint32_t default_value)
{
    string valstr = Get(section, name, "");
    const char* value = valstr.c_str();
    char* end;
    // This parses "1234" (decimal) and also "0x4D2" (hex)
    uint32_t n = (uint32_t) strtol(value, &end, 0);
    return end > value ? n : default_value;
}

