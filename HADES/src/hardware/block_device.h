#pragma once
#include "io_bus.h"
#include <vector>
#include <cstring>

// Block Storage Device (Disk)
//
// A simple sector-based storage device for CERBERUS file system support.
// Simulates a disk with 512-byte sectors and DMA-style transfer.
//
// I/O Registers (offsets from base):
//   0x00  COMMAND:   Write: 0=NOP, 1=READ, 2=WRITE. Read: last command.
//   0x04  SECTOR:    Sector number (0-based, 512 bytes each)
//   0x08  BUFFER:    RAM address for data transfer (DMA target/source)
//   0x0C  STATUS:    0=idle, 1=busy, 2=done, 3=error
//   0x10  DISK_SIZE: Read-only: total number of sectors
//   0x14  LATENCY:   Read/write: access latency in cycles (simulates seek)
//
// Operation:
//   1. CPU writes SECTOR number
//   2. CPU writes BUFFER address (where in RAM to put/get data)
//   3. CPU writes COMMAND (1=READ or 2=WRITE)
//   4. Device transfers 512 bytes between disk and RAM
//   5. STATUS becomes DONE (2) or ERROR (3)
//   6. Optionally triggers IRQ on completion
//
// The disk storage is backed by a std::vector (in-memory).
// Python can load/save disk images via host API.

class BlockDevice : public IODevice {
public:
    static constexpr uint32_t SECTOR_SIZE = 512;
    static constexpr uint32_t DEFAULT_SECTORS = 128;  // 64KB default disk
    static constexpr uint32_t CMD_NOP = 0;
    static constexpr uint32_t CMD_READ = 1;
    static constexpr uint32_t CMD_WRITE = 2;
    static constexpr uint32_t STATUS_IDLE = 0;
    static constexpr uint32_t STATUS_BUSY = 1;
    static constexpr uint32_t STATUS_DONE = 2;
    static constexpr uint32_t STATUS_ERROR = 3;

    // mem_read/mem_write function pointers for DMA transfer
    using MemWriteFn = void(*)(void* ctx, uint32_t addr, uint8_t val);
    using MemReadFn = uint8_t(*)(void* ctx, uint32_t addr);

    BlockDevice() { reset(); }

    void reset() {
        disk_.assign(DEFAULT_SECTORS * SECTOR_SIZE, 0);
        command_ = CMD_NOP;
        sector_ = 0;
        buffer_addr_ = 0;
        status_ = STATUS_IDLE;
        latency_ = 10;  // default 10 cycles per operation
        latency_counter_ = 0;
        irq_enable_ = false;
        irq_pending_ = false;
        reads_ = 0;
        writes_ = 0;
        mem_ctx_ = nullptr;
        mem_write_fn_ = nullptr;
        mem_read_fn_ = nullptr;
    }

    // Set memory access callbacks (for DMA transfer)
    void set_mem_callbacks(void* ctx, MemReadFn read_fn, MemWriteFn write_fn) {
        mem_ctx_ = ctx;
        mem_read_fn_ = read_fn;
        mem_write_fn_ = write_fn;
    }

    uint32_t read(uint32_t offset) override {
        switch (offset) {
            case 0x00: return command_;
            case 0x04: return sector_;
            case 0x08: return buffer_addr_;
            case 0x0C: return status_;
            case 0x10: return (uint32_t)(disk_.size() / SECTOR_SIZE);
            case 0x14: return latency_;
            default: return 0;
        }
    }

    void write(uint32_t offset, uint32_t value) override {
        switch (offset) {
            case 0x00: // COMMAND
                command_ = value;
                if (value == CMD_READ || value == CMD_WRITE) {
                    execute_command();
                }
                break;
            case 0x04: sector_ = value; break;
            case 0x08: buffer_addr_ = value; break;
            case 0x0C: // STATUS: write clears done/error
                if (value == 0) { status_ = STATUS_IDLE; irq_pending_ = false; }
                break;
            case 0x14: latency_ = value; break;
            default: break;
        }
    }

    void tick() override {
        if (latency_counter_ > 0) {
            latency_counter_--;
            if (latency_counter_ == 0) {
                status_ = STATUS_DONE;
                if (irq_enable_) irq_pending_ = true;
            }
        }
    }

    bool irq_pending() override { return irq_pending_; }

    // ─── Host-side API (Python) ───

    // Load disk image (raw bytes, multiple of 512)
    void load_image(const std::vector<uint8_t>& data) {
        disk_ = data;
        // Pad to sector boundary
        while (disk_.size() % SECTOR_SIZE != 0) disk_.push_back(0);
    }

    // Get disk contents
    std::vector<uint8_t> save_image() const { return disk_; }

    // Resize disk (number of sectors)
    void resize(uint32_t num_sectors) {
        disk_.resize(num_sectors * SECTOR_SIZE, 0);
    }

    // Direct sector read (from Python, bypasses DMA)
    std::vector<uint8_t> read_sector(uint32_t sector) const {
        if (sector * SECTOR_SIZE >= disk_.size()) return std::vector<uint8_t>(SECTOR_SIZE, 0);
        return std::vector<uint8_t>(
            disk_.begin() + sector * SECTOR_SIZE,
            disk_.begin() + (sector + 1) * SECTOR_SIZE);
    }

    // Direct sector write (from Python, bypasses DMA)
    void write_sector(uint32_t sector, const std::vector<uint8_t>& data) {
        if (sector * SECTOR_SIZE >= disk_.size()) return;
        size_t len = std::min((size_t)SECTOR_SIZE, data.size());
        std::memcpy(&disk_[sector * SECTOR_SIZE], data.data(), len);
    }

    // Stats
    uint64_t get_reads() const { return reads_; }
    uint64_t get_writes() const { return writes_; }

private:
    std::vector<uint8_t> disk_;
    uint32_t command_;
    uint32_t sector_;
    uint32_t buffer_addr_;
    uint32_t status_;
    uint32_t latency_;
    uint32_t latency_counter_;
    bool irq_enable_;
    bool irq_pending_;
    uint64_t reads_;
    uint64_t writes_;

    // Memory access for DMA
    void* mem_ctx_;
    MemReadFn mem_read_fn_;
    MemWriteFn mem_write_fn_;

    void execute_command() {
        uint32_t disk_offset = sector_ * SECTOR_SIZE;

        if (disk_offset + SECTOR_SIZE > disk_.size()) {
            status_ = STATUS_ERROR;
            return;
        }

        if (command_ == CMD_READ && mem_write_fn_) {
            // DMA: disk → RAM
            for (uint32_t i = 0; i < SECTOR_SIZE; i++) {
                mem_write_fn_(mem_ctx_, buffer_addr_ + i, disk_[disk_offset + i]);
            }
            reads_++;
        } else if (command_ == CMD_WRITE && mem_read_fn_) {
            // DMA: RAM → disk
            for (uint32_t i = 0; i < SECTOR_SIZE; i++) {
                disk_[disk_offset + i] = mem_read_fn_(mem_ctx_, buffer_addr_ + i);
            }
            writes_++;
        } else {
            // No DMA callbacks: just mark done (Python will handle transfer)
            if (command_ == CMD_READ) reads_++;
            if (command_ == CMD_WRITE) writes_++;
        }

        // Start latency countdown
        if (latency_ > 0) {
            status_ = STATUS_BUSY;
            latency_counter_ = latency_;
        } else {
            status_ = STATUS_DONE;
            if (irq_enable_) irq_pending_ = true;
        }
    }
};