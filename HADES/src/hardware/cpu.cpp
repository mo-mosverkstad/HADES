#include "cpu.h"
#include "rv32_decode.h"

CPU::CPU() { reset(); }

void CPU::reset() {
    reset_base();
}

void CPU::step() {
    if (halted_) return;
    uint32_t instr = mem_.imem().read_word(pc_);
    if (mem_.imem().has_fault()) { halted_ = true; return; }
    perf_.mcycle += mem_.imem().drain_penalty();
    execute(instr);
    if (io_enabled_) io_bus_.tick_all();
    check_interrupts();
    perf_.mcycle++;
    perf_.minstret++;
}

void CPU::run(uint32_t max_instructions) {
    if (max_instructions == 0) {
        exec_.run_async(0);
        return;
    }
    // Synchronous: run directly, no thread
    for (uint32_t i = 0; i < max_instructions && !halted_; i++) {
        step();
    }
}

void CPU::execute(uint32_t instr) {
    Decoded d = decode_instr(instr);

    switch (d.opcode) {
    case OP_LUI:
        write_reg(d.rd, (uint32_t)d.imm_u);
        pc_ += 4;
        break;

    case OP_AUIPC:
        write_reg(d.rd, pc_ + (uint32_t)d.imm_u);
        pc_ += 4;
        break;

    case OP_JAL: {
        uint32_t ret = pc_ + 4;
        pc_ = pc_ + (uint32_t)d.imm_j;
        write_reg(d.rd, ret);
        break;
    }

    case OP_JALR: {
        uint32_t ret = pc_ + 4;
        pc_ = (regs_[d.rs1] + (uint32_t)d.imm_i) & ~1u;
        write_reg(d.rd, ret);
        break;
    }

    case OP_BRANCH: {
        int32_t a = (int32_t)regs_[d.rs1];
        int32_t b = (int32_t)regs_[d.rs2];
        uint32_t ua = regs_[d.rs1];
        uint32_t ub = regs_[d.rs2];
        bool taken = false;
        switch (d.funct3) {
            case 0b000: taken = (a == b); break;
            case 0b001: taken = (a != b); break;
            case 0b100: taken = (a < b); break;
            case 0b101: taken = (a >= b); break;
            case 0b110: taken = (ua < ub); break;
            case 0b111: taken = (ua >= ub); break;
        }
        pc_ = taken ? pc_ + (uint32_t)d.imm_b : pc_ + 4;
        break;
    }

    case OP_LOAD: {
        uint32_t addr = regs_[d.rs1] + (uint32_t)d.imm_i;
        uint32_t val = 0;
        switch (d.funct3) {
            case 0b000: val = (uint32_t)(int32_t)(int8_t)mem_.dmem().read_byte(addr); break;
            case 0b001: val = (uint32_t)(int32_t)(int16_t)mem_.dmem().read_half(addr); break;
            case 0b010: val = mem_.dmem().read_word(addr); break;
            case 0b100: val = mem_.dmem().read_byte(addr); break;
            case 0b101: val = mem_.dmem().read_half(addr); break;
        }
        if (mem_.dmem().has_fault()) { halted_ = true; break; }
        perf_.mcycle += mem_.dmem().drain_penalty();
        write_reg(d.rd, val);
        pc_ += 4;
        break;
    }

    case OP_STORE: {
        uint32_t addr = regs_[d.rs1] + (uint32_t)d.imm_s;
        switch (d.funct3) {
            case 0b000: mem_.dmem().write_byte(addr, regs_[d.rs2] & 0xFF); break;
            case 0b001: mem_.dmem().write_half(addr, regs_[d.rs2] & 0xFFFF); break;
            case 0b010: mem_.dmem().write_word(addr, regs_[d.rs2]); break;
        }
        if (mem_.dmem().has_fault()) { halted_ = true; break; }
        mem_.dmem().drain_penalty();
        pc_ += 4;
        break;
    }

    case OP_IMM: {
        uint32_t src = regs_[d.rs1];
        uint32_t result = 0;
        switch (d.funct3) {
            case 0b000: result = src + (uint32_t)d.imm_i; break;
            case 0b010: result = ((int32_t)src < d.imm_i) ? 1 : 0; break;
            case 0b011: result = (src < (uint32_t)d.imm_i) ? 1 : 0; break;
            case 0b100: result = src ^ (uint32_t)d.imm_i; break;
            case 0b110: result = src | (uint32_t)d.imm_i; break;
            case 0b111: result = src & (uint32_t)d.imm_i; break;
            case 0b001: result = src << (d.imm_i & 0x1F); break;
            case 0b101:
                result = (d.funct7 & 0x20)
                    ? (uint32_t)((int32_t)src >> (d.imm_i & 0x1F))
                    : src >> (d.imm_i & 0x1F);
                break;
        }
        write_reg(d.rd, result);
        pc_ += 4;
        break;
    }

    case OP_REG: {
        uint32_t a = regs_[d.rs1], b = regs_[d.rs2];
        uint32_t result = 0;
        switch (d.funct3) {
            case 0b000: result = (d.funct7 & 0x20) ? (a - b) : (a + b); break;
            case 0b001: result = a << (b & 0x1F); break;
            case 0b010: result = ((int32_t)a < (int32_t)b) ? 1 : 0; break;
            case 0b011: result = (a < b) ? 1 : 0; break;
            case 0b100: result = a ^ b; break;
            case 0b101: result = (d.funct7 & 0x20) ? (uint32_t)((int32_t)a >> (b & 0x1F)) : (a >> (b & 0x1F)); break;
            case 0b110: result = a | b; break;
            case 0b111: result = a & b; break;
        }
        write_reg(d.rd, result);
        pc_ += 4;
        break;
    }

    case OP_SYSTEM: {
        uint32_t csr_addr = (instr >> 20) & 0xFFF;
        // funct3 == 0: ECALL or MRET
        if (d.funct3 == 0) {
            if (csr_addr == 0x302) {
                // MRET: return from interrupt
                pc_ = csr_read(CSR_MEPC);
                interrupts_enabled_ = true;
            } else {
                // ECALL = halt
                halted_ = true;
            }
            break;
        }
        // CSR instructions
        uint32_t rs1_val = regs_[d.rs1];
        uint32_t old_val = csr_read(csr_addr);
        switch (d.funct3) {
            case 0b001: csr_write(csr_addr, rs1_val); break;          // CSRRW
            case 0b010: csr_write(csr_addr, old_val | rs1_val); break; // CSRRS
            case 0b011: csr_write(csr_addr, old_val & ~rs1_val); break;// CSRRC
        }
        write_reg(d.rd, old_val);
        pc_ += 4;
        break;
    }

    default:
        halted_ = true;
        break;
    }

    regs_[0] = 0;
}
