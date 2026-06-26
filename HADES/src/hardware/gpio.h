#pragma once
#include "io_bus.h"
#include <atomic>

// DTEK-V GPIO (Thread-Safe)
// CPU thread calls read()/write() via I/O bus.
// Main thread calls set_input()/get_output() from Python.
// input_pins_ and data_out_ are atomic for cross-thread access.

/**
 * General-purpose I/O with edge detection and interrupt support.
 * Registers: DATA(0x00), DIRECTION(0x04), INTERRUPTMASK(0x08), EDGECAPTURE(0x0C).
 * IRQ fires when (edge_capture & interrupt_mask) != 0.
 */
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

    // Set input pin values (called from Python/main thread)
    void set_input(uint32_t value) {
        input_pins_.store(value, std::memory_order_relaxed);
    }

    // Read output pin values (called from Python/main thread)
    uint32_t get_output() const {
        return data_out_.load(std::memory_order_relaxed);
    }

    uint32_t read(uint32_t offset) override {
        switch (offset) {
            case 0x00: return input_pins_.load(std::memory_order_relaxed);
            case 0x04: return direction_;
            case 0x08: return interrupt_mask_;
            case 0x0C: return edge_capture_;
            default: return 0;
        }
    }

    void write(uint32_t offset, uint32_t value) override {
        switch (offset) {
            case 0x00: data_out_.store(value, std::memory_order_relaxed); break;
            case 0x04: direction_ = value; break;
            case 0x08: interrupt_mask_ = value; break;
            case 0x0C: edge_capture_ &= ~value; break;
            default: break;
        }
    }

    void tick() override {
        uint32_t cur = input_pins_.load(std::memory_order_relaxed);
        uint32_t changed = cur ^ prev_input_;
        edge_capture_ |= changed;
        prev_input_ = cur;
    }

    bool irq_pending() override {
        return (edge_capture_ & interrupt_mask_) != 0;
    }

private:
    std::atomic<uint32_t> input_pins_{0};
    std::atomic<uint32_t> data_out_{0};
    uint32_t direction_ = 0;
    uint32_t interrupt_mask_ = 0;
    uint32_t edge_capture_ = 0;
    uint32_t prev_input_ = 0;
};
