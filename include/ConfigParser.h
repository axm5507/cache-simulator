#pragma once

#include "Config.h"
#include <string>

// Parses a key=value text file into a CacheConfig.
//
// File format rules:
//   - One setting per line: key = value
//   - Lines beginning with '#' are comments and are skipped.
//   - Blank lines are skipped.
//   - Keys and values are stripped of surrounding whitespace.
//   - Inline comments (# ...) after a value are stripped.
//   - Keys are case-insensitive; string values (e.g. name) preserve their case.
//   - cache_size and block_size accept a K or M suffix (32K = 32768, 1M = 1048576).
//   - Unknown keys emit a warning to stderr and are skipped.
//
// Supported keys:
//   name          any string          — label shown in output
//   cache_size    integer[K|M]        — total capacity in bytes
//   block_size    integer[K|M]        — bytes per cache line
//   associativity integer             — number of ways per set
//   replacement   lru | fifo | random — eviction policy
//   write_policy  write_back | write_through
//   write_miss    write_allocate | no_write_allocate
//
// Typical usage:
//   cache_sim trace.txt --config configs/l1.conf --assoc 8
class ConfigParser {
public:
    // Parse `path` and return a CacheConfig.
    // `defaults` is the starting point; only keys present in the file are
    // overwritten, so CLI flags applied before this call are preserved.
    // Throws std::runtime_error if the file cannot be opened.
    static CacheConfig fromFile(const std::string& path,
                                const CacheConfig& defaults = CacheConfig{});

    // Apply one "key = value" line to `cfg`.
    // Returns true if the key was recognised and applied.
    // Returns false for blank lines, comment lines, and malformed lines.
    // Unknown keys and bad values print a warning and return false.
    static bool applyLine(const std::string& line, CacheConfig& cfg);
};
