#pragma once
#include <concepts>
#include <cstdint>
#include <vector>
#include "cpu.h"
#include "pipelined_cpu.h"

template<typename T>
concept CPULike = requires(T cpu, std::vector<uint8_t> bin, uint32_t addr, uint32_t len) {
    { cpu.load_program(bin, addr) } -> std::same_as<void>;
    { cpu.load_data(bin, addr) } -> std::same_as<void>;
    { cpu.run(1u) } -> std::same_as<void>;
    { cpu.reset() } -> std::same_as<void>;
    { cpu.get_cycles() } -> std::same_as<uint64_t>;
    { cpu.get_pc() } -> std::same_as<uint32_t>;
    { cpu.get_reg(0u) } -> std::same_as<uint32_t>;
    { cpu.read_mem(addr, len) } -> std::same_as<std::vector<uint8_t>>;
};

static_assert(CPULike<CPU>);
static_assert(CPULike<PipelinedCPU>);