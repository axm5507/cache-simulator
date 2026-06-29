#include "CacheSet.h"
#include <cassert>

CacheSet::CacheSet(int associativity): m_associativity(associativity), m_lines(associativity){}

int CacheSet::associativity() const {
    return m_associativity;
}

int CacheSet::findTag(uint64_t tag) const {
    for (int way = 0; way < m_associativity; ++way) {
        if (m_lines[way].isValid() && m_lines[way].getTag() == tag)
            return way;
    }
    return -1;
}

int CacheSet::findEmptyWay() const {
    for (int way = 0; way < m_associativity; ++way) {
        if (!m_lines[way].isValid())
            return way;
    }
    return -1;
}

bool CacheSet::hasEmptyWay() const {
    return findEmptyWay() != -1;
}

CacheLine& CacheSet::getLine(int way) {
    assert(way >= 0 && way < m_associativity);
    return m_lines[way];
}

const CacheLine& CacheSet::getLine(int way) const {
    assert(way >= 0 && way < m_associativity);
    return m_lines[way];
}
