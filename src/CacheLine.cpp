#include "CacheLine.h"

CacheLine::CacheLine(): m_valid(false), m_dirty(false), m_tag(0), m_lastUsed(0), m_insertionTime(0) {}

void CacheLine::reset() {
    m_valid = false;
    m_dirty = false;
    m_tag = 0;
    m_lastUsed = 0;
    m_insertionTime = 0;
}

//getter and setter methods
bool CacheLine::isValid() const { return m_valid; }
bool CacheLine::isDirty() const { return m_dirty; }
uint64_t CacheLine::getTag() const { return m_tag; }
uint64_t CacheLine::getLastUsed() const { return m_lastUsed; }
uint64_t CacheLine::getInsertionTime() const { return m_insertionTime; }

void CacheLine::setValid(bool valid) { m_valid = valid; }
void CacheLine::setDirty(bool dirty) { m_dirty = dirty; }
void CacheLine::setTag(uint64_t tag) { m_tag = tag; }
void CacheLine::setLastUsed(uint64_t timestamp) { m_lastUsed = timestamp; }
void CacheLine::setInsertionTime(uint64_t timestamp) { m_insertionTime = timestamp; }
