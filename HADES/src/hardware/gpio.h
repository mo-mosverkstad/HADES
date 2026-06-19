#pragma once
#include "io_bus.h"

// DTEK-V GPIO (Parallel I/O)
// Registers (offsets from base):
//   0x00 DATA:          Read: input pins. Write: output pins.
//   0x04 DIRECTION:     0=input, 1=output per bit
//   0x08 INTERRUPTMASK: 1=enable IRQ for that bit
//   0x0C EDGECAPTURE:   1=edge detected. Write 1 to clear.

class GPIO : public IODevice {
public:
    GPIO() { reset(); }

    void reset() {
        data_out_ = 0;
        direction_ = 0;
        interrupt_mask_ = 0;
        edge_capture_ = 0;
        input_pins_ = 0;
        prev_input_ = 0;
    }

    // Set input pin values (called from Python, simulates external signals)
    void set_input(uint32_t value) {
        prev_input_ = input_pins_;
        input_pins_ = value;
    }

    // Read output pin values (called from Python)
    uint32_t get_output() const { return data_out_; }

    uint32_t read(uint32_t offset) override {
        switch (offset) {
            case 0x00: // DATA: read returns input pins (not output)
                return input_pins_;
            case 0x04:
                return direction_;
            case 0x08:
                return interrupt_mask_;
            case 0x0C:
                return edge_capture_;
            default: return 0;
        }
    }

    void write(uint32_t offset, uint32_t value) override {
        switch (offset) {
            case 0x00: // DATA: write sets output pins
                data_out_ = value;
                break;
            case 0x04:
                direction_ = value;
                break;
            case 0x08:
                interrupt_mask_ = value;
                break;
            case 0x0C: // EDGECAPTURE: write 1 to clear bits
                edge_capture_ &= ~value;
                break;
            default: break;
        }
    }

    void tick() override {
        // Detect edges (rising or falling) on input pins
        uint32_t changed = input_pins_ ^ prev_input_;
        edge_capture_ |= changed;
        prev_input_ = input_pins_;
    }

    bool irq_pending() override {
        return (edge_capture_ & interrupt_mask_) != 0;
    }

private:
    uint32_t data_out_;
    uint32_t direction_;
    uint32_t interrupt_mask_;
    uint32_t edge_capture_;
    uint32_t input_pins_;
    uint32_t prev_input_;
};