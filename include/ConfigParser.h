#pragma once

#include "Config.h"
#include <string>

//I decided to implement this so that cache parameters can be loaded in from a text config file
//supported keys are name, cache_size, block_size, associativity, replacement, write_policy, and write_miss
//file format rules include one key = value per line, lines beginning with # and blank lines are skipped,
//Comments after a value are stripped, surrounding whitespace is stripped, keys are case-insensitive,
//cache_size and block_size accept a K or M suffix, and unknown keys emit a warning to stderr and are skipped
class ConfigParser {
public:
    //parse `path` and return a CacheConfig
    static CacheConfig fromFile(const std::string& path, const CacheConfig& defaults = CacheConfig{});

    //apply one "key = value" line to cfg, returns true if key was recognised and applied, false otherwise
    static bool applyLine(const std::string& line, CacheConfig& cfg);
};
