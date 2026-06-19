
#pragma once
#include "io_bus.h"
#include <queue>
#include <vector>

// DTEK-V JTAG UART
// Registers (offsets from base):
//   0x00 DATA:    Read: [7:0] data, [15] RVALID (valid packet or not), [31:16] RAVAIL (how many more bytes to read)
//                 Write: [7:0] data to TX FIFO
//   0x04 CONTROL: [0] RE (read IRQ enable), [1] WE (write IRQ enable),
//                 [8] RI (read IRQ pending), [9] WI (write IRQ pending),
//                 [10] AC (activity), [31:16] WSPACE

class UART : public IODevice {
public:
    static constexpr uint32_t FIFO_SIZE = 64;

    UART() { reset(); }

    void reset() {
        while (!rx_fifo_.empty()) rx_fifo_.pop();
        tx_output_.clear();
        ctrl_re_ = false;
        ctrl_we_ = false;
    }

    // Host -> CPU: push bytes into RX FIFO (called from Python)
    void host_send(const std::vector<uint8_t>& data) {
        for (uint8_t b : data) {
            if (rx_fifo_.size() < FIFO_SIZE) {
                rx_fifo_.push(b);
            }
        }
    }

    // CPU -> Host: read TX output buffer (called from Python)
    std::vector<uint8_t> host_recv() {
        std::vector<uint8_t> out = tx_output_;
        tx_output_.clear();
        return out;
    }

    uint32_t read(uint32_t offset) override {
        switch (offset) {
            case 0x00: { // DATA register read
                if (rx_fifo_.empty()) {
                    return 0; // RVALID = 0, no data
                }
                uint8_t val = rx_fifo_.front();
                rx_fifo_.pop();
                uint32_t ravail = (uint32_t)rx_fifo_.size();
                return val | (1 << 15) | (ravail << 16); // RVALID=1
            }
            case 0x04: { // CONTROL register read
                uint32_t ctrl = 0;
                ctrl |= ctrl_re_ ? 1 : 0;
                ctrl |= ctrl_we_ ? 2 : 0;
                if (!rx_fifo_.empty()) ctrl |= (1 << 8);  // RI
                ctrl |= (1 << 9);  // WI (always ready to write)
                ctrl |= (1 << 10); // AC (activity = host connected)
                uint32_t wspace = FIFO_SIZE - (uint32_t)tx_output_.size();
                ctrl |= (wspace << 16);
                return ctrl;
            }
            default: return 0;
        }
    }

    void write(uint32_t offset, uint32_t value) override {
        switch (offset) {
            case 0x00: // DATA register write → TX
                if (tx_output_.size() < FIFO_SIZE) {
                    tx_output_.push_back(value & 0xFF);
                }
                break;
            case 0x04: // CONTROL register write
                ctrl_re_ = (value & 1) != 0;
                ctrl_we_ = (value & 2) != 0;
                break;
            default: break;
        }
    }

    void tick() override {
        // UART doesn't need per-cycle tick in this simplified model
    }

    bool irq_pending() override {
        if (ctrl_re_ && !rx_fifo_.empty()) return true;
        return false;
    }

private:
    std::queue<uint8_t> rx_fifo_;     // host → CPU
    std::vector<uint8_t> tx_output_;  // CPU → host
    bool ctrl_re_;
    bool ctrl_we_;
};