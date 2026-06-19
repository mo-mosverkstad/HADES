#pragma once
#include <cstdint>
#include <vector>
#include "cache.h"
#include "mem_hierarchy.h"
#include "io_bus.h"

// MemoryPort: One access path into the memory subsystem.
// Internally holds a cache and references the shared backing store.
// The CPU doesn't know or care about caches — it just reads/writes.

class MemoryPort {
public:
    explicit MemoryPort(MemHierarchy& backing, IOBus& io_bus, const bool& io_enabled)
        : backing_(backing), io_bus_(io_bus), io_enabled_(io_enabled) {}

    uint8_t read_byte(uint32_t addr) {
        account_read(addr);
        if (is_io(addr)) {
            uint32_t aligned = addr & ~0x3u;
            uint32_t word = io_bus_.read(aligned);
            uint32_t byte_offset = addr & 0x3;
            return static_cast<uint8_t>(word >> (byte_offset * 8));
        }
        return backing_.read_byte(addr);
    }

    uint16_t read_half(uint32_t addr) {
        account_read(addr);
        if (is_io(addr)) {
            uint32_t aligned = addr & ~0x3u;
            uint32_t word = io_bus_.read(aligned);
            uint32_t byte_offset = addr & 0x3;
            if (byte_offset <= 2) {
                return static_cast<uint16_t>(word >> (byte_offset * 8));
            }
            // Crosses word boundary: low byte from this word, high byte from next
            uint32_t word2 = io_bus_.read(aligned + 4);
            return static_cast<uint16_t>((word >> 24) | (word2 << 8));
        }
        return backing_.read_half(addr);
    }

    uint32_t read_word(uint32_t addr) {
        account_read(addr);
        if (is_io(addr)) {
            uint32_t aligned = addr & ~0x3u;
            uint32_t word = io_bus_.read(aligned);
            uint32_t byte_offset = addr & 0x3;
            if (byte_offset == 0) return word;
            // Unaligned: spans two I/O words
            uint32_t word2 = io_bus_.read(aligned + 4);
            uint32_t shift = byte_offset * 8;
            return (word >> shift) | (word2 << (32 - shift));
        }
        return backing_.read_word(addr);
    }

    void write_byte(uint32_t addr, uint8_t val) {
        account_write(addr);
        if (is_io(addr)) {
            uint32_t aligned = addr & ~0x3u;
            uint32_t word = io_bus_.read(aligned);
            uint32_t byte_offset = addr & 0x3;
            uint32_t shift = byte_offset * 8;
            word = (word & ~(0xFFu << shift)) | (static_cast<uint32_t>(val) << shift);
            io_bus_.write(aligned, word);
            return;
        }
        backing_.write_byte(addr, val);
    }

    void write_half(uint32_t addr, uint16_t val) {
        account_write(addr);
        if (is_io(addr)) {
            uint32_t aligned = addr & ~0x3u;
            uint32_t byte_offset = addr & 0x3;
            uint32_t word = io_bus_.read(aligned);
            uint32_t shift = byte_offset * 8;
            if (byte_offset <= 2) {
                word = (word & ~(0xFFFFu << shift)) | (static_cast<uint32_t>(val) << shift);
                io_bus_.write(aligned, word);
            } else {
                // Crosses word boundary
                word = (word & 0x00FFFFFFu) | (static_cast<uint32_t>(val & 0xFF) << 24);
                io_bus_.write(aligned, word);
                uint32_t word2 = io_bus_.read(aligned + 4);
                word2 = (word2 & 0xFFFFFF00u) | (static_cast<uint32_t>(val >> 8));
                io_bus_.write(aligned + 4, word2);
            }
            return;
        }
        backing_.write_half(addr, val);
    }

    void write_word(uint32_t addr, uint32_t val) {
        account_write(addr);
        if (is_io(addr)) {
            uint32_t aligned = addr & ~0x3u;
            uint32_t byte_offset = addr & 0x3;
            if (byte_offset == 0) {
                io_bus_.write(aligned, val);
            } else {
                // Unaligned: spans two I/O words
                uint32_t shift = byte_offset * 8;
                uint32_t word1 = io_bus_.read(aligned);
                uint32_t mask1 = (1u << shift) - 1u;
                word1 = (word1 & mask1) | (val << shift);
                io_bus_.write(aligned, word1);

                uint32_t word2 = io_bus_.read(aligned + 4);
                uint32_t mask2 = ~((1u << shift) - 1u);
                word2 = (word2 & mask2) | (val >> (32 - shift));
                io_bus_.write(aligned + 4, word2);
            }
            return;
        }
        backing_.write_word(addr, val);
    }

    uint32_t drain_penalty() { uint32_t p = penalty_; penalty_ = 0; return p; }

    // Configuration (called by Memory composite, not by CPU)
    void set_cache_enabled(bool enabled) { cache_enabled_ = enabled; }
    void set_miss_penalty(uint32_t cycles) { miss_penalty_ = cycles; }

    // Stats
    uint64_t get_cache_misses() const { return cache_.get_misses(); }
    uint64_t get_cache_hits() const { return cache_.get_hits(); }

    void reset() { cache_.reset(); penalty_ = 0; }

private:
    MemHierarchy& backing_;
    IOBus& io_bus_;
    const bool& io_enabled_;
    Cache cache_;

    bool cache_enabled_ = false;
    uint32_t miss_penalty_ = 20;
    uint32_t penalty_ = 0;

    bool is_io(uint32_t addr) const {
        return io_enabled_ && io_bus_.is_io_address(addr);
    }

    void account_read(uint32_t addr) {
        if (is_io(addr)) return; // I/O bypasses cache/hierarchy
        if (cache_enabled_) {
            if (!cache_.access(addr))
                penalty_ += miss_penalty_ + backing_.compute_latency(addr);
        } else if (backing_.is_enabled()) {
            penalty_ += backing_.compute_latency(addr);
        }
    }

    void account_write(uint32_t addr) {
        if (is_io(addr)) return; // I/O bypasses cache/hierarchy
        if (cache_enabled_) cache_.write_access(addr);
        if (backing_.is_enabled()) backing_.compute_latency(addr, true);
    }
};

// Memory: The entire memory subsystem as seen by the processor.
// Exposes two ports: imem (instruction) and dmem (data).
// Caches, hierarchy, SDRAM — all internal details.

class Memory {
public:
    Memory(IOBus& io_bus, const bool& io_enabled)
        : hierarchy_(), io_bus_(io_bus),
          imem_(hierarchy_, io_bus, io_enabled), dmem_(hierarchy_, io_bus, io_enabled) {}

    // ─── Port access (what the CPU sees) ────────────────────────────────

    MemoryPort& imem() { return imem_; }
    MemoryPort& dmem() { return dmem_; }
    const MemoryPort& imem() const { return imem_; }
    const MemoryPort& dmem() const { return dmem_; }

    // ─── Global configuration ───────────────────────────────────────────

    void set_hierarchy_enabled(bool enabled) { hierarchy_.set_enabled(enabled); }
    bool is_hierarchy_enabled() const { return hierarchy_.is_enabled(); }

    void set_miss_penalty(uint32_t cycles) {
        imem_.set_miss_penalty(cycles);
        dmem_.set_miss_penalty(cycles);
    }

    void set_cache_enabled(bool enabled) {
        imem_.set_cache_enabled(enabled);
        dmem_.set_cache_enabled(enabled);
    }

    // ─── Stats (exposed for Python bindings) ────────────────────────────

    SDRAMModel& sdram() { return hierarchy_.sdram(); }
    const SDRAMModel& sdram() const { return hierarchy_.sdram(); }

    // ─── Direct access (bypasses cache, for program/data loading) ───────

    void load(uint32_t base, const std::vector<uint8_t>& data) { hierarchy_.load(base, data); }
    std::vector<uint8_t> dump(uint32_t addr, uint32_t len) const { return hierarchy_.dump(addr, len); }

    // ─── Lifecycle ──────────────────────────────────────────────────────

    void reset() {
        hierarchy_.clear();
        imem_.reset();
        dmem_.reset();
    }

private:
    MemHierarchy hierarchy_;
    IOBus& io_bus_;
    MemoryPort imem_;
    MemoryPort dmem_;
};
