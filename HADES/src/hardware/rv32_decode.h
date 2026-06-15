#pragma once
#include <cstdint>

enum RV32_Opcode : uint32_t {
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

struct Decoded {
    uint32_t opcode, rd, funct3, rs1, rs2, funct7;
    int32_t imm_i, imm_s, imm_b, imm_u, imm_j;
};

inline Decoded decode_instr(uint32_t instr) {
    Decoded d;
    d.opcode = instr & 0x7F;
    d.rd     = (instr >> 7) & 0x1F;
    d.funct3 = (instr >> 12) & 0x7;
    d.rs1    = (instr >> 15) & 0x1F;
    d.rs2    = (instr >> 20) & 0x1F;
    d.funct7 = (instr >> 25) & 0x7F;

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
