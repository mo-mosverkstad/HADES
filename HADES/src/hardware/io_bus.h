#pragma once
#include <cstdint>
#include <vector>
#include <utility>

// Base class for all memory-mapped I/O devices
class IODevice {
public:
    virtual ~IODevice() = default;
    virtual uint32_t read(uint32_t offset) = 0;
    virtual void write(uint32_t offset, uint32_t value) = 0;
    virtual void tick() = 0;           // called each CPU cycle
    virtual bool irq_pending() = 0;    // does this device want to interrupt?
};

// I/O Bus: routes memory-mapped addresses to registered devices
// DTEK-V I/O region starts at 0x40000000, but for HADES simplified map
// we use addresses >= 0xF000 (within our 64KB space)
class IOBus {
public:
    static constexpr uint32_t IO_BASE = 0xF000;

    IOBus() = default;

    void register_device(uint32_t base_addr, IODevice* dev) {
        devices_.push_back({base_addr, dev});
    }

    bool is_io_address(uint32_t addr) const {
        return addr >= IO_BASE;
    }

    uint32_t read(uint32_t addr) {
        for (auto& [base, dev] : devices_) {
            if (addr >= base && addr < base + 0x20) {
                return dev->read(addr - base);
            }
        }
        return 0; // unmapped
    }

    void write(uint32_t addr, uint32_t value) {
        for (auto& [base, dev] : devices_) {
            if (addr >= base && addr < base + 0x20) {
                dev->write(addr - base, value);
                return;
            }
        }
    }

    void tick_all() {
        for (auto& [base, dev] : devices_) {
            dev->tick();
        }
    }

    bool any_irq_pending() const {
        for (auto& [base, dev] : devices_) {
            if (dev->irq_pending()) return true;
        }
        return false;
    }

private:
    std::vector<std::pair<uint32_t, IODevice*>> devices_;
};