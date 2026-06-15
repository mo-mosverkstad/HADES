import struct

# Register aliases
ZERO, RA, SP = 0, 1, 2
T0, T1, T2, T3, T4, T5, T6 = 5, 6, 7, 28, 29, 30, 31
A0, A1, A2 = 10, 11, 12
S0, S1 = 8, 9

# Opcodes
OP_REG = 0b0110011
OP_IMM = 0b0010011
OP_LOAD = 0b0000011
OP_STORE = 0b0100011
OP_BRANCH = 0b1100011
OP_JAL = 0b1101111
OP_JALR = 0b1100111
OP_LUI = 0b0110111
OP_AUIPC = 0b0010111
OP_SYSTEM = 0b1110011

ECALL = 0b00000000000000000000000001110011

def encode_r(funct7, rs2, rs1, funct3, rd, opcode):
    return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode


def encode_i(imm, rs1, funct3, rd, opcode):
    return ((imm & 0xFFF) << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode


def encode_s(imm, rs2, rs1, funct3, opcode):
    imm11_5 = (imm >> 5) & 0x7F
    imm4_0 = imm & 0x1F
    return (imm11_5 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (imm4_0 << 7) | opcode


def encode_b(imm, rs2, rs1, funct3, opcode):
    imm12 = (imm >> 12) & 1
    imm10_5 = (imm >> 5) & 0x3F
    imm4_1 = (imm >> 1) & 0xF
    imm11 = (imm >> 11) & 1
    return (imm12 << 31) | (imm10_5 << 25) | (rs2 << 20) | (rs1 << 15) | \
           (funct3 << 12) | (imm4_1 << 8) | (imm11 << 7) | opcode


def encode_u(imm, rd, opcode):
    return (imm & 0xFFFFF000) | (rd << 7) | opcode


def encode_j(imm, rd, opcode):
    imm20 = (imm >> 20) & 1
    imm10_1 = (imm >> 1) & 0x3FF
    imm11 = (imm >> 11) & 1
    imm19_12 = (imm >> 12) & 0xFF
    return (imm20 << 31) | (imm10_1 << 21) | (imm11 << 20) | (imm19_12 << 12) | (rd << 7) | opcode


def to_bytes(instructions):
    """Convert list of 32-bit instructions to bytes (little-endian)."""
    return b''.join(struct.pack('<I', i) for i in instructions)