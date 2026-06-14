#pragma once
#include <cstdint>

// 3-stage pipeline: IF/ID → EX → MEM/WB
// Matches DTEK-V teaching processor

struct StageIFID {
    uint32_t instr = 0;
    uint32_t pc = 0;
    bool valid = false;
};

struct StageEX {
    uint32_t instr = 0;
    uint32_t pc = 0;
    uint32_t alu_result = 0;
    uint32_t rs2_val = 0;     // for stores
    uint32_t rd = 0;
    bool writes_rd = false;
    bool is_load = false;
    bool is_store = false;
    bool is_branch = false;
    bool branch_taken = false;
    uint32_t branch_target = 0;
    bool valid = false;
};

struct StageMEMWB {
    uint32_t rd = 0;
    uint32_t result = 0;      // value to write back
    bool writes_rd = false;
    bool is_load = false;
    bool valid = false;
};

// Performance counters (DTEK-V compatible CSRs)
struct PerfCounters {
    uint64_t mcycle = 0;        // total cycles
    uint64_t minstret = 0;      // retired instructions
    uint64_t stalls_data = 0;   // data hazard stalls (load-use)
    uint64_t stalls_branch = 0; // branch penalty cycles

    void reset() {
        mcycle = 0;
        minstret = 0;
        stalls_data = 0;
        stalls_branch = 0;
    }
};

// CSR addresses (RISC-V standard)
enum CSR_Addr {
    CSR_MCYCLE      = 0xB00,
    CSR_MINSTRET    = 0xB02,
    CSR_MCYCLEH     = 0xB80,
    CSR_MINSTRETH   = 0xB82,
    // Custom performance counters
    CSR_MHPMCOUNTER3 = 0xB03,  // stalls_data
    CSR_MHPMCOUNTER4 = 0xB04,  // stalls_branch
};