#include "pipelined_cpu.h"
#include <iostream>

// RV32I opcode constants
enum RV32_Opcode {
    OP_LUI    = 0b0110111,
    OP_AUIPC  = 0b0010111,
    OP_JAL    = 0b1101111,
    OP_JALR   = 0b1100111,
    OP_BRANCH = 0b1100011,
    OP_LOAD   = 0b0000011,
    OP_STORE  = 0b0100011,
    OP_IMM    = 0b0010011,
    OP_REG    = 0b0110011,
    OP_SYSTEM = 0b1110011,
};

PipelinedCPU::PipelinedCPU() { reset(); }

void PipelinedCPU::reset() {
    for (int i = 0; i < 32; i++) regs_[i] = 0;
    pc_ = 0x1000;
    halted_ = false;
    ifid_ = {};
    ex_ = {};
    memwb_ = {};
    perf_.reset();
    csrs_.clear();
    mem_.clear();
}

void PipelinedCPU::load_program(const std::vector<uint8_t>& binary, uint32_t base_addr) {
    mem_.load(base_addr, binary);
    pc_ = base_addr;
}

void PipelinedCPU::load_data(const std::vector<uint8_t>& data, uint32_t base_addr) {
    mem_.load(base_addr, data);
}

void PipelinedCPU::run(uint32_t max_instructions) {
    uint64_t max_cycles = (uint64_t)max_instructions * 4;
    while (perf_.mcycle < max_cycles) {
        pipeline_cycle();
        // Stop when halted and pipeline fully drained
        if (halted_ && !memwb_.valid) break;
        if (perf_.minstret >= max_instructions) break;
    }
}

// ─── Pipeline Cycle ─────────────────────────────────────────────────────

void PipelinedCPU::pipeline_cycle() {
    perf_.mcycle++;
    // Check for load-use hazard: stall IF/ID and EX, insert bubble
    if (detect_load_use_hazard()) {
        perf_.stalls_data++;
        // Writeback proceeds normally
        stage_writeback();
        // MEM/WB gets EX result (the load)
        stage_memory();
        // EX becomes bubble (don't advance IF/ID into EX)
        ex_ = {};
        // IF/ID stays the same (stall), PC doesn't advance
        return;
    }

    // Normal pipeline advance (back-to-front to avoid overwrite)
    stage_writeback();
    stage_memory();

    // If halted was set by previous EX (ECALL), don't fetch/execute more
    if (halted_){
        // drain the pipeline by clearing ex and ifid registers
        ex_ = {};
        ifid_ = {};
        return;
    }

    stage_execute();
    stage_fetch_decode();
}

// ─── Stage: Writeback ───────────────────────────────────────────────────

void PipelinedCPU::stage_writeback() {
    if (!memwb_.valid) return;
    if (memwb_.writes_rd && memwb_.rd != 0) {
        write_reg(memwb_.rd, memwb_.result);
    }
    perf_.minstret++;
}

// ─── Stage: Memory ──────────────────────────────────────────────────────

void PipelinedCPU::stage_memory() {
    memwb_ = {};
    if (!ex_.valid) return;

    memwb_.valid = true;
    memwb_.rd = ex_.rd;
    memwb_.writes_rd = ex_.writes_rd;
    memwb_.is_load = ex_.is_load;

    if (ex_.is_load) {
        // Perform memory read
        uint32_t addr = ex_.alu_result;
        uint32_t instr = ex_.instr;
        uint32_t f3 = (instr >> 12) & 0x7;
        uint32_t val = 0;
        switch (f3) {
            case 0b000: { int8_t b = (int8_t)mem_.read_byte(addr); val = (uint32_t)(int32_t)b; break; }
            case 0b001: { int16_t h = (int16_t)mem_.read_half(addr); val = (uint32_t)(int32_t)h; break; }
            case 0b010: val = mem_.read_word(addr); break;
            case 0b100: val = mem_.read_byte(addr); break;
            case 0b101: val = mem_.read_half(addr); break;
        }
        memwb_.result = val;
    } else if (ex_.is_store) {
        // Perform memory write
        uint32_t addr = ex_.alu_result;
        uint32_t instr = ex_.instr;
        uint32_t f3 = (instr >> 12) & 0x7;
        switch (f3) {
            case 0b000: mem_.write_byte(addr, ex_.rs2_val & 0xFF); break;
            case 0b001: mem_.write_half(addr, ex_.rs2_val & 0xFFFF); break;
            case 0b010: mem_.write_word(addr, ex_.rs2_val); break;
        }
        memwb_.writes_rd = false;
    } else {
        memwb_.result = ex_.alu_result;
    }
}

// ─── Stage: Execute ─────────────────────────────────────────────────────

void PipelinedCPU::stage_execute() {
    ex_ = {};
    if (!ifid_.valid) return;

    DecodeResult d = decode(ifid_.instr);

    // Handle ECALL
    if (d.opcode == OP_SYSTEM) {
        // CSR instructions
        if (d.funct3 != 0) {
            uint32_t csr_addr = (ifid_.instr >> 20) & 0xFFF;
            uint32_t rs1_val = forward_reg(d.rs1);
            uint32_t old_val = csr_read(csr_addr);
            switch (d.funct3) {
                case 0b001: csr_write(csr_addr, rs1_val); break;          // CSRRW
                case 0b010: csr_write(csr_addr, old_val | rs1_val); break; // CSRRS
                case 0b011: csr_write(csr_addr, old_val & ~rs1_val); break;// CSRRC
            }
            ex_.valid = true;
            ex_.instr = ifid_.instr;
            ex_.pc = ifid_.pc;
            ex_.rd = d.rd;
            ex_.writes_rd = (d.rd != 0);
            ex_.alu_result = old_val;
            return;
        }
        // ECALL = halt, but mark as a special instruction that flows through pipeline
        ex_.valid = true;
        ex_.instr = ifid_.instr;
        ex_.pc = ifid_.pc;
        ex_.writes_rd = false;
        ex_.is_load = false;
        ex_.is_store = false;
        ex_.is_branch = false;
        // Set halted after this cycle's writeback has completed
        halted_ = true;
        return;
    }

    // Get forwarded register values
    uint32_t rs1_val = forward_reg(d.rs1);
    uint32_t rs2_val = forward_reg(d.rs2);

    // Execute
    ExecResult er = execute_alu(d, rs1_val, rs2_val, ifid_.pc);

    ex_.valid = true;
    ex_.instr = ifid_.instr;
    ex_.pc = ifid_.pc;
    ex_.alu_result = er.result;
    ex_.rs2_val = er.store_value;
    ex_.rd = d.rd;
    ex_.writes_rd = er.writes_rd;
    ex_.is_load = er.is_load;
    ex_.is_store = er.is_store;
    ex_.is_branch = er.is_branch;
    ex_.branch_taken = er.branch_taken;
    ex_.branch_target = er.branch_target;

    // Handle branch/jump: flush IF/ID, redirect PC
    if (er.is_branch && er.branch_taken) {
        pc_ = er.branch_target;
        ifid_ = {}; // flush
        perf_.stalls_branch++;
    }
}

// ─── Stage: Fetch/Decode ────────────────────────────────────────────────

void PipelinedCPU::stage_fetch_decode() {
    // If EX took a branch, IF/ID was already flushed and PC redirected
    if (ex_.is_branch && ex_.branch_taken) return;

    ifid_.instr = mem_.read_word(pc_);
    ifid_.pc = pc_;
    ifid_.valid = true;
    pc_ += 4;
}

// ─── Forwarding ─────────────────────────────────────────────────────────

uint32_t PipelinedCPU::forward_reg(uint32_t reg_idx) const {
    if (reg_idx == 0) return 0;

    // Forward from EX stage (previous instruction's result)
    if (ex_.valid && ex_.writes_rd && ex_.rd == reg_idx && !ex_.is_load) {
        return ex_.alu_result;
    }

    // Forward from MEM/WB stage
    if (memwb_.valid && memwb_.writes_rd && memwb_.rd == reg_idx) {
        return memwb_.result;
    }

    return regs_[reg_idx];
}

// ─── Hazard Detection ───────────────────────────────────────────────────

bool PipelinedCPU::detect_load_use_hazard() const {
    // Load-use: EX stage has a load, and IF/ID needs that register
    if (!ex_.valid || !ex_.is_load || !ifid_.valid) return false;

    DecodeResult d = decode(ifid_.instr);
    uint32_t load_rd = ex_.rd;

    // Check if IF/ID instruction reads the load destination
    bool uses_rs1 = (d.opcode != OP_LUI && d.opcode != OP_AUIPC && d.opcode != OP_JAL);
    bool uses_rs2 = (d.opcode == OP_REG || d.opcode == OP_BRANCH || d.opcode == OP_STORE);

    if (uses_rs1 && d.rs1 == load_rd && load_rd != 0) return true;
    if (uses_rs2 && d.rs2 == load_rd && load_rd != 0) return true;

    return false;
}

// ─── Decode ─────────────────────────────────────────────────────────────

PipelinedCPU::DecodeResult PipelinedCPU::decode(uint32_t instr) {
    DecodeResult d;
    d.opcode = instr & 0x7F;
    d.rd = (instr >> 7) & 0x1F;
    d.funct3 = (instr >> 12) & 0x7;
    d.rs1 = (instr >> 15) & 0x1F;
    d.rs2 = (instr >> 20) & 0x1F;
    d.funct7 = (instr >> 25) & 0x7F;

    // Immediates
    d.imm_i = (int32_t)instr >> 20;

    int32_t imm_s = ((instr >> 25) << 5) | ((instr >> 7) & 0x1F);
    if (imm_s & 0x800) imm_s |= 0xFFFFF000;
    d.imm_s = imm_s;

    int32_t imm_b = 0;
    imm_b |= ((instr >> 31) & 1) << 12;
    imm_b |= ((instr >> 7) & 1) << 11;
    imm_b |= ((instr >> 25) & 0x3F) << 5;
    imm_b |= ((instr >> 8) & 0xF) << 1;
    if (imm_b & 0x1000) imm_b |= 0xFFFFE000;
    d.imm_b = imm_b;

    d.imm_u = (int32_t)(instr & 0xFFFFF000);

    int32_t imm_j = 0;
    imm_j |= ((instr >> 31) & 1) << 20;
    imm_j |= ((instr >> 12) & 0xFF) << 12;
    imm_j |= ((instr >> 20) & 1) << 11;
    imm_j |= ((instr >> 21) & 0x3FF) << 1;
    if (imm_j & 0x100000) imm_j |= 0xFFE00000;
    d.imm_j = imm_j;

    return d;
}

// ─── ALU Execute ────────────────────────────────────────────────────────

PipelinedCPU::ExecResult PipelinedCPU::execute_alu(const DecodeResult& d, uint32_t rs1_val, uint32_t rs2_val, uint32_t pc) {
    ExecResult r = {};

    switch (d.opcode) {
    case OP_LUI:
        r.result = (uint32_t)d.imm_u;
        r.writes_rd = true;
        break;

    case OP_AUIPC:
        r.result = pc + (uint32_t)d.imm_u;
        r.writes_rd = true;
        break;

    case OP_JAL:
        r.result = pc + 4; // link
        r.writes_rd = true;
        r.is_branch = true;
        r.branch_taken = true;
        r.branch_target = pc + (uint32_t)d.imm_j;
        break;

    case OP_JALR:
        r.result = pc + 4;
        r.writes_rd = true;
        r.is_branch = true;
        r.branch_taken = true;
        r.branch_target = (rs1_val + (uint32_t)d.imm_i) & ~1u;
        break;

    case OP_BRANCH: {
        int32_t a = (int32_t)rs1_val, b = (int32_t)rs2_val;
        uint32_t ua = rs1_val, ub = rs2_val;
        bool taken = false;
        switch (d.funct3) {
            case 0b000: taken = (a == b); break;
            case 0b001: taken = (a != b); break;
            case 0b100: taken = (a < b); break;
            case 0b101: taken = (a >= b); break;
            case 0b110: taken = (ua < ub); break;
            case 0b111: taken = (ua >= ub); break;
        }
        r.is_branch = true;
        r.branch_taken = taken;
        r.branch_target = pc + (uint32_t)d.imm_b;
        r.writes_rd = false;
        break;
    }

    case OP_LOAD:
        r.result = rs1_val + (uint32_t)d.imm_i; // address
        r.writes_rd = true;
        r.is_load = true;
        break;

    case OP_STORE:
        r.result = rs1_val + (uint32_t)d.imm_s; // address
        r.store_value = rs2_val;
        r.is_store = true;
        break;

    case OP_IMM: {
        uint32_t src = rs1_val;
        switch (d.funct3) {
            case 0b000: r.result = src + (uint32_t)d.imm_i; break;
            case 0b010: r.result = ((int32_t)src < d.imm_i) ? 1 : 0; break;
            case 0b011: r.result = (src < (uint32_t)d.imm_i) ? 1 : 0; break;
            case 0b100: r.result = src ^ (uint32_t)d.imm_i; break;
            case 0b110: r.result = src | (uint32_t)d.imm_i; break;
            case 0b111: r.result = src & (uint32_t)d.imm_i; break;
            case 0b001: r.result = src << (d.imm_i & 0x1F); break;
            case 0b101:
                if (d.funct7 & 0x20)
                    r.result = (uint32_t)((int32_t)src >> (d.imm_i & 0x1F));
                else
                    r.result = src >> (d.imm_i & 0x1F);
                break;
        }
        r.writes_rd = true;
        break;
    }

    case OP_REG: {
        uint32_t a = rs1_val, b = rs2_val;
        switch (d.funct3) {
            case 0b000: r.result = (d.funct7 & 0x20) ? (a - b) : (a + b); break;
            case 0b001: r.result = a << (b & 0x1F); break;
            case 0b010: r.result = ((int32_t)a < (int32_t)b) ? 1 : 0; break;
            case 0b011: r.result = (a < b) ? 1 : 0; break;
            case 0b100: r.result = a ^ b; break;
            case 0b101: r.result = (d.funct7 & 0x20) ? (uint32_t)((int32_t)a >> (b & 0x1F)) : (a >> (b & 0x1F)); break;
            case 0b110: r.result = a | b; break;
            case 0b111: r.result = a & b; break;
        }
        r.writes_rd = true;
        break;
    }
    }

    return r;
}

// ─── CSR Access ─────────────────────────────────────────────────────────

uint32_t PipelinedCPU::csr_read(uint32_t addr) const {
    switch (addr) {
        case CSR_MCYCLE:    return (uint32_t)(perf_.mcycle);
        case CSR_MCYCLEH:   return (uint32_t)(perf_.mcycle >> 32);
        case CSR_MINSTRET:  return (uint32_t)(perf_.minstret);
        case CSR_MINSTRETH: return (uint32_t)(perf_.minstret >> 32);
        case CSR_MHPMCOUNTER3: return (uint32_t)(perf_.stalls_data);
        case CSR_MHPMCOUNTER4: return (uint32_t)(perf_.stalls_branch);
        default: {
            auto it = csrs_.find(addr);
            return (it != csrs_.end()) ? it->second : 0;
        }
    }
}

void PipelinedCPU::csr_write(uint32_t addr, uint32_t value) {
    csrs_[addr] = value;
}

void PipelinedCPU::write_reg(uint32_t rd, uint32_t value) {
    if (rd == 0) return;
    regs_[rd] = value;
}

// ─── Observation ────────────────────────────────────────────────────────

uint64_t PipelinedCPU::get_cycles() const { return perf_.mcycle; }
uint64_t PipelinedCPU::get_instret() const { return perf_.minstret; }
uint32_t PipelinedCPU::get_pc() const { return pc_; }
uint32_t PipelinedCPU::get_reg(uint32_t idx) const { return (idx < 32) ? regs_[idx] : 0; }
std::vector<uint8_t> PipelinedCPU::read_mem(uint32_t addr, uint32_t len) const { return mem_.dump(addr, len); }
PerfCounters PipelinedCPU::get_perf_counters() const { return perf_; }
