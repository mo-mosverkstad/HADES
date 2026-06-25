
    .text
    .global _start

# ═══════════════════════════════════════════════════════════════════
# HADES Phase 13: Echo Terminal with Color Support
#
# Characters received from UART are displayed on VGA with color.
# Special commands (start with '$'):
#   $r -> set color to red (1)
#   $g -> set color to green (2)
#   $b -> set color to blue (3)
#   $y -> set color to yellow (4)
#   $c -> set color to cyan (5)
#   $m -> set color to magenta (6)
#   $- -> reset to white/default (0)
#   $q -> quit (halt CPU)
#
# VGA registers:
#   0xF080 + 0x00 CONTROL
#   0xF080 + 0x08 CURSOR_X
#   0xF080 + 0x0C CURSOR_Y
#   0xF080 + 0x18 CHAR_WRITE (writes char with current color)
#   0xF080 + 0x1C COLOR_ATTR (set color: 0=white,1=red,2=green,3=blue,4=yellow,5=cyan,6=magenta)
#
# Register usage:
#   s0 = cursor X
#   s1 = cursor Y
#   s2 = state (0=normal, 1=waiting for command char after '$')
#   t4 = UART base (0xF020)
#   t5 = VGA base (0xF080)
# ═══════════════════════════════════════════════════════════════════

_start:
    # Setup I/O addresses
    lui   t4, 0xF
    addi  t4, t4, 0x020      # t4 = 0xF020 (UART)
    lui   t5, 0xF
    addi  t5, t5, 0x080      # t5 = 0xF080 (VGA)

    # Init VGA: char mode + enable
    li    t0, 3
    sw    t0, 0x00(t5)

    # Init cursor
    li    s0, 0
    li    s1, 0
    sw    s0, 0x08(t5)
    sw    s1, 0x0C(t5)

    # Init color = 0 (white/default)
    sw    zero, 0x1C(t5)

    # Init state = 0 (normal)
    li    s2, 0

# ═══════════════════════════════════════════════════════════════════
# Main loop
# ═══════════════════════════════════════════════════════════════════

echo_loop:
    # Read UART
    lw    t0, 0(t4)
    srli  t1, t0, 15
    andi  t1, t1, 1
    beqz  t1, echo_loop      # spin if no data

    andi  t2, t0, 0xFF       # t2 = received char

    # Echo back to UART TX
    sw    t2, 0(t4)

    # Check state: are we in command mode?
    bnez  s2, handle_command

    # ─── Normal mode ───

    # Check for '$' (enter command mode)
    li    t3, '$'
    beq   t2, t3, enter_command

    # Check for newline
    li    t3, 0x0A
    beq   t2, t3, handle_newline
    li    t3, 0x0D
    beq   t2, t3, handle_newline

    # Check for backspace
    li    t3, 0x08
    beq   t2, t3, handle_backspace

    # Normal char: write to VGA
    sw    s0, 0x08(t5)       # cursor_x
    sw    s1, 0x0C(t5)       # cursor_y
    sw    t2, 0x18(t5)       # char_write (uses current color)

    # Advance cursor
    addi  s0, s0, 1
    li    t3, 80
    blt   s0, t3, echo_loop
    li    s0, 0
    addi  s1, s1, 1
    li    t3, 60
    blt   s1, t3, echo_loop
    li    s1, 0
    j     echo_loop

enter_command:
    li    s2, 1              # set state = command mode
    j     echo_loop

# ─── Command mode: next char determines action ───
handle_command:
    li    s2, 0              # reset state to normal

    # $q -> quit
    li    t3, 'q'
    beq   t2, t3, halt

    # $r -> red (1)
    li    t3, 'r'
    beq   t2, t3, set_red

    # $g -> green (2)
    li    t3, 'g'
    beq   t2, t3, set_green

    # $b -> blue (3)
    li    t3, 'b'
    beq   t2, t3, set_blue

    # $y -> yellow (4)
    li    t3, 'y'
    beq   t2, t3, set_yellow

    # $c -> cyan (5)
    li    t3, 'c'
    beq   t2, t3, set_cyan

    # $m -> magenta (6)
    li    t3, 'm'
    beq   t2, t3, set_magenta

    # $- -> default (0)
    li    t3, '-'
    beq   t2, t3, set_default

    # Unknown command char: ignore
    j     echo_loop

set_red:
    li    t0, 1
    sw    t0, 0x1C(t5)
    j     echo_loop
set_green:
    li    t0, 2
    sw    t0, 0x1C(t5)
    j     echo_loop
set_blue:
    li    t0, 3
    sw    t0, 0x1C(t5)
    j     echo_loop
set_yellow:
    li    t0, 4
    sw    t0, 0x1C(t5)
    j     echo_loop
set_cyan:
    li    t0, 5
    sw    t0, 0x1C(t5)
    j     echo_loop
set_magenta:
    li    t0, 6
    sw    t0, 0x1C(t5)
    j     echo_loop
set_default:
    sw    zero, 0x1C(t5)
    j     echo_loop

handle_newline:
    li    s0, 0
    addi  s1, s1, 1
    li    t3, 60
    blt   s1, t3, echo_loop
    li    s1, 0
    j     echo_loop

handle_backspace:
    beqz  s0, echo_loop
    addi  s0, s0, -1
    sw    s0, 0x08(t5)
    sw    s1, 0x0C(t5)
    li    t0, 0x20
    sw    t0, 0x18(t5)
    j     echo_loop

halt:
    ecall