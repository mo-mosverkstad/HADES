#pragma once
#include <cstdint>
#include <vector>
#include <utility>

/**
 * Base class for all memory-mapped I/O devices.
 * Each device handles reads/writes at offsets from its base address.
 */
class IODevice {
public:
    virtual ~IODevice() = default;

    /** Reads a 32-bit register at the given offset from device base. */
    virtual uint32_t read(uint32_t offset) = 0;

    /** Writes a 32-bit value to the register at offset from device base. */
    virtual void write(uint32_t offset, uint32_t value) = 0;

    /** Called once per CPU cycle to advance device state (timers, FIFOs, etc). */
    virtual void tick() = 0;

    /** Returns true if this device is asserting an interrupt request. */
    virtual bool irq_pending() = 0;
};

/**
 * I/O Bus: routes memory-mapped addresses (>= 0xF000) to registered devices.
 * Each device occupies a 32-byte region starting at its base address.
 */
class IOBus {
public:
    static constexpr uint32_t IO_BASE = 0xF000;

    IOBus() = default;

    /** Registers a device at base_addr. Device handles offsets 0x00-0x1F. */
    void register_device(uint32_t base_addr, IODevice* dev) {
        devices_.push_back({base_addr, dev});
    }

    /** Returns true if addr falls in the I/O region (0xF000-0xFFFF). */
    bool is_io_address(uint32_t addr) const {
        return addr >= IO_BASE && addr <= 0xFFFF;
    }

    /** Reads from the device mapped at addr. Returns 0 if unmapped. */
    uint32_t read(uint32_t addr) {
        for (auto& [base, dev] : devices_) {
            if (addr >= base && addr < base + 0x20) {
                return dev->read(addr - base);
            }
        }
        return 0; // unmapped
    }

    /** Writes value to the device mapped at addr. No-op if unmapped. */
    void write(uint32_t addr, uint32_t value) {
        for (auto& [base, dev] : devices_) {
            if (addr >= base && addr < base + 0x20) {
                dev->write(addr - base, value);
                return;
            }
        }
    }

    /** Ticks all registered devices (called once per CPU cycle). */
    void tick_all() {
        for (auto& [base, dev] : devices_) {
            dev->tick();
        }
    }

    /** Returns true if any registered device has a pending IRQ. */
    bool any_irq_pending() const {
        for (auto& [base, dev] : devices_) {
            if (dev->irq_pending()) return true;
        }
        return false;
    }

private:
    std::vector<std::pair<uint32_t, IODevice*>> devices_;
};
