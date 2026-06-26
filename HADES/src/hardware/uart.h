#pragma once
#include "io_bus.h"
#include <queue>
#include <vector>
#include <mutex>

// DTEK-V JTAG UART (Thread-Safe)
// CPU thread calls read()/write() via I/O bus.
// Main thread calls host_send()/host_recv() from Python.
// rx_mutex_ protects rx_fifo_, tx_mutex_ protects tx_output_.

/**
 * UART (serial I/O) with 64-byte RX/TX FIFOs.
 * Registers: DATA(0x00), CONTROL(0x04).
 * Host sends to RX FIFO via host_send(), CPU reads via DATA register.
 * CPU writes to DATA register -> TX FIFO, host reads via host_recv().
 */
class UART : public IODevice {
public:
    static constexpr uint32_t FIFO_SIZE = 64;

    UART() { reset(); }

    void reset() {
        {
            std::lock_guard<std::mutex> lk(rx_mutex_);
            while (!rx_fifo_.empty()) rx_fifo_.pop();
        }
        {
            std::lock_guard<std::mutex> lk(tx_mutex_);
            tx_output_.clear();
        }
        ctrl_re_ = false;
        ctrl_we_ = false;
    }

    // Host -> CPU: push bytes into RX FIFO (called from Python/main thread)
    void host_send(const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lk(rx_mutex_);
        for (uint8_t b : data) {
            if (rx_fifo_.size() < FIFO_SIZE) rx_fifo_.push(b);
        }
    }

    // CPU -> Host: read TX output buffer (called from Python/main thread)
    std::vector<uint8_t> host_recv() {
        std::lock_guard<std::mutex> lk(tx_mutex_);
        std::vector<uint8_t> out;
        out.swap(tx_output_);
        return out;
    }

    uint32_t read(uint32_t offset) override {
        switch (offset) {
            case 0x00: {
                std::lock_guard<std::mutex> lk(rx_mutex_);
                if (rx_fifo_.empty()) return 0;
                uint8_t val = rx_fifo_.front();
                rx_fifo_.pop();
                uint32_t ravail = (uint32_t)rx_fifo_.size();
                return val | (1 << 15) | (ravail << 16);
            }
            case 0x04: {
                std::lock_guard<std::mutex> lk(rx_mutex_);
                uint32_t ctrl = 0;
                ctrl |= ctrl_re_ ? 1 : 0;
                ctrl |= ctrl_we_ ? 2 : 0;
                if (!rx_fifo_.empty()) ctrl |= (1 << 8);
                ctrl |= (1 << 9);
                ctrl |= (1 << 10);
                uint32_t wspace;
                { std::lock_guard<std::mutex> lk2(tx_mutex_);
                  wspace = FIFO_SIZE - (uint32_t)tx_output_.size(); }
                ctrl |= (wspace << 16);
                return ctrl;
            }
            default: return 0;
        }
    }

    void write(uint32_t offset, uint32_t value) override {
        switch (offset) {
            case 0x00: {
                std::lock_guard<std::mutex> lk(tx_mutex_);
                if (tx_output_.size() < FIFO_SIZE)
                    tx_output_.push_back(value & 0xFF);
                break;
            }
            case 0x04:
                ctrl_re_ = (value & 1) != 0;
                ctrl_we_ = (value & 2) != 0;
                break;
            default: break;
        }
    }

    void tick() override {}

    bool irq_pending() override {
        std::lock_guard<std::mutex> lk(rx_mutex_);
        return ctrl_re_ && !rx_fifo_.empty();
    }

private:
    std::queue<uint8_t> rx_fifo_;
    std::vector<uint8_t> tx_output_;
    std::mutex rx_mutex_;
    std::mutex tx_mutex_;
    bool ctrl_re_ = false;
    bool ctrl_we_ = false;
};
