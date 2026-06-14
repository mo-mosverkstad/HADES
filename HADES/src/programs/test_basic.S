    .text
    .global _start

# Test program: compute sum 1+2+...+10, store result to memory address 0x0020
_start:
    li   t0, 0            # sum = 0
    li   t1, 1            # i = 1
    li   t2, 11           # limit = 11

loop:
    add  t0, t0, t1       # sum += i
    addi t1, t1, 1        # i++
    blt  t1, t2, loop     # if i < 11, continue

    # Store result (55) to address 0x0020
    li   t3, 0x20
    sw   t0, 0(t3)

    # Test XOR
    li   t4, 0xAA
    li   t5, 0x55
    xor  t6, t4, t5       # t6 = 0xFF

    # Store XOR result to 0x0024
    sw   t6, 4(t3)

    # Halt
    ecall