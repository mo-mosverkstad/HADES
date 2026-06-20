#pragma once
#include "io_bus.h"
#include <vector>
#include <cstring>

// DTEK-V VGA Display (simplified)
//
// Two modes:
//   1. Pixel buffer: 320×240, RGB565 (16-bit per pixel)
//   2. Character buffer: 80×60, ASCII (8-bit per character)
//
// Memory-mapped at base address (registered on I/O bus):
//   Offset 0x00: CONTROL register
//     [0] MODE: 0=pixel, 1=character
//     [1] ENABLE: display on/off
//   Offset 0x04: STATUS register (read-only)
//     [0] VSYNC: vertical sync pulse (toggled each frame)
//   Offset 0x08: CURSOR_X (character mode)
//   Offset 0x0C: CURSOR_Y (character mode)
//   Offset 0x10: PIXEL_ADDR (write pixel at this address)
//   Offset 0x14: PIXEL_DATA (RGB565 value to write)
//   Offset 0x18: CHAR_WRITE (write ASCII char at cursor, auto-advance)
//
// For bulk pixel access, the framebuffer is also accessible via
// a separate memory region (0xF100-0xFFFF in HADES simplified map).
// But for simplicity, we use register-based pixel writes.

class VGA : public IODevice {
public:
    static constexpr uint32_t WIDTH = 320;
    static constexpr uint32_t HEIGHT = 240;
    static constexpr uint32_t CHAR_COLS = 80;
    static constexpr uint32_t CHAR_ROWS = 60;

    VGA() { reset(); }

    void reset() {
        pixels_.assign(WIDTH * HEIGHT, 0);
        chars_.assign(CHAR_COLS * CHAR_ROWS, ' ');
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
            case 0x14: // Read pixel at pixel_addr_
                if (pixel_addr_ < WIDTH * HEIGHT)
                    return pixels_[pixel_addr_];
                return 0;
            case 0x18: // Read char at cursor
                if (cursor_y_ < CHAR_ROWS && cursor_x_ < CHAR_COLS)
                    return chars_[cursor_y_ * CHAR_COLS + cursor_x_];
                return 0;
            default: return 0;
        }
    }

    void write(uint32_t offset, uint32_t value) override {
        switch (offset) {
            case 0x00: // CONTROL
                mode_char_ = (value & 1) != 0;
                enabled_ = (value & 2) != 0;
                break;
            case 0x08: // CURSOR_X
                cursor_x_ = value % CHAR_COLS;
                break;
            case 0x0C: // CURSOR_Y
                cursor_y_ = value % CHAR_ROWS;
                break;
            case 0x10: // PIXEL_ADDR
                pixel_addr_ = value % (WIDTH * HEIGHT);
                break;
            case 0x14: // PIXEL_DATA: write pixel at pixel_addr_, auto-increment
                if (pixel_addr_ < WIDTH * HEIGHT) {
                    pixels_[pixel_addr_] = (uint16_t)(value & 0xFFFF);
                    pixel_addr_++;
                }
                break;
            case 0x18: // CHAR_WRITE: write char at cursor, auto-advance
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
            default: break;
        }
    }

    void tick() override {
        tick_count_++;
        // Simulate VSYNC: toggle every ~76800 ticks (320*240 pixels at 1 pixel/tick)
        // Simplified: toggle every 1000 ticks
        if (tick_count_ % 1000 == 0) {
            vsync_ = !vsync_;
            frame_count_++;
        }
    }

    bool irq_pending() override {
        return false; // VGA doesn't generate interrupts in this model
    }

    // ─── Host-side access (Python API) ───

    // Get entire framebuffer as flat array (for rendering/export)
    std::vector<uint16_t> get_framebuffer() const { return pixels_; }

    // Get character buffer as string
    std::vector<uint8_t> get_char_buffer() const { return chars_; }

    // Get a single row of characters as string
    std::string get_char_row(uint32_t row) const {
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
    std::vector<uint16_t> pixels_;   // 320×240 RGB565
    std::vector<uint8_t> chars_;     // 80×60 ASCII
    bool mode_char_;
    bool enabled_;
    bool vsync_;
    uint32_t cursor_x_;
    uint32_t cursor_y_;
    uint32_t pixel_addr_;
    uint32_t frame_count_;
    uint64_t tick_count_;
};