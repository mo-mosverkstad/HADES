# RISC-V RV32I Cheat Sheet (HADES)

## Registers

| Reg   | ABI  | x#  | Usage                    |
|-------|------|-----|--------------------------|
| x0    | zero | 0   | Hardwired to 0           |
| x1    | ra   | 1   | Return address           |
| x2    | sp   | 2   | Stack pointer            |
| x5-x7 | t0-t2| 5-7 | Temporaries              |
| x8-x9 | s0-s1| 8-9 | Saved registers          |
| x10-x12| a0-a2|10-12| Arguments / return vals  |
| x28-x31| t3-t6|28-31| Temporaries              |

## Instruction Formats

```
R:  [funct7(7)][rs2(5)][rs1(5)][funct3(3)][rd(5)][opcode(7)]
I:  [imm[11:0](12)    ][rs1(5)][funct3(3)][rd(5)][opcode(7)]
S:  [imm[11:5](7)][rs2(5)][rs1(5)][funct3(3)][imm[4:0](5)][opcode(7)]
B:  [imm[12|10:5](7)][rs2(5)][rs1(5)][funct3(3)][imm[4:1|11](5)][opcode(7)]
U:  [imm[31:12](20)                        ][rd(5)][opcode(7)]
J:  [imm[20|10:1|11|19:12](20)             ][rd(5)][opcode(7)]
```

## Opcodes

| Opcode    | Binary    | Hex  | Format |
|-----------|-----------|------|--------|
| OP_LUI    | 0110111   | 0x37 | U      |
| OP_AUIPC  | 0010111   | 0x17 | U      |
| OP_JAL    | 1101111   | 0x6F | J      |
| OP_JALR   | 1100111   | 0x67 | I      |
| OP_BRANCH | 1100011   | 0x63 | B      |
| OP_LOAD   | 0000011   | 0x03 | I      |
| OP_STORE  | 0100011   | 0x23 | S      |
| OP_IMM    | 0010011   | 0x13 | I      |
| OP_REG    | 0110011   | 0x33 | R      |
| OP_SYSTEM | 1110011   | 0x73 | I      |

## Instructions

### Arithmetic & Logic (R-type: OP_REG)

| Instruction | funct7  | funct3 | C equivalent        |
|-------------|---------|--------|---------------------|
| ADD rd,rs1,rs2  | 0000000 | 000 | rd = rs1 + rs2   |
| SUB rd,rs1,rs2  | 0100000 | 000 | rd = rs1 - rs2   |
| AND rd,rs1,rs2  | 0000000 | 111 | rd = rs1 & rs2   |
| OR  rd,rs1,rs2  | 0000000 | 110 | rd = rs1 | rs2   |
| XOR rd,rs1,rs2  | 0000000 | 100 | rd = rs1 ^ rs2   |
| SLL rd,rs1,rs2  | 0000000 | 001 | rd = rs1 << rs2  |
| SRL rd,rs1,rs2  | 0000000 | 101 | rd = rs1 >> rs2  |
| SRA rd,rs1,rs2  | 0100000 | 101 | rd = (int)rs1 >> rs2 |
| SLT rd,rs1,rs2  | 0000000 | 010 | rd = (int)rs1 < (int)rs2 |
| SLTU rd,rs1,rs2 | 0000000 | 011 | rd = rs1 < rs2 (unsigned) |

### Immediate (I-type: OP_IMM)

| Instruction | funct3 | C equivalent             |
|-------------|--------|--------------------------|
| ADDI rd,rs1,imm  | 000 | rd = rs1 + imm         |
| ANDI rd,rs1,imm  | 111 | rd = rs1 & imm         |
| ORI  rd,rs1,imm  | 110 | rd = rs1 | imm         |
| XORI rd,rs1,imm  | 100 | rd = rs1 ^ imm         |
| SLTI rd,rs1,imm  | 010 | rd = (int)rs1 < imm    |
| SLTIU rd,rs1,imm | 011 | rd = rs1 < imm (unsigned) |
| SLLI rd,rs1,shamt | 001 | rd = rs1 << shamt      |
| SRLI rd,rs1,shamt | 101 | rd = rs1 >> shamt      |
| SRAI rd,rs1,shamt | 101 | rd = (int)rs1 >> shamt (funct7=0100000) |

imm: sign-extended 12-bit [-2048, 2047]

### Load (I-type: OP_LOAD)

| Instruction | funct3 | C equivalent            |
|-------------|--------|-------------------------|
| LW  rd,imm(rs1) | 010 | rd = *(int32*)(rs1+imm)  |
| LH  rd,imm(rs1) | 001 | rd = *(int16*)(rs1+imm)  |
| LB  rd,imm(rs1) | 000 | rd = *(int8*)(rs1+imm)   |
| LHU rd,imm(rs1) | 101 | rd = *(uint16*)(rs1+imm) |
| LBU rd,imm(rs1) | 100 | rd = *(uint8*)(rs1+imm)  |

### Store (S-type: OP_STORE)

| Instruction | funct3 | C equivalent              |
|-------------|--------|---------------------------|
| SW rs2,imm(rs1) | 010 | *(int32*)(rs1+imm) = rs2 |
| SH rs2,imm(rs1) | 001 | *(int16*)(rs1+imm) = rs2 |
| SB rs2,imm(rs1) | 000 | *(int8*)(rs1+imm) = rs2  |

### Branch (B-type: OP_BRANCH)

| Instruction | funct3 | C equivalent              |
|-------------|--------|---------------------------|
| BEQ rs1,rs2,off  | 000 | if (rs1 == rs2) pc += off |
| BNE rs1,rs2,off  | 001 | if (rs1 != rs2) pc += off |
| BLT rs1,rs2,off  | 100 | if ((int)rs1 < (int)rs2) pc += off |
| BGE rs1,rs2,off  | 101 | if ((int)rs1 >= (int)rs2) pc += off |
| BLTU rs1,rs2,off | 110 | if (rs1 < rs2) pc += off (unsigned) |
| BGEU rs1,rs2,off | 111 | if (rs1 >= rs2) pc += off (unsigned) |

off: signed, 2-byte aligned, PC-relative

### Upper Immediate (U-type)

| Instruction | Opcode | C equivalent           |
|-------------|--------|------------------------|
| LUI rd,imm    | OP_LUI   | rd = imm << 12      |
| AUIPC rd,imm  | OP_AUIPC | rd = pc + (imm << 12) |

### Jump (J-type / I-type)

| Instruction | C equivalent                  |
|-------------|-------------------------------|
| JAL rd,off    | rd = pc+4; pc += off        |
| JALR rd,rs1,imm | rd = pc+4; pc = (rs1+imm)&~1 |

### System (I-type: OP_SYSTEM)

| Instruction | funct3 | imm[11:0] | C equivalent          |
|-------------|--------|-----------|-------------------------------|
| ECALL       | 000    | 0x000     | halt (trap to environment)    |
| MRET        | 000    | 0x302     | pc = mepc; enable interrupts  |
| CSRRW rd,csr,rs1 | 001 | csr   | rd = csr; csr = rs1          |
| CSRRS rd,csr,rs1 | 010 | csr   | rd = csr; csr |= rs1         |
| CSRRC rd,csr,rs1 | 011 | csr   | rd = csr; csr &= ~rs1        |

## CSR Addresses

| Address | Name          | R/W | Description         |
|---------|---------------|-----|---------------------|
| 0x180   | satp          | RW  | MMU page table base |
| 0x305   | mtvec         | RW  | Trap handler address|
| 0x341   | mepc          | RW  | Exception PC        |
| 0x342   | mcause        | RW  | Trap cause          |
| 0xB00   | mcycle        | R   | Cycle count (low)   |
| 0xB02   | minstret      | R   | Instret count (low) |
| 0xB03   | mhpmcounter3  | R   | Data stalls         |
| 0xB04   | mhpmcounter4  | R   | Branch stalls       |
| 0xB80   | mcycleh       | R   | Cycle count (high)  |
| 0xB82   | minstreth     | R   | Instret count (high)|

## HADES Memory Map

```
0x0000 - 0x0FFF   Data segment (4KB)
0x1000 - 0xEFFF   Code + general memory
0xF000 - 0xFFFF   I/O devices (memory-mapped)
```

## I/O Device Registers

### Timer (base: 0xF000)

| Offset | Register   | R/W | Bits                        |
|--------|-----------|-----|-----------------------------|
| 0x00   | STATUS    | RW  | [0] TO (write 0 to clear)   |
| 0x04   | CONTROL   | RW  | [0]ITO [1]CONT [2]START [3]STOP |
| 0x08   | PERIOD_LO | RW  | Period low 32 bits           |
| 0x0C   | PERIOD_HI | RW  | Period high 32 bits          |
| 0x10   | SNAP_LO   | W   | Write triggers snapshot      |

IRQ fires when TO=1 and ITO=1.

### UART (base: 0xF020)

| Offset | Register | R/W | Bits                           |
|--------|----------|-----|--------------------------------|
| 0x00   | DATA     | RW  | R:[15]RVALID [23:16]RAVAIL [7:0]data / W:[7:0]TX |
| 0x04   | CONTROL  | RW  | [0]RE [1]WE [8]RI [9]WI [31:16]WSPACE |

Read DATA: pops RX FIFO. Write DATA: pushes TX FIFO.

### GPIO (base: 0xF040)

| Offset | Register      | R/W | Description          |
|--------|--------------|-----|----------------------|
| 0x00   | DATA         | RW  | R=input, W=output    |
| 0x04   | DIRECTION    | RW  | 1=output, 0=input    |
| 0x08   | INTERRUPTMASK| RW  | 1=enable edge IRQ    |
| 0x0C   | EDGECAPTURE  | RW  | Edge detected (W1C)  |

### VGA (base: 0xF080)

| Offset | Register   | RW | Description                    |
|--------|-----------|-----|--------------------------------|
| 0x00   | CONTROL   | RW  | [0]enable [1]mode(0=char,1=px) |
| 0x04   | CURSOR_X  | RW  | Char column (0-79)             |
| 0x08   | CURSOR_Y  | RW  | Char row (0-59)                |
| 0x0C   | PIXEL_ADDR| RW  | Pixel index (auto-increment)   |
| 0x10   | PIXEL_DATA| W   | RGB565 pixel value             |
| 0x14   | CHAR_WRITE| W   | [7:0]ASCII [15:8]color         |

Char buffer: 80x60. Pixel buffer: 320x240 RGB565.

### Disk (base: 0xF0A0)

| Offset | Register  | RW | Description                     |
|--------|----------|----|---------------------------------|
| 0x00   | COMMAND  | RW | 0=NOP, 1=READ(disk->RAM), 2=WRITE(RAM->disk) |
| 0x04   | SECTOR   | RW | Sector number (512 bytes each)  |
| 0x08   | BUFFER   | RW | RAM address for DMA             |
| 0x0C   | STATUS   | RW | 0=idle, 1=busy, 2=done, 3=error |
| 0x10   | DISK_SIZE| R  | Total sectors                   |
| 0x14   | LATENCY  | RW | Cycles per operation            |

Write SECTOR, then BUFFER, then COMMAND (triggers DMA).

### Mutex (Multi-core, base: 0xF060)

| Offset | Register | RW | Bits                    |
|--------|----------|-----|-------------------------|
| 0x00   | LOCK     | RW  | [31:1]owner [0]locked   |

Lock: write (owner<<1)|1, read back to verify. Unlock: write 0.

## Interrupt Flow

```
1. Device sets IRQ pending (e.g. Timer TO + ITO)
2. CPU checks: interrupts_enabled && io_bus.any_irq_pending()
3. mepc = pc
4. mcause = 0x80000007 (interrupt | timer)
5. interrupts_enabled = false
6. pc = mtvec
7. Handler runs...
8. MRET: pc = mepc, interrupts_enabled = true
```

## Pipeline (3-stage)

```
IF/ID -> EX -> MEM/WB

Forwarding: EX->EX, MEM/WB->EX
Load-use hazard: 1 cycle stall
Branch taken: 1 cycle penalty (flush IF/ID)
```

## Python Encoder (riscvtools)

```python
from riscvtools import *

# R-type: encode_r(funct7, rs2, rs1, funct3, rd, opcode)
encode_r(0, T1, T0, 0b000, T2, OP_REG)      # add t2, t0, t1

# I-type: encode_i(imm, rs1, funct3, rd, opcode)
encode_i(42, T0, 0b000, T1, OP_IMM)          # addi t1, t0, 42
encode_i(0, T0, 0b010, T1, OP_LOAD)          # lw t1, 0(t0)
encode_i(0x305, T0, 0b001, ZERO, OP_SYSTEM)  # csrrw x0, mtvec, t0

# S-type: encode_s(imm, rs2, rs1, funct3, opcode)
encode_s(8, T1, T0, 0b010, OP_STORE)         # sw t1, 8(t0)

# B-type: encode_b(imm, rs2, rs1, funct3, opcode)
encode_b(-8, T1, T0, 0b000, OP_BRANCH)       # beq t0, t1, -8

# U-type: encode_u(imm, rd, opcode)
encode_u(0x12345000, T0, OP_LUI)             # lui t0, 0x12345

# J-type: encode_j(imm, rd, opcode)
encode_j(0x100, RA, OP_JAL)                  # jal ra, 0x100

# Convert to bytes
prog = to_bytes([instr1, instr2, ...])
```

## Common Patterns

```python
# Load 32-bit constant into register (lui + addi)
encode_u(0x0000F000, T4, OP_LUI)       # t4 = 0xF000 (upper)
encode_i(0x0A0, T4, 0b000, T4, OP_IMM) # t4 = 0xF0A0 (add lower)

# NOP
encode_i(0, ZERO, 0b000, ZERO, OP_IMM) # addi x0, x0, 0

# Infinite loop
encode_b(0, ZERO, ZERO, 0b000, OP_BRANCH) # beq x0, x0, 0 (loop to self)

# ECALL (halt)
ECALL  # = 0x00000073

# MRET (return from interrupt)
encode_i(0x302, ZERO, 0b000, ZERO, OP_SYSTEM)
```
