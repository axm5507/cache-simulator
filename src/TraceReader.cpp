#include "TraceReader.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>

//constructors

TraceReader::TraceReader(const std::string& filePath): m_fileOwned(filePath), m_stream(&m_fileOwned), m_lineNumber(0){}

TraceReader::TraceReader(std::istream& stream): m_stream(&stream), m_lineNumber(0){}

//public interface

bool TraceReader::isOpen() const {
    //if constructed from a path, m_stream points to m_fileOwned
    if (m_stream == static_cast<const std::istream*>(&m_fileOwned))
        return m_fileOwned.is_open();
    //injected stream, assume the caller provides a valid stream
    return m_stream != nullptr;
}

uint64_t TraceReader::lineNumber() const { return m_lineNumber; }

std::optional<TraceEntry> TraceReader::next() {
    std::string line;
    while (std::getline(*m_stream, line)) {
        ++m_lineNumber;
        auto entry = parseLine(line);
        if (entry.has_value())
            return entry;
        // parseLine returns nullopt for blank/comment lines and malformed lines
    }
    return std::nullopt;
}

//private helpers

std::optional<TraceEntry> TraceReader::parseLine(const std::string& raw) const {
    size_t start = raw.find_first_not_of(" \t\r");
    if (start == std::string::npos) return std::nullopt; //blank line
    const std::string line = raw.substr(start);

    //skip comment lines
    if (line[0] == '#') return std::nullopt;

    std::istringstream ss(line);
    char op = 0;
    std::string addrStr;

    if (!(ss >> op >> addrStr)) {
        std::cerr << "TraceReader: malformed line " << m_lineNumber
                  << ": \"" << line << "\"\n";
        return std::nullopt;
    }

    op = static_cast<char>(std::toupper(static_cast<unsigned char>(op)));
    if (op != 'R' && op != 'W') {
        std::cerr << "TraceReader: unknown op '" << op
                  << "' on line " << m_lineNumber << "\n";
        return std::nullopt;
    }

    // Parse hex address; accept both "0x..." and plain hex
    uint64_t address = 0;
    try {
        size_t consumed = 0;
        address = std::stoull(addrStr, &consumed, 16);
        if (consumed != addrStr.size()) {
            std::cerr << "TraceReader: trailing garbage after address on line "
                      << m_lineNumber << ": \"" << addrStr << "\"\n";
            return std::nullopt;
        }
    } catch (const std::exception&) {
        std::cerr << "TraceReader: invalid address on line "
                  << m_lineNumber << ": \"" << addrStr << "\"\n";
        return std::nullopt;
    }

    return TraceEntry{op, address};
}
