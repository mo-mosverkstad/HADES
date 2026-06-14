#include "cpu.h"
#include <stdexcept>

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

CPU::CPU() { reset(); }

void CPU::reset() {
    for (int i = 0; i < 32; i++) regs_[i] = 0;
    pc_ = 0x1000;
    cycles_ = 0;
    halted_ = false;
    mem_.clear();
    leak_.clear();
}

void CPU::load_program(const std::vector<uint8_t>& binary, uint32_t base_addr) {
    mem_.load(base_addr, binary);
    pc_ = base_addr;
}

void CPU::load_data(const std::vector<uint8_t>& data, uint32_t base_addr) {
    mem_.load(base_addr, data);
}

void CPU::run(uint32_t max_instructions) {
    uint32_t count = 0;
    while (!halted_ && count < max_instructions) {
        uint32_t instr = mem_.read_word(pc_);
        execute(instr);
        cycles_++;
        count++;
    }
}

void CPU::write_reg(uint32_t rd, uint32_t value) {
    if (rd == 0) return; // x0 is hardwired to 0
    regs_[rd] = value;
    leak_.record(value);
}

// Immediate decoders
int32_t CPU::imm_i(uint32_t instr) {
    return (int32_t)instr >> 20;
}

int32_t CPU::imm_s(uint32_t instr) {
    int32_t imm = ((instr >> 25) << 5) | ((instr >> 7) & 0x1F);
    // sign extend from bit 11
    if (imm & 0x800) imm |= 0xFFFFF000;
    return imm;
}

int32_t CPU::imm_b(uint32_t instr) {
    int32_t imm = 0;
    imm |= ((instr >> 31) & 1) << 12;
    imm |= ((instr >> 7) & 1) << 11;
    imm |= ((instr >> 25) & 0x3F) << 5;
    imm |= ((instr >> 8) & 0xF) << 1;
    // sign extend from bit 12
    if (imm & 0x1000) imm |= 0xFFFFE000;
    return imm;
}

int32_t CPU::imm_u(uint32_t instr) {
    return (int32_t)(instr & 0xFFFFF000);
}

int32_t CPU::imm_j(uint32_t instr) {
    int32_t imm = 0;
    imm |= ((instr >> 31) & 1) << 20;
    imm |= ((instr >> 12) & 0xFF) << 12;
    imm |= ((instr >> 20) & 1) << 11;
    imm |= ((instr >> 21) & 0x3FF) << 1;
    // sign extend from bit 20
    if (imm & 0x100000) imm |= 0xFFE00000;
    return imm;
}

void CPU::execute(uint32_t instr) {
    uint32_t op = opcode(instr);
    uint32_t d = rd(instr);
    uint32_t f3 = funct3(instr);
    uint32_t r1 = rs1(instr);
    uint32_t r2 = rs2(instr);
    uint32_t f7 = funct7(instr);

    switch (op) {
    case OP_LUI:
        write_reg(d, (uint32_t)imm_u(instr));
        pc_ += 4;
        break;

    case OP_AUIPC:
        write_reg(d, pc_ + (uint32_t)imm_u(instr));
        pc_ += 4;
        break;

    case OP_JAL: {
        uint32_t ret = pc_ + 4;
        pc_ = pc_ + (uint32_t)imm_j(instr);
        write_reg(d, ret);
        break;
    }

    case OP_JALR: {
        uint32_t ret = pc_ + 4;
        pc_ = (regs_[r1] + (uint32_t)imm_i(instr)) & ~1u;
        write_reg(d, ret);
        break;
    }

    case OP_BRANCH: {
        int32_t a = (int32_t)regs_[r1];
        int32_t b = (int32_t)regs_[r2];
        uint32_t ua = regs_[r1];
        uint32_t ub = regs_[r2];
        bool taken = false;
        switch (f3) {
            case 0b000: taken = (a == b); break;   // BEQ
            case 0b001: taken = (a != b); break;   // BNE
            case 0b100: taken = (a < b); break;    // BLT
            case 0b101: taken = (a >= b); break;   // BGE
            case 0b110: taken = (ua < ub); break;  // BLTU
            case 0b111: taken = (ua >= ub); break; // BGEU
        }
        if (taken)
            pc_ = pc_ + (uint32_t)imm_b(instr);
        else
            pc_ += 4;
        break;
    }

    case OP_LOAD: {
        uint32_t addr = regs_[r1] + (uint32_t)imm_i(instr);
        uint32_t val = 0;
        switch (f3) {
            case 0b000: { // LB
                int8_t b = (int8_t)mem_.read_byte(addr);
                val = (uint32_t)(int32_t)b;
                break;
            }
            case 0b001: { // LH
                int16_t h = (int16_t)mem_.read_half(addr);
                val = (uint32_t)(int32_t)h;
                break;
            }
            case 0b010: // LW
                val = mem_.read_word(addr);
                break;
            case 0b100: // LBU
                val = mem_.read_byte(addr);
                break;
            case 0b101: // LHU
                val = mem_.read_half(addr);
                break;
        }
        write_reg(d, val);
        pc_ += 4;
        break;
    }

    case OP_STORE: {
        uint32_t addr = regs_[r1] + (uint32_t)imm_s(instr);
        switch (f3) {
            case 0b000: // SB
                mem_.write_byte(addr, regs_[r2] & 0xFF);
                break;
            case 0b001: // SH
                mem_.write_half(addr, regs_[r2] & 0xFFFF);
                break;
            case 0b010: // SW
                mem_.write_word(addr, regs_[r2]);
                break;
        }
        pc_ += 4;
        break;
    }

    case OP_IMM: {
        int32_t imm = imm_i(instr);
        uint32_t src = regs_[r1];
        uint32_t result = 0;
        switch (f3) {
            case 0b000: // ADDI
                result = src + (uint32_t)imm;
                break;
            case 0b010: // SLTI
                result = ((int32_t)src < imm) ? 1 : 0;
                break;
            case 0b011: // SLTIU
                result = (src < (uint32_t)imm) ? 1 : 0;
                break;
            case 0b100: // XORI
                result = src ^ (uint32_t)imm;
                break;
            case 0b110: // ORI
                result = src | (uint32_t)imm;
                break;
            case 0b111: // ANDI
                result = src & (uint32_t)imm;
                break;
            case 0b001: // SLLI
                result = src << (imm & 0x1F);
                break;
            case 0b101: // SRLI / SRAI
                if (f7 & 0x20)
                    result = (uint32_t)((int32_t)src >> (imm & 0x1F)); // SRAI
                else
                    result = src >> (imm & 0x1F); // SRLI
                break;
        }
        write_reg(d, result);
        pc_ += 4;
        break;
    }

    case OP_REG: {
        uint32_t a = regs_[r1];
        uint32_t b = regs_[r2];
        uint32_t result = 0;
        switch (f3) {
            case 0b000: // ADD / SUB
                result = (f7 & 0x20) ? (a - b) : (a + b);
                break;
            case 0b001: // SLL
                result = a << (b & 0x1F);
                break;
            case 0b010: // SLT
                result = ((int32_t)a < (int32_t)b) ? 1 : 0;
                break;
            case 0b011: // SLTU
                result = (a < b) ? 1 : 0;
                break;
            case 0b100: // XOR
                result = a ^ b;
                break;
            case 0b101: // SRL / SRA
                if (f7 & 0x20)
                    result = (uint32_t)((int32_t)a >> (b & 0x1F)); // SRA
                else
                    result = a >> (b & 0x1F); // SRL
                break;
            case 0b110: // OR
                result = a | b;
                break;
            case 0b111: // AND
                result = a & b;
                break;
        }
        write_reg(d, result);
        pc_ += 4;
        break;
    }

    case OP_SYSTEM:
        // ECALL = halt
        halted_ = true;
        break;

    default:
        // Unknown instruction - halt
        halted_ = true;
        break;
    }

    // Enforce x0 = 0
    regs_[0] = 0;
}

// Observation methods
std::vector<double> CPU::get_power_trace() const {
    return leak_.trace();
}

uint64_t CPU::get_cycles() const { return cycles_; }
uint32_t CPU::get_pc() const { return pc_; }
uint32_t CPU::get_reg(uint32_t idx) const { return (idx < 32) ? regs_[idx] : 0; }

std::vector<uint8_t> CPU::read_mem(uint32_t addr, uint32_t len) const {
    return mem_.dump(addr, len);
}

// Configuration
void CPU::set_leakage_model(LeakageModel model) { leak_.set_model(model); }
void CPU::set_noise(double stddev) { leak_.set_noise(stddev); }
void CPU::set_seed(uint64_t seed) { leak_.set_seed(seed); }