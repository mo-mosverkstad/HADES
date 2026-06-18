#include "cpu.h"
#include "rv32_decode.h"

CPU::CPU() { reset(); }

void CPU::reset() {
    for (int i = 0; i < 32; i++) regs_[i] = 0;
    pc_ = 0x1000;
    cycles_ = 0;
    halted_ = false;
    mem_.reset();
}

void CPU::run(uint32_t max_instructions) {
    uint32_t count = 0;
    while (!halted_ && count < max_instructions) {
        uint32_t instr = mem_.icache().read_word(pc_);
        cycles_ += mem_.icache().drain_penalty();

        execute(instr);
        cycles_++;
        count++;
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
            case 0b000: val = (uint32_t)(int32_t)(int8_t)mem_.dcache().read_byte(addr); break;
            case 0b001: val = (uint32_t)(int32_t)(int16_t)mem_.dcache().read_half(addr); break;
            case 0b010: val = mem_.dcache().read_word(addr); break;
            case 0b100: val = mem_.dcache().read_byte(addr); break;
            case 0b101: val = mem_.dcache().read_half(addr); break;
        }
        cycles_ += mem_.dcache().drain_penalty();
        write_reg(d.rd, val);
        pc_ += 4;
        break;
    }

    case OP_STORE: {
        uint32_t addr = regs_[d.rs1] + (uint32_t)d.imm_s;
        switch (d.funct3) {
            case 0b000: mem_.dcache().write_byte(addr, regs_[d.rs2] & 0xFF); break;
            case 0b001: mem_.dcache().write_half(addr, regs_[d.rs2] & 0xFFFF); break;
            case 0b010: mem_.dcache().write_word(addr, regs_[d.rs2]); break;
        }
        mem_.dcache().drain_penalty();
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

    case OP_SYSTEM:
    default:
        halted_ = true;
        break;
    }

    regs_[0] = 0;
}

uint64_t CPU::get_cycles() const { return cycles_; }
