    .text
    .global _start

# ═══════════════════════════════════════════════════════════════════
# HADES Phase 12: "Guess the Number" Game
#
# The CPU picks a secret number (1-9).
# Player sends guesses via UART (ASCII digits '1'-'9').
# VGA displays: prompt, guesses, and "TOO LOW" / "TOO HIGH" / "WIN!"
#
# I/O addresses:
#   UART DATA:  0xF020 (read = pop RX, write = push TX)
#   VGA BASE:   0xF080
#     +0x00 CONTROL (write 3 = char mode + enable)
#     +0x08 CURSOR_X
#     +0x0C CURSOR_Y
#     +0x18 CHAR_WRITE (write char, auto-advance cursor)
#
# Register usage:
#   s0 = secret number (fixed at 5 for determinism)
#   s1 = current VGA row (for output)
#   t4 = UART base (0xF020)
#   t5 = VGA base (0xF080)
#   t0-t3 = temporaries
# ═══════════════════════════════════════════════════════════════════

_start:
    # ─── Setup I/O addresses ───
    lui   t4, 0xF            # t4 = 0xF000
    addi  t4, t4, 0x020      # t4 = 0xF020 (UART)
    lui   t5, 0xF            # t5 = 0xF000
    addi  t5, t5, 0x080      # t5 = 0xF080 (VGA)

    # ─── Init VGA: char mode + enable ───
    li    t0, 3              # CONTROL = char_mode(1) | enable(2)
    sw    t0, 0x00(t5)       # VGA CONTROL
    sw    zero, 0x08(t5)     # cursor_x = 0
    sw    zero, 0x0C(t5)     # cursor_y = 0

    # ─── Print title: "GUESS 1-9" ───
    li    t0, 'G'
    sw    t0, 0x18(t5)
    li    t0, 'U'
    sw    t0, 0x18(t5)
    li    t0, 'E'
    sw    t0, 0x18(t5)
    li    t0, 'S'
    sw    t0, 0x18(t5)
    li    t0, 'S'
    sw    t0, 0x18(t5)
    li    t0, ' '
    sw    t0, 0x18(t5)
    li    t0, '1'
    sw    t0, 0x18(t5)
    li    t0, '-'
    sw    t0, 0x18(t5)
    li    t0, '9'
    sw    t0, 0x18(t5)

    # ─── Set secret number ───
    li    s0, 5              # secret = 5

    # ─── Set output row ───
    li    s1, 1              # start output on row 1

    # ═══════════════════════════════════════════════════════════════
    # Main game loop: read guess from UART, compare, display result
    # ═══════════════════════════════════════════════════════════════

game_loop:
    # ─── Read from UART ───
    lw    t0, 0(t4)          # read UART DATA register
    srli  t1, t0, 15         # shift RVALID (bit 15) to bit 0
    andi  t1, t1, 1          # isolate it
    beqz  t1, game_loop      # no data available → spin

    # ─── Extract ASCII digit ───
    andi  t2, t0, 0xFF       # t2 = received byte (ASCII)

    # ─── Move cursor to output row ───
    sw    zero, 0x08(t5)     # cursor_x = 0
    sw    s1, 0x0C(t5)       # cursor_y = s1

    # ─── Display ">" + guess char ───
    li    t0, '>'
    sw    t0, 0x18(t5)
    sw    t2, 0x18(t5)       # display the guess digit
    li    t0, ' '
    sw    t0, 0x18(t5)

    # ─── Convert ASCII to number ───
    addi  t3, t2, -48        # t3 = digit value (ASCII '0' = 48)

    # ─── Compare with secret ───
    beq   t3, s0, win        # guess == secret → WIN
    blt   t3, s0, too_low    # guess < secret → TOO LOW

    # ─── TOO HIGH ───
    li    t0, 'H'
    sw    t0, 0x18(t5)
    li    t0, 'I'
    sw    t0, 0x18(t5)
    li    t0, 'G'
    sw    t0, 0x18(t5)
    li    t0, 'H'
    sw    t0, 0x18(t5)
    j     next_round

too_low:
    li    t0, 'L'
    sw    t0, 0x18(t5)
    li    t0, 'O'
    sw    t0, 0x18(t5)
    li    t0, 'W'
    sw    t0, 0x18(t5)
    j     next_round

win:
    li    t0, 'W'
    sw    t0, 0x18(t5)
    li    t0, 'I'
    sw    t0, 0x18(t5)
    li    t0, 'N'
    sw    t0, 0x18(t5)
    li    t0, '!'
    sw    t0, 0x18(t5)

    # Echo 'W' to UART so Python knows we won
    li    t0, 'W'
    sw    t0, 0(t4)          # UART TX

    ecall                    # halt on win

next_round:
    addi  s1, s1, 1          # next row
    j     game_loop