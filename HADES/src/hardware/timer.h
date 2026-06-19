#pragma once
#include "io_bus.h"

// DTEK-V Interval Timer
// Registers (offsets from base):
//   0x00 STATUS:    [0] TO (timeout occurred), write 0 to clear
//   0x04 CONTROL:   [0] ITO (IRQ enable), [1] CONT, [2] START, [3] STOP
//   0x08 PERIOD_LO: low 32 bits of period
//   0x0C PERIOD_HI: high 32 bits of period (unused in 32-bit mode)
//   0x10 SNAP_LO:   snapshot low (write to capture current counter)
//   0x14 SNAP_HI:   snapshot high

class Timer : public IODevice {
public:
    Timer() { reset(); }

    void reset() {
        status_to_ = false;
        ctrl_ito_ = false;
        ctrl_cont_ = false;
        running_ = false;
        period_ = 0;
        counter_ = 0;
        snapshot_ = 0;
    }

    uint32_t read(uint32_t offset) override {
        switch (offset) {
            case 0x00: return status_to_ ? 1 : 0;
            case 0x04: return (ctrl_ito_ ? 1 : 0) | (ctrl_cont_ ? 2 : 0) |
                              (running_ ? 4 : 0);
            case 0x08: return (uint32_t)(period_ & 0xFFFFFFFF);
            case 0x0C: return (uint32_t)(period_ >> 32);
            case 0x10: return (uint32_t)(snapshot_ & 0xFFFFFFFF);
            case 0x14: return (uint32_t)(snapshot_ >> 32);
            default: return 0;
        }
    }

    void write(uint32_t offset, uint32_t value) override {
        switch (offset) {
            case 0x00: // STATUS: write 0 to clear TO
                if (value == 0) status_to_ = false;
                break;
            case 0x04: // CONTROL
                ctrl_ito_ = (value & 1) != 0;
                ctrl_cont_ = (value & 2) != 0;
                if (value & 4) { // START
                    running_ = true;
                    counter_ = period_;
                }
                if (value & 8) { // STOP
                    running_ = false;
                }
                break;
            case 0x08: // PERIOD_LO
                period_ = (period_ & 0xFFFFFFFF00000000ULL) | value;
                break;
            case 0x0C: // PERIOD_HI
                period_ = (period_ & 0xFFFFFFFF) | ((uint64_t)value << 32);
                break;
            case 0x10: // SNAP_LO: writing triggers snapshot capture
                snapshot_ = counter_;
                break;
            default: break;
        }
    }

    void tick() override {
        if (!running_) return;
        if (counter_ == 0) {
            status_to_ = true;
            if (ctrl_cont_) {
                counter_ = period_; // reload
            } else {
                running_ = false;
            }
        } else {
            counter_--;
        }
    }

    bool irq_pending() override {
        return status_to_ && ctrl_ito_;
    }

private:
    bool status_to_;
    bool ctrl_ito_;
    bool ctrl_cont_;
    bool running_;
    uint64_t period_;
    uint64_t counter_;
    uint64_t snapshot_;
};