#pragma once
#include "io_bus.h"
#include <vector>
#include <string>
#include <mutex>

// DTEK-V VGA Display (Thread-Safe)
// CPU thread calls read()/write() via I/O bus.
// Main thread calls get_char_row()/get_framebuffer() from Python.
// char_mutex_ protects chars_[], pixel_mutex_ protects pixels_[].

class VGA : public IODevice {
public:
    static constexpr uint32_t WIDTH = 320;
    static constexpr uint32_t HEIGHT = 240;
    static constexpr uint32_t CHAR_COLS = 80;
    static constexpr uint32_t CHAR_ROWS = 60;

    VGA() { reset(); }

    void reset() {
        {
            std::lock_guard<std::mutex> lk(pixel_mutex_);
            pixels_.assign(WIDTH * HEIGHT, 0);
        }
        {
            std::lock_guard<std::mutex> lk(char_mutex_);
            chars_.assign(CHAR_COLS * CHAR_ROWS, ' ');
        }
        mode_char_ = false;
        enabled_ = false;
        vsync_ = false;
        cursor_x_ = 0;
        cursor_y_ = 0;
        pixel_addr_ = 0;
        frame_count_ = 0;
        tick_count_ = 0;
    }

    uint32_t read(uint32_t offset) override {
        switch (offset) {
            case 0x00: return (mode_char_ ? 1 : 0) | (enabled_ ? 2 : 0);
            case 0x04: return vsync_ ? 1 : 0;
            case 0x08: return cursor_x_;
            case 0x0C: return cursor_y_;
            case 0x10: return pixel_addr_;
            case 0x14: {
                std::lock_guard<std::mutex> lk(pixel_mutex_);
                return (pixel_addr_ < WIDTH * HEIGHT) ? pixels_[pixel_addr_] : 0;
            }
            case 0x18: {
                std::lock_guard<std::mutex> lk(char_mutex_);
                if (cursor_y_ < CHAR_ROWS && cursor_x_ < CHAR_COLS)
                    return chars_[cursor_y_ * CHAR_COLS + cursor_x_];
                return 0;
            }
            default: return 0;
        }
    }

    void write(uint32_t offset, uint32_t value) override {
        switch (offset) {
            case 0x00:
                mode_char_ = (value & 1) != 0;
                enabled_ = (value & 2) != 0;
                break;
            case 0x08: cursor_x_ = value % CHAR_COLS; break;
            case 0x0C: cursor_y_ = value % CHAR_ROWS; break;
            case 0x10: pixel_addr_ = value % (WIDTH * HEIGHT); break;
            case 0x14: {
                std::lock_guard<std::mutex> lk(pixel_mutex_);
                if (pixel_addr_ < WIDTH * HEIGHT) {
                    pixels_[pixel_addr_] = (uint16_t)(value & 0xFFFF);
                    pixel_addr_++;
                }
                break;
            }
            case 0x18: {
                std::lock_guard<std::mutex> lk(char_mutex_);
                if (cursor_y_ < CHAR_ROWS && cursor_x_ < CHAR_COLS) {
                    chars_[cursor_y_ * CHAR_COLS + cursor_x_] = (uint8_t)(value & 0x7F);
                    cursor_x_++;
                    if (cursor_x_ >= CHAR_COLS) {
                        cursor_x_ = 0;
                        cursor_y_++;
                        if (cursor_y_ >= CHAR_ROWS) cursor_y_ = 0;
                    }
                }
                break;
            }
            default: break;
        }
    }

    void tick() override {
        tick_count_++;
        if (tick_count_ % 1000 == 0) {
            vsync_ = !vsync_;
            frame_count_++;
        }
    }

    bool irq_pending() override { return false; }

    // ─── Host-side access (Python/main thread) ───

    std::vector<uint16_t> get_framebuffer() const {
        std::lock_guard<std::mutex> lk(pixel_mutex_);
        return pixels_;
    }

    std::vector<uint8_t> get_char_buffer() const {
        std::lock_guard<std::mutex> lk(char_mutex_);
        return chars_;
    }

    std::string get_char_row(uint32_t row) const {
        std::lock_guard<std::mutex> lk(char_mutex_);
        if (row >= CHAR_ROWS) return "";
        std::string s(CHAR_COLS, ' ');
        for (uint32_t x = 0; x < CHAR_COLS; x++) {
            char c = (char)chars_[row * CHAR_COLS + x];
            s[x] = (c >= 32 && c < 127) ? c : ' ';
        }
        return s;
    }

    uint32_t get_frame_count() const { return frame_count_; }

private:
    std::vector<uint16_t> pixels_;
    std::vector<uint8_t> chars_;
    mutable std::mutex pixel_mutex_;
    mutable std::mutex char_mutex_;
    bool mode_char_ = false;
    bool enabled_ = false;
    bool vsync_ = false;
    uint32_t cursor_x_ = 0;
    uint32_t cursor_y_ = 0;
    uint32_t pixel_addr_ = 0;
    uint32_t frame_count_ = 0;
    uint64_t tick_count_ = 0;
};
