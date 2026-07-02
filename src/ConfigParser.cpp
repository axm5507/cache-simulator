#include "ConfigParser.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

//parses an integer that may end with a K (×1024) or M (×1048576) suffix
uint64_t parseSize(const std::string& s) {
    if (s.empty()) throw std::invalid_argument("empty size string");
    size_t pos = 0;
    uint64_t n = std::stoull(s, &pos);
    if (pos < s.size()) {
        char suffix = static_cast<char>(
            std::tolower(static_cast<unsigned char>(s[pos])));
        if      (suffix == 'k') n *= 1024ULL;
        else if (suffix == 'm') n *= 1024ULL * 1024;
        else throw std::invalid_argument("unknown size suffix '" + s.substr(pos) + "'");
    }
    return n;
}

} // namespace

//public interface

bool ConfigParser::applyLine(const std::string& rawLine, CacheConfig& cfg) {
    const std::string line = trim(rawLine);
    if (line.empty() || line[0] == '#') return false;

    const auto eq = line.find('=');
    if (eq == std::string::npos) {
        std::cerr << "ConfigParser: missing '=' in: \"" << line << "\"\n";
        return false;
    }

    const std::string key = toLower(trim(line.substr(0, eq)));

    //strip inline comments from the value portion, then trim
    std::string valRaw = line.substr(eq + 1);
    const auto commentPos = valRaw.find('#');
    if (commentPos != std::string::npos)
        valRaw = valRaw.substr(0, commentPos);
    valRaw = trim(valRaw);
    const std::string val = toLower(valRaw);

    try {
        if (key == "name") {
            cfg.name = valRaw; //preserve original case for the output label
        } else if (key == "cache_size" || key == "capacity") {
            cfg.capacityBytes = parseSize(val);
        } else if (key == "block_size") {
            cfg.blockBytes = static_cast<int>(parseSize(val));
        } else if (key == "associativity" || key == "assoc") {
            cfg.associativity = std::stoi(val);
        } else if (key == "replacement" || key == "replacement_policy") {
            if      (val == "lru")    cfg.replacementPolicy = ReplacementPolicy::LRU;
            else if (val == "fifo")   cfg.replacementPolicy = ReplacementPolicy::FIFO;
            else if (val == "random") cfg.replacementPolicy = ReplacementPolicy::Random;
            else {
                std::cerr << "ConfigParser: unknown replacement policy '" << val << "'\n";
                return false;
            }
        } else if (key == "write_policy") {
            if      (val == "write_back"    || val == "writeback")
                cfg.writePolicy = WritePolicy::WriteBack;
            else if (val == "write_through" || val == "writethrough")
                cfg.writePolicy = WritePolicy::WriteThrough;
            else {
                std::cerr << "ConfigParser: unknown write_policy '" << val << "'\n";
                return false;
            }
        } else if (key == "write_miss" || key == "write_miss_policy") {
            if      (val == "write_allocate"    || val == "writeallocate")
                cfg.writeMissPolicy = WriteMissPolicy::WriteAllocate;
            else if (val == "no_write_allocate" || val == "nowriteallocate"
                  || val == "no_allocate")
                cfg.writeMissPolicy = WriteMissPolicy::NoWriteAllocate;
            else {
                std::cerr << "ConfigParser: unknown write_miss policy '" << val << "'\n";
                return false;
            }
        } else {
            std::cerr << "ConfigParser: unknown key '" << key << "'\n";
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "ConfigParser: bad value for '" << key << "': " << e.what() << "\n";
        return false;
    }
    return true;
}

CacheConfig ConfigParser::fromFile(const std::string& path,
                                   const CacheConfig& defaults) {
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("cannot open config file '" + path + "'");

    CacheConfig cfg = defaults;
    std::string line;
    while (std::getline(file, line))
        applyLine(line, cfg);
    return cfg;
}
