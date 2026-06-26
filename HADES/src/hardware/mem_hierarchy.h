#pragma once
#include <cstdint>
#include <vector>
#include <cstring>

// DTEK-V Memory Hierarchy:
//   On-chip RAM: fast (1-2 cycles)
//   SDRAM:       slow (20-50 cycles), row/bank effects, refresh
//
// For backward compatibility with Phase 1-5 (flat 64KB at 0x0000-0xFFFF),
// the hierarchy is OPTIONAL. When disabled, all accesses go to flat memory.
//
// When enabled, address routing:
//   0x0000-0xFFFF → on-chip RAM (64KB, 1-2 cycle latency)
//   Addresses outside this range → SDRAM model (high latency)
//
// SDRAM model:
//   - Row buffer: accessing same row = fast (5 cycles)
//   - Row miss: different row = slow (25 cycles)
//   - Refresh: every N cycles, adds stall

class SDRAMModel {
public:
    SDRAMModel() { reset(); }

    void reset() {
        current_row_ = 0xFFFFFFFF; // no row open
        access_count_ = 0;
        row_hits_ = 0;
        row_misses_ = 0;
        refresh_stalls_ = 0;
        cycle_counter_ = 0;
    }

    // Returns latency in cycles for accessing this address
    uint32_t access_latency(uint32_t addr) {
        access_count_++;
        cycle_counter_++;

        // Check for periodic refresh (every 10000 accesses)
        uint32_t refresh_penalty = 0;
        if (cycle_counter_ % refresh_interval_ == 0) {
            refresh_penalty = refresh_latency_;
            refresh_stalls_++;
        }

        // Row buffer check
        uint32_t row = addr >> row_bits_; // row = upper bits
        if (row == current_row_) {
            // Row hit: data already in row buffer
            row_hits_++;
            return row_hit_latency_ + refresh_penalty;
        } else {
            // Row miss: must activate new row
            current_row_ = row;
            row_misses_++;
            return row_miss_latency_ + refresh_penalty;
        }
    }

    // Configuration
    void set_row_hit_latency(uint32_t cycles) { row_hit_latency_ = cycles; }
    void set_row_miss_latency(uint32_t cycles) { row_miss_latency_ = cycles; }
    void set_refresh_interval(uint32_t accesses) { refresh_interval_ = accesses; }
    void set_refresh_latency(uint32_t cycles) { refresh_latency_ = cycles; }
    void set_row_bits(uint32_t bits) { row_bits_ = bits; }

    // Stats
    uint64_t get_row_hits() const { return row_hits_; }
    uint64_t get_row_misses() const { return row_misses_; }
    uint64_t get_refresh_stalls() const { return refresh_stalls_; }
    uint64_t get_access_count() const { return access_count_; }

private:
    uint32_t current_row_;
    uint64_t access_count_;
    uint64_t row_hits_;
    uint64_t row_misses_;
    uint64_t refresh_stalls_;
    uint64_t cycle_counter_;

    // Configurable parameters (DTEK-V defaults)
    uint32_t row_hit_latency_ = 5;      // cycles for same-row access
    uint32_t row_miss_latency_ = 25;    // cycles for different-row access
    uint32_t refresh_interval_ = 10000; // refresh every N accesses
    uint32_t refresh_latency_ = 50;     // cycles lost to refresh
    uint32_t row_bits_ = 10;            // row = addr >> 10 (1KB rows)
};

// Memory hierarchy controller
// Routes addresses and computes access latency
/**
 * Memory hierarchy controller: 1MB flat backing store with optional SDRAM timing model.
 * When hierarchy is enabled, accesses incur row hit/miss latency.
 * When disabled, all accesses are single-cycle (backward compatible).
 */
class MemHierarchy {
public:
    // static constexpr uint32_t ONCHIP_SIZE = 65536; // 64KB on-chip RAM
    static constexpr uint32_t ONCHIP_SIZE = 1048576; // 1MB (expanded for MMU page tables)

    MemHierarchy() : data_(ONCHIP_SIZE, 0), enabled_(false), onchip_latency_(1) {}

    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }

    void set_onchip_latency(uint32_t cycles) { onchip_latency_ = cycles; }

    // Access memory and return latency (0 if hierarchy disabled)
    // The actual data read/write is still done through the flat memory
    // This only computes the LATENCY for timing simulation
    uint32_t compute_latency(uint32_t addr, bool is_write = false) {
        if (!enabled_) return 0;

        // On-chip RAM region: 0x0000-0xFFFF (fast)
        if (addr < ONCHIP_SIZE) {
            return onchip_latency_;
        }

        // Everything else: SDRAM (slow, row-dependent)
        return sdram_.access_latency(addr);
    }

    // Raw data access (flat memory, no latency)
    uint8_t read_byte(uint32_t addr) const {
        return data_[addr & (ONCHIP_SIZE - 1)];
    }

    uint16_t read_half(uint32_t addr) const {
        uint16_t val = 0;
        val |= (uint16_t)data_[(addr + 0) & (ONCHIP_SIZE - 1)];
        val |= (uint16_t)data_[(addr + 1) & (ONCHIP_SIZE - 1)] << 8;
        return val;
    }

    uint32_t read_word(uint32_t addr) const {
        uint32_t val = 0;
        val |= (uint32_t)data_[(addr + 0) & (ONCHIP_SIZE - 1)];
        val |= (uint32_t)data_[(addr + 1) & (ONCHIP_SIZE - 1)] << 8;
        val |= (uint32_t)data_[(addr + 2) & (ONCHIP_SIZE - 1)] << 16;
        val |= (uint32_t)data_[(addr + 3) & (ONCHIP_SIZE - 1)] << 24;
        return val;
    }

    void write_byte(uint32_t addr, uint8_t val) {
        data_[addr & (ONCHIP_SIZE - 1)] = val;
    }

    void write_half(uint32_t addr, uint16_t val) {
        data_[(addr + 0) & (ONCHIP_SIZE - 1)] = val & 0xFF;
        data_[(addr + 1) & (ONCHIP_SIZE - 1)] = (val >> 8) & 0xFF;
    }

    void write_word(uint32_t addr, uint32_t val) {
        data_[(addr + 0) & (ONCHIP_SIZE - 1)] = val & 0xFF;
        data_[(addr + 1) & (ONCHIP_SIZE - 1)] = (val >> 8) & 0xFF;
        data_[(addr + 2) & (ONCHIP_SIZE - 1)] = (val >> 16) & 0xFF;
        data_[(addr + 3) & (ONCHIP_SIZE - 1)] = (val >> 24) & 0xFF;
    }

    void load(uint32_t base_addr, const std::vector<uint8_t>& bytes) {
        for (size_t i = 0; i < bytes.size(); i++) {
            data_[(base_addr + i) & (ONCHIP_SIZE - 1)] = bytes[i];
        }
    }

    std::vector<uint8_t> dump(uint32_t addr, uint32_t len) const {
        std::vector<uint8_t> out(len);
        for (uint32_t i = 0; i < len; i++) {
            out[i] = data_[(addr + i) & (ONCHIP_SIZE - 1)];
        }
        return out;
    }

    void clear() {
        std::fill(data_.begin(), data_.end(), 0);
        sdram_.reset();
    }

    // SDRAM stats
    SDRAMModel& sdram() { return sdram_; }
    const SDRAMModel& sdram() const { return sdram_; }

private:
    std::vector<uint8_t> data_;
    SDRAMModel sdram_;
    bool enabled_;
    uint32_t onchip_latency_;
};