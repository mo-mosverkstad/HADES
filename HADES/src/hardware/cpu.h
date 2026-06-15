#pragma once
#include <cstdint>
#include <vector>
#include "cpu_base.h"

class CPU : public CPUBase<CPU> {
public:
    CPU();

    void run(uint32_t max_instructions = 1000000);
    void reset();

    uint64_t get_cycles() const;

private:
    uint64_t cycles_;

    void execute(uint32_t instr);
};
