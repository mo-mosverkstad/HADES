#pragma once
#include <cstdint>
#include <vector>
#include "cache.h"
#include "mem_hierarchy.h"

// MemoryPort: One access path (instruction or data) with its own cache.
// Owned by the Memory composite. Not used standalone.

class MemoryPort {
public:
    explicit MemoryPort(MemHierarchy& mem) : mem_(mem) {}

    uint8_t read_byte(uint32_t addr) { account_read(addr); return mem_.read_byte(addr); }
    uint16_t read_half(uint32_t addr) { account_read(addr); return mem_.read_half(addr); }
    uint32_t read_word(uint32_t addr) { account_read(addr); return mem_.read_word(addr); }

    void write_byte(uint32_t addr, uint8_t val) { account_write(addr); mem_.write_byte(addr, val); }
    void write_half(uint32_t addr, uint16_t val) { account_write(addr); mem_.write_half(addr, val); }
    void write_word(uint32_t addr, uint32_t val) { account_write(addr); mem_.write_word(addr, val); }

    uint32_t drain_penalty() { uint32_t p = penalty_; penalty_ = 0; return p; }

    void set_cache_enabled(bool enabled) { cache_enabled_ = enabled; }
    void set_miss_penalty(uint32_t cycles) { miss_penalty_ = cycles; }

    uint64_t get_cache_misses() const { return cache_.get_misses(); }
    uint64_t get_cache_hits() const { return cache_.get_hits(); }

    void reset() { cache_.reset(); penalty_ = 0; }

private:
    MemHierarchy& mem_;
    Cache cache_;
    bool cache_enabled_ = false;
    uint32_t miss_penalty_ = 20;
    uint32_t penalty_ = 0;

    void account_read(uint32_t addr) {
        if (cache_enabled_) {
            if (!cache_.access(addr))
                penalty_ += miss_penalty_ + mem_.compute_latency(addr);
        } else if (mem_.is_enabled()) {
            penalty_ += mem_.compute_latency(addr);
        }
    }

    void account_write(uint32_t addr) {
        if (cache_enabled_) cache_.write_access(addr);
        if (mem_.is_enabled()) mem_.compute_latency(addr, true);
    }
};

// Memory: Composite owning the entire memory subsystem.
// CPUBase holds a single Memory instance. Access via chaining:
//   mem_.icache().read_word(pc_)
//   mem_.dcache().read_word(addr)
//   mem_.sdram().get_row_hits()

class Memory {
public:
    Memory() : icache_(hierarchy_), dcache_(hierarchy_) {}

    // ─── Port access (chaining) ─────────────────────────────────────────

    MemoryPort& icache() { return icache_; }
    MemoryPort& dcache() { return dcache_; }
    const MemoryPort& icache() const { return icache_; }
    const MemoryPort& dcache() const { return dcache_; }

    // ─── SDRAM stats/config ─────────────────────────────────────────────

    SDRAMModel& sdram() { return hierarchy_.sdram(); }
    const SDRAMModel& sdram() const { return hierarchy_.sdram(); }

    // ─── Global configuration ───────────────────────────────────────────

    void set_hierarchy_enabled(bool enabled) { hierarchy_.set_enabled(enabled); }
    bool is_hierarchy_enabled() const { return hierarchy_.is_enabled(); }

    void set_miss_penalty(uint32_t cycles) {
        icache_.set_miss_penalty(cycles);
        dcache_.set_miss_penalty(cycles);
    }

    void set_cache_enabled(bool enabled) {
        icache_.set_cache_enabled(enabled);
        dcache_.set_cache_enabled(enabled);
    }

    // ─── Direct access (bypasses cache, for program/data loading) ───────

    void load(uint32_t base, const std::vector<uint8_t>& data) { hierarchy_.load(base, data); }
    std::vector<uint8_t> dump(uint32_t addr, uint32_t len) const { return hierarchy_.dump(addr, len); }

    // ─── Lifecycle ──────────────────────────────────────────────────────

    void reset() {
        hierarchy_.clear();
        icache_.reset();
        dcache_.reset();
    }

private:
    MemHierarchy hierarchy_;
    MemoryPort icache_;
    MemoryPort dcache_;
};
