#pragma once
#include <cstdint>
#include <vector>
#include <cstring>

class Memory {
public:
    static constexpr uint32_t SIZE = 65536; // 64KB

    Memory() : data_(SIZE, 0) {}

    uint8_t read_byte(uint32_t addr) const {
        return data_[addr & (SIZE - 1)];
    }

    uint16_t read_half(uint32_t addr) const {
        uint16_t val = 0;
        val |= (uint16_t)data_[(addr + 0) & (SIZE - 1)];
        val |= (uint16_t)data_[(addr + 1) & (SIZE - 1)] << 8;
        return val;
    }

    uint32_t read_word(uint32_t addr) const {
        uint32_t val = 0;
        val |= (uint32_t)data_[(addr + 0) & (SIZE - 1)];
        val |= (uint32_t)data_[(addr + 1) & (SIZE - 1)] << 8;
        val |= (uint32_t)data_[(addr + 2) & (SIZE - 1)] << 16;
        val |= (uint32_t)data_[(addr + 3) & (SIZE - 1)] << 24;
        return val;
    }

    void write_byte(uint32_t addr, uint8_t val) {
        data_[addr & (SIZE - 1)] = val;
    }

    void write_half(uint32_t addr, uint16_t val) {
        data_[(addr + 0) & (SIZE - 1)] = val & 0xFF;
        data_[(addr + 1) & (SIZE - 1)] = (val >> 8) & 0xFF;
    }

    void write_word(uint32_t addr, uint32_t val) {
        data_[(addr + 0) & (SIZE - 1)] = val & 0xFF;
        data_[(addr + 1) & (SIZE - 1)] = (val >> 8) & 0xFF;
        data_[(addr + 2) & (SIZE - 1)] = (val >> 16) & 0xFF;
        data_[(addr + 3) & (SIZE - 1)] = (val >> 24) & 0xFF;
    }

    void load(uint32_t base_addr, const std::vector<uint8_t>& bytes) {
        for (size_t i = 0; i < bytes.size(); i++) {
            data_[(base_addr + i) & (SIZE - 1)] = bytes[i];
        }
    }

    std::vector<uint8_t> dump(uint32_t addr, uint32_t len) const {
        std::vector<uint8_t> out(len);
        for (uint32_t i = 0; i < len; i++) {
            out[i] = data_[(addr + i) & (SIZE - 1)];
        }
        return out;
    }

    void clear() { std::fill(data_.begin(), data_.end(), 0); }

private:
    std::vector<uint8_t> data_;
};