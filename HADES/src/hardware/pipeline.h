#pragma once
#include <cstdint>

// 3-stage pipeline: IF/ID -> EX -> MEM/WB

/** IF/ID pipeline register: holds fetched instruction and its PC. */
struct StageIFID {
    uint32_t instr = 0;
    uint32_t pc = 0;
    bool valid = false;
};

/** EX pipeline register: holds ALU result and control signals. */
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

/** MEM/WB pipeline register: holds writeback value and control. */
struct StageMEMWB {
    uint32_t rd = 0;
    uint32_t result = 0;      // value to write back
    bool writes_rd = false;
    bool is_load = false;
    bool valid = false;
};

/** Performance counters accessible via CSR instructions. */
struct PerfCounters {
    uint64_t mcycle = 0;        // total cycles
    uint64_t minstret = 0;      // retired instructions
    uint64_t stalls_data = 0;   // data hazard stalls (load-use)
    uint64_t stalls_branch = 0; // branch penalty cycles

    /** Resets all counters to zero. */
    void reset() {
        mcycle = 0;
        minstret = 0;
        stalls_data = 0;
        stalls_branch = 0;
    }
};

/** CSR addresses (RISC-V standard + HADES extensions). */
enum CSR_Addr {
    CSR_MCYCLE      = 0xB00,   // cycle count (low 32)
    CSR_MINSTRET    = 0xB02,   // retired instructions (low 32)
    CSR_MCYCLEH     = 0xB80,   // cycle count (high 32)
    CSR_MINSTRETH   = 0xB82,   // retired instructions (high 32)
    // Custom performance counters
    CSR_MHPMCOUNTER3 = 0xB03,  // stalls_data
    CSR_MHPMCOUNTER4 = 0xB04,  // stalls_branch
    // Trap handling
    CSR_MTVEC       = 0x305,   // trap vector base address
    CSR_MEPC        = 0x341,   // exception PC (saved on interrupt)
    CSR_MCAUSE      = 0x342,   // trap cause code
    CSR_MTVAL       = 0x343,   // trap value (faulting address)
    CSR_SATP        = 0x180,   // address translation (MMU page table base)
};
