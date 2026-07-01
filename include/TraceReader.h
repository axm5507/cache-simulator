#pragma once

#include <cstdint>
#include <fstream>
#include <istream>
#include <optional>
#include <string>

// One parsed memory access record
struct TraceEntry {
    char op;      // 'R' (read) or 'W' (write)
    uint64_t address; // byte level memory address
};


//This class will stream TraceEntry records form a memory access trace file
//The supported trace format is one access per line, <op> <hex-address>
//op = R|r or W|w, and hex-address = 0x... or plain hex digits
//Lines starting with # or blank lines are skipped, malformed lines emit a warning and are skipped

class TraceReader {
public:
    //construct from a file path, check isOpen() before calling next()
    explicit TraceReader(const std::string& filePath);

    //construct from an existing stream, caller is responsible for the stream's lifetime
    explicit TraceReader(std::istream& stream);

    //returns next valid entry/std::nullopt at end of file
    std::optional<TraceEntry> next();

    //true if underlying file/stream is ready to read
    bool isOpen() const;

    //1-based number of the last line read
    uint64_t lineNumber() const;

private:
    std::ifstream m_fileOwned; //valid only when constructed from a path
    std::istream* m_stream;    //always points to either m_fileOwned or the injected stream
    uint64_t m_lineNumber;

    //parse one raw text line, return nullopt if the line should be skipped
    std::optional<TraceEntry> parseLine(const std::string& raw) const;
};
