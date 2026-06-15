#pragma once
#include <cstdint>
#include <cstring>

// DTEK-V L1 Cache: 2KB, direct-mapped, 32-byte blocks, 64 lines
// Address breakdown: [tag | index(6 bits) | offset(5 bits)]

class Cache {
public:
    static constexpr uint32_t NUM_LINES = 64;
    static constexpr uint32_t BLOCK_SIZE = 32;   // bytes per block
    static constexpr uint32_t INDEX_BITS = 6;    // log2(64)
    static constexpr uint32_t OFFSET_BITS = 5;   // log2(32)
    static constexpr uint32_t TAG_SHIFT = INDEX_BITS + OFFSET_BITS; // 11

    Cache() { reset(); }

    void reset() {
        for (uint32_t i = 0; i < NUM_LINES; i++) {
            lines_[i].valid = false;
            lines_[i].tag = 0;
        }
        hits_ = 0;
        misses_ = 0;
    }

    // Returns true on hit, false on miss
    bool access(uint32_t addr) {
        uint32_t index = (addr >> OFFSET_BITS) & (NUM_LINES - 1);
        uint32_t tag = addr >> TAG_SHIFT;

        if (lines_[index].valid && lines_[index].tag == tag) {
            hits_++;
            return true;  // HIT
        } else {
            // MISS: allocate line
            lines_[index].valid = true;
            lines_[index].tag = tag;
            misses_++;
            return false; // MISS
        }
    }

    // Write-through: on store, just check if line exists (for timing)
    // Does NOT allocate on write miss (write-through, no-write-allocate)
    bool write_access(uint32_t addr) {
        uint32_t index = (addr >> OFFSET_BITS) & (NUM_LINES - 1);
        uint32_t tag = addr >> TAG_SHIFT;

        if (lines_[index].valid && lines_[index].tag == tag) {
            hits_++;
            return true;  // HIT (write-through: data goes to memory too)
        } else {
            misses_++;
            return false; // MISS (write goes directly to memory)
        }
    }

    // Invalidate entire cache (useful for experiments)
    void flush() {
        for (uint32_t i = 0; i < NUM_LINES; i++) {
            lines_[i].valid = false;
        }
    }

    uint64_t get_hits() const { return hits_; }
    uint64_t get_misses() const { return misses_; }

private:
    struct CacheLine {
        bool valid;
        uint32_t tag;
    };

    CacheLine lines_[NUM_LINES];
    uint64_t hits_;
    uint64_t misses_;
};