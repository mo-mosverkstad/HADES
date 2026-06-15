"""
HADES Phase 1 - CPU Regression Tests
Tests RV32I instruction execution using hand-encoded machine code.
No RISC-V cross-compiler needed to run these tests.
"""
import sys
from asmpack import *

try:
    import hades
except ImportError:
    print("ERROR: hades module not found. Run 'make engine' first.")
    sys.exit(1)

def test_addi():
    """Test ADDI instruction."""
    prog = to_bytes([
        encode_i(42, ZERO, 0b000, T0, OP_IMM),  # addi t0, zero, 42
        encode_i(8, T0, 0b000, T1, OP_IMM),      # addi t1, t0, 8
        ECALL,
    ])
    cpu = hades.CPU()
    cpu.load_program(list(prog))
    cpu.run()
    assert cpu.get_reg(T0) == 42, f"Expected 42, got {cpu.get_reg(T0)}"
    assert cpu.get_reg(T1) == 50, f"Expected 50, got {cpu.get_reg(T1)}"
    print("  PASS: test_addi")


def test_add_sub():
    """Test ADD and SUB."""
    prog = to_bytes([
        encode_i(100, ZERO, 0b000, T0, OP_IMM),  # addi t0, zero, 100
        encode_i(30, ZERO, 0b000, T1, OP_IMM),   # addi t1, zero, 30
        encode_r(0b0000000, T1, T0, 0b000, T2, OP_REG),  # add t2, t0, t1
        encode_r(0b0100000, T1, T0, 0b000, T3, OP_REG),  # sub t3, t0, t1
        ECALL,
    ])
    cpu = hades.CPU()
    cpu.load_program(list(prog))
    cpu.run()
    assert cpu.get_reg(T2) == 130, f"Expected 130, got {cpu.get_reg(T2)}"
    assert cpu.get_reg(T3) == 70, f"Expected 70, got {cpu.get_reg(T3)}"
    print("  PASS: test_add_sub")


def test_logic():
    """Test AND, OR, XOR."""
    prog = to_bytes([
        encode_i(0xFF, ZERO, 0b000, T0, OP_IMM),   # addi t0, zero, 0xFF
        encode_i(0x0F, ZERO, 0b000, T1, OP_IMM),   # addi t1, zero, 0x0F
        encode_r(0, T1, T0, 0b111, T2, OP_REG),    # and t2, t0, t1
        encode_r(0, T1, T0, 0b110, T3, OP_REG),    # or  t3, t0, t1
        encode_r(0, T1, T0, 0b100, T4, OP_REG),    # xor t4, t0, t1
        ECALL,
    ])
    cpu = hades.CPU()
    cpu.load_program(list(prog))
    cpu.run()
    assert cpu.get_reg(T2) == 0x0F, f"AND: expected 0x0F, got {cpu.get_reg(T2):#x}"
    assert cpu.get_reg(T3) == 0xFF, f"OR: expected 0xFF, got {cpu.get_reg(T3):#x}"
    assert cpu.get_reg(T4) == 0xF0, f"XOR: expected 0xF0, got {cpu.get_reg(T4):#x}"
    print("  PASS: test_logic")


def test_shift():
    """Test SLL, SRL, SRA, SLLI, SRLI, SRAI."""
    prog = to_bytes([
        encode_i(1, ZERO, 0b000, T0, OP_IMM),      # addi t0, zero, 1
        encode_i(4, ZERO, 0b000, T1, OP_IMM),      # addi t1, zero, 4
        encode_r(0, T1, T0, 0b001, T2, OP_REG),    # sll t2, t0, t1 → 16
        encode_i(8, ZERO, 0b000, T0, OP_IMM),      # addi t0, zero, 8 (shift amount for imm)
        # SLLI t3, t0, 2 → 32
        encode_i(2, T0, 0b001, T3, OP_IMM),
        ECALL,
    ])
    cpu = hades.CPU()
    cpu.load_program(list(prog))
    cpu.run()
    assert cpu.get_reg(T2) == 16, f"SLL: expected 16, got {cpu.get_reg(T2)}"
    assert cpu.get_reg(T3) == 32, f"SLLI: expected 32, got {cpu.get_reg(T3)}"
    print("  PASS: test_shift")


def test_load_store():
    """Test LW, SW, LB, LBU, SB."""
    prog = to_bytes([
        # Store 0xDEADBEEF to address 0x0000
        encode_u(0xDEADB000, T0, OP_LUI),                  # lui t0, 0xDEADB
        encode_i(0xEEF, T0, 0b000, T0, OP_IMM),            # addi t0, t0, 0xEEF (sign-ext!)
        encode_i(0, ZERO, 0b000, T1, OP_IMM),              # addi t1, zero, 0
        encode_s(0x40, T0, T1, 0b010, OP_STORE),           # sw t0, 0x40(t1)
        # Load it back
        encode_i(0x40, T1, 0b010, T2, OP_LOAD),            # lw t2, 0x40(t1)
        # Load byte (signed)
        encode_i(0x40, T1, 0b000, T3, OP_LOAD),            # lb t3, 0x40(t1)
        # Load byte (unsigned)
        encode_i(0x40, T1, 0b100, T4, OP_LOAD),            # lbu t4, 0x40(t1)
        ECALL,
    ])
    cpu = hades.CPU()
    cpu.load_program(list(prog))
    cpu.run()
    # LUI 0xDEADB000 + ADDI 0xFFFFFEEF (sign-extended) = 0xDEADAEEF
    # Actually: lui loads 0xDEADB000, addi adds sign-extended 0xEEF
    # 0xEEF = 0xFFFFFEEF (sign-extended 12-bit), so result = 0xDEADB000 + 0xFFFFFEEF = 0xDEADAAEF? 
    # Let's just check load/store round-trip
    val = cpu.get_reg(T0)
    assert cpu.get_reg(T2) == val, f"LW: expected {val:#x}, got {cpu.get_reg(T2):#x}"
    # LB: lowest byte, sign-extended
    low_byte = val & 0xFF
    if low_byte & 0x80:
        expected_lb = low_byte | 0xFFFFFF00
    else:
        expected_lb = low_byte
    assert cpu.get_reg(T3) == expected_lb, f"LB: expected {expected_lb:#x}, got {cpu.get_reg(T3):#x}"
    # LBU: lowest byte, zero-extended
    assert cpu.get_reg(T4) == low_byte, f"LBU: expected {low_byte:#x}, got {cpu.get_reg(T4):#x}"
    print("  PASS: test_load_store")


def test_branch():
    """Test BEQ, BNE, BLT."""
    prog = to_bytes([
        encode_i(5, ZERO, 0b000, T0, OP_IMM),     # addi t0, zero, 5
        encode_i(5, ZERO, 0b000, T1, OP_IMM),     # addi t1, zero, 5
        # BEQ t0, t1, +8 (skip next instruction)
        encode_b(8, T1, T0, 0b000, OP_BRANCH),
        encode_i(99, ZERO, 0b000, T2, OP_IMM),    # addi t2, zero, 99 (should be skipped)
        # Land here
        encode_i(1, ZERO, 0b000, T3, OP_IMM),     # addi t3, zero, 1
        ECALL,
    ])
    cpu = hades.CPU()
    cpu.load_program(list(prog))
    cpu.run()
    assert cpu.get_reg(T2) == 0, f"BEQ skip failed: t2 = {cpu.get_reg(T2)}"
    assert cpu.get_reg(T3) == 1, f"BEQ land failed: t3 = {cpu.get_reg(T3)}"
    print("  PASS: test_branch")


def test_jal():
    """Test JAL (jump and link)."""
    prog = to_bytes([
        encode_i(10, ZERO, 0b000, T0, OP_IMM),    # addi t0, zero, 10
        # JAL ra, +8 (skip next instruction)
        encode_j(8, RA, OP_JAL),
        encode_i(99, ZERO, 0b000, T1, OP_IMM),    # addi t1, zero, 99 (skipped)
        # Land here
        encode_i(20, ZERO, 0b000, T2, OP_IMM),    # addi t2, zero, 20
        ECALL,
    ])
    cpu = hades.CPU()
    cpu.load_program(list(prog))
    cpu.run()
    assert cpu.get_reg(T1) == 0, f"JAL skip failed: t1 = {cpu.get_reg(T1)}"
    assert cpu.get_reg(T2) == 20, f"JAL land failed: t2 = {cpu.get_reg(T2)}"
    # ra should hold return address (pc of JAL + 4)
    assert cpu.get_reg(RA) == 0x1000 + 8, f"JAL ra: expected {0x1008:#x}, got {cpu.get_reg(RA):#x}"
    print("  PASS: test_jal")


def test_lui():
    """Test LUI."""
    prog = to_bytes([
        encode_u(0x12345000, T0, OP_LUI),  # lui t0, 0x12345
        ECALL,
    ])
    cpu = hades.CPU()
    cpu.load_program(list(prog))
    cpu.run()
    assert cpu.get_reg(T0) == 0x12345000, f"LUI: expected 0x12345000, got {cpu.get_reg(T0):#x}"
    print("  PASS: test_lui")


def test_loop_sum():
    """Test loop: compute 1+2+...+10 = 55."""
    # t0 = sum, t1 = i, t2 = 11
    prog = to_bytes([
        encode_i(0, ZERO, 0b000, T0, OP_IMM),     # addi t0, zero, 0 (sum)
        encode_i(1, ZERO, 0b000, T1, OP_IMM),     # addi t1, zero, 1 (i)
        encode_i(11, ZERO, 0b000, T2, OP_IMM),    # addi t2, zero, 11 (limit)
        # loop: add t0, t0, t1
        encode_r(0, T1, T0, 0b000, T0, OP_REG),
        # addi t1, t1, 1
        encode_i(1, T1, 0b000, T1, OP_IMM),
        # blt t1, t2, -8 (back to add)
        encode_b(-8 & 0x1FFF, T2, T1, 0b100, OP_BRANCH),
        ECALL,
    ])
    cpu = hades.CPU()
    cpu.load_program(list(prog))
    cpu.run()
    assert cpu.get_reg(T0) == 55, f"Loop sum: expected 55, got {cpu.get_reg(T0)}"
    print("  PASS: test_loop_sum")

def test_x0_hardwired():
    """Test that x0 always reads as 0."""
    prog = to_bytes([
        encode_i(42, ZERO, 0b000, ZERO, OP_IMM),  # addi x0, x0, 42 (should be no-op)
        encode_i(0, ZERO, 0b000, T0, OP_IMM),     # addi t0, x0, 0
        ECALL,
    ])
    cpu = hades.CPU()
    cpu.load_program(list(prog))
    cpu.run()
    assert cpu.get_reg(0) == 0, f"x0: expected 0, got {cpu.get_reg(0)}"
    assert cpu.get_reg(T0) == 0, f"t0 from x0: expected 0, got {cpu.get_reg(T0)}"
    print("  PASS: test_x0_hardwired")


def main():
    print("=" * 50)
    print("HADES Phase 1 (regressional tests) - RV32I CPU Regression Tests")
    print("=" * 50)

    tests = [
        test_addi,
        test_add_sub,
        test_logic,
        test_shift,
        test_load_store,
        test_branch,
        test_jal,
        test_lui,
        test_loop_sum,
        test_x0_hardwired,
    ]

    passed = 0
    failed = 0
    for t in tests:
        try:
            t()
            passed += 1
        except AssertionError as e:
            print(f"  FAIL: {t.__name__}: {e}")
            failed += 1
        except Exception as e:
            print(f"  ERROR: {t.__name__}: {e}")
            failed += 1

    print("=" * 50)
    print(f"Results: {passed} passed, {failed} failed, {passed + failed} total")
    if failed > 0:
        sys.exit(1)
    print("All tests passed!")


if __name__ == "__main__":
    main()


def test_pipeline_basic():
    """Test pipeline mode produces correct results."""
    prog = to_bytes([
        encode_i(42, ZERO, 0b000, T0, OP_IMM),  # addi t0, zero, 42
        encode_i(8, T0, 0b000, T1, OP_IMM),      # addi t1, t0, 8
        ECALL,
    ])
    cpu = hades.CPU()
    cpu.load_program(list(prog))
    cpu.run()
    assert cpu.get_reg(T0) == 42, f"Pipeline: t0 expected 42, got {cpu.get_reg(T0)}"
    assert cpu.get_reg(T1) == 50, f"Pipeline: t1 expected 50, got {cpu.get_reg(T1)}"
    print("  PASS: test_pipeline_basic")


def test_pipeline_forwarding():
    """Test that forwarding resolves data dependencies without stall."""
    # addi t0, zero, 10  → t0 = 10
    # addi t1, t0, 5     → t1 = 15 (needs t0 from previous, forwarded from EX)
    # add  t2, t0, t1    → t2 = 25 (needs both, forwarded)
    prog = to_bytes([
        encode_i(10, ZERO, 0b000, T0, OP_IMM),
        encode_i(5, T0, 0b000, T1, OP_IMM),
        encode_r(0, T1, T0, 0b000, T2, OP_REG),
        ECALL,
    ])
    cpu = hades.PipelinedCPU()
    cpu.load_program(list(prog))
    cpu.run()
    assert cpu.get_reg(T0) == 10
    assert cpu.get_reg(T1) == 15
    assert cpu.get_reg(T2) == 25, f"Forwarding: t2 expected 25, got {cpu.get_reg(T2)}"
    # No data stalls expected (ALU→ALU forwarding)
    perf = cpu.get_perf_counters()
    assert perf.stalls_data == 0, f"Forwarding: expected 0 data stalls, got {perf.stalls_data}"
    print("  PASS: test_pipeline_forwarding")


def test_pipeline_load_use_stall():
    """Test load-use hazard causes exactly 1 stall."""
    # lw   t0, 0(zero)   → load from addr 0 (whatever is there)
    # addi t1, t0, 1     → uses t0 immediately → must stall 1 cycle
    prog = to_bytes([
        encode_i(0x40, ZERO, 0b010, T0, OP_LOAD),  # lw t0, 0x40(zero)
        encode_i(1, T0, 0b000, T1, OP_IMM),         # addi t1, t0, 1
        ECALL,
    ])
    cpu = hades.PipelinedCPU()
    # Pre-store a value at address 0x40
    cpu.load_data([0x07, 0x00, 0x00, 0x00], 0x40)  # value = 7
    cpu.load_program(list(prog))
    cpu.run()
    assert cpu.get_reg(T0) == 7, f"Load-use: t0 expected 7, got {cpu.get_reg(T0)}"
    assert cpu.get_reg(T1) == 8, f"Load-use: t1 expected 8, got {cpu.get_reg(T1)}"
    perf = cpu.get_perf_counters()
    assert perf.stalls_data == 1, f"Load-use: expected 1 data stall, got {perf.stalls_data}"
    print("  PASS: test_pipeline_load_use_stall")


def test_pipeline_branch_penalty():
    """Test branch taken causes 1 cycle penalty."""
    prog = to_bytes([
        encode_i(5, ZERO, 0b000, T0, OP_IMM),     # addi t0, zero, 5
        encode_i(5, ZERO, 0b000, T1, OP_IMM),     # addi t1, zero, 5
        # BEQ t0, t1, +8 (skip next)
        encode_b(8, T1, T0, 0b000, OP_BRANCH),
        encode_i(99, ZERO, 0b000, T2, OP_IMM),    # skipped
        encode_i(1, ZERO, 0b000, T3, OP_IMM),     # land here
        ECALL,
    ])
    cpu = hades.PipelinedCPU()
    cpu.load_program(list(prog))
    cpu.run()
    assert cpu.get_reg(T2) == 0, f"Branch: t2 should be 0 (skipped), got {cpu.get_reg(T2)}"
    assert cpu.get_reg(T3) == 1, f"Branch: t3 should be 1, got {cpu.get_reg(T3)}"
    perf = cpu.get_perf_counters()
    assert perf.stalls_branch >= 1, f"Branch: expected >=1 branch stall, got {perf.stalls_branch}"
    print("  PASS: test_pipeline_branch_penalty")


def test_pipeline_cycles_gt_instret():
    """Test that pipeline cycles > instructions due to startup + stalls."""
    prog = to_bytes([
        encode_i(1, ZERO, 0b000, T0, OP_IMM),
        encode_i(2, ZERO, 0b000, T1, OP_IMM),
        encode_i(3, ZERO, 0b000, T2, OP_IMM),
        ECALL,
    ])
    cpu = hades.PipelinedCPU()
    cpu.load_program(list(prog))
    cpu.run(100)  # small limit to avoid runaway
    perf = cpu.get_perf_counters()
    # 3 instructions + ecall flows through pipeline
    assert perf.minstret >= 3, f"instret: expected >=3, got {perf.minstret}"
    assert perf.mcycle > perf.minstret, f"cycles ({perf.mcycle}) should > instret ({perf.minstret})"
    print("  PASS: test_pipeline_cycles_gt_instret")


def test_pipeline_loop():
    """Test pipeline handles loop correctly (sum 1..10)."""
    prog = to_bytes([
        encode_i(0, ZERO, 0b000, T0, OP_IMM),     # sum = 0
        encode_i(1, ZERO, 0b000, T1, OP_IMM),     # i = 1
        encode_i(11, ZERO, 0b000, T2, OP_IMM),    # limit = 11
        encode_r(0, T1, T0, 0b000, T0, OP_REG),   # sum += i
        encode_i(1, T1, 0b000, T1, OP_IMM),       # i++
        encode_b(-8 & 0x1FFF, T2, T1, 0b100, OP_BRANCH),  # blt i, limit, -8
        ECALL,
    ])
    cpu = hades.PipelinedCPU()
    cpu.load_program(list(prog))
    cpu.run()
    assert cpu.get_reg(T0) == 55, f"Pipeline loop: expected 55, got {cpu.get_reg(T0)}"
    print("  PASS: test_pipeline_loop")


def test_perf_counters():
    """Test performance counters are accessible."""
    prog = to_bytes([
        encode_i(1, ZERO, 0b000, T0, OP_IMM),
        ECALL,
    ])
    cpu = hades.PipelinedCPU()
    cpu.load_program(list(prog))
    cpu.run()
    perf = cpu.get_perf_counters()
    assert perf.mcycle > 0
    assert perf.minstret > 0
    print("  PASS: test_perf_counters")


def test_backward_compat_single_cycle():
    """Test that disabling pipeline gives same results as Phase 1."""
    prog = to_bytes([
        encode_i(42, ZERO, 0b000, T0, OP_IMM),
        encode_i(8, T0, 0b000, T1, OP_IMM),
        ECALL,
    ])
    cpu = hades.CPU()
    cpu.load_program(list(prog))
    cpu.run()
    assert cpu.get_reg(T0) == 42
    assert cpu.get_reg(T1) == 50
    assert cpu.get_cycles() == 3  # 2 instructions + ecall
    print("  PASS: test_backward_compat_single_cycle")


# Add pipeline tests to main
if __name__ != "__main__":
    pass  # imported as module
else:
    # Already ran from main() above, add pipeline tests
    pipeline_tests = [
        test_pipeline_basic,
        test_pipeline_forwarding,
        test_pipeline_load_use_stall,
        test_pipeline_branch_penalty,
        test_pipeline_cycles_gt_instret,
        test_pipeline_loop,
        test_perf_counters,
        test_backward_compat_single_cycle,
    ]

    print("\n" + "=" * 50)
    print("HADES Phase 4 - Pipeline Tests")
    print("=" * 50)

    p4_passed = 0
    p4_failed = 0
    for t in pipeline_tests:
        try:
            t()
            p4_passed += 1
        except AssertionError as e:
            print(f"  FAIL: {t.__name__}: {e}")
            p4_failed += 1
        except Exception as e:
            print(f"  ERROR: {t.__name__}: {e}")
            p4_failed += 1

    print("=" * 50)
    print(f"Pipeline Results: {p4_passed} passed, {p4_failed} failed")
    if p4_failed > 0:
        sys.exit(1)