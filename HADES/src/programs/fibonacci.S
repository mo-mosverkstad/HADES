    .text
    .global _start

# Compute first 10 Fibonacci numbers, store to memory at 0x0000
_start:
    li   t0, 0x00         # base address
    li   t1, 0            # fib(0) = 0
    li   t2, 1            # fib(1) = 1
    li   t3, 10           # count

    sb   t1, 0(t0)        # store fib(0)
    sb   t2, 1(t0)        # store fib(1)
    addi t0, t0, 2        # advance pointer
    addi t3, t3, -2       # count -= 2

loop:
    add  t4, t1, t2       # t4 = fib(n-2) + fib(n-1)
    sb   t4, 0(t0)        # store fib(n)
    mv   t1, t2           # shift
    mv   t2, t4
    addi t0, t0, 1
    addi t3, t3, -1
    bnez t3, loop

    ecall                  # halt