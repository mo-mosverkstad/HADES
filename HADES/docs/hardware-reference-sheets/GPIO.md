# GPIO (Parallel I/O)

The PIO core provides memory-mapped general-purpose I/O, connecting the CPU to external devices (LEDs, switches, etc.) via FPGA pins.

## Register Map

| Offset | Register | Purpose |
|--------|----------|---------|
| 0 | data | Read/write I/O value |
| 1 | direction | Input (0) / Output (1) control per pin |
| 2 | interruptmask | Enable IRQ per pin |
| 3 | edgecapture | Detect signal edges |

### HADES Device Structure

```cpp
struct GPIO {
    uint32_t data;
    uint32_t direction;
    uint32_t interruptmask;
    uint32_t edgecapture;
};
```

## Data Register

- **Read**: Returns current input signal (not previously written value)
- **Write**: Drives output value to pins

```cpp
uint32_t read_data() { return input_pins; }
void write_data(uint32_t value) { output_pins = value; }
```

## Direction Register

| Value | Meaning |
|-------|---------|
| 0 | Input (high impedance) |
| 1 | Output (drives pin) |

## Edge Capture Register

Captures signal transitions (rising, falling, or both edges).

- When edge occurs: `edgecapture[i] = 1`
- Cleared by writing `1` to the corresponding bit

```cpp
if (input_changed(i))
    edgecapture |= (1 << i);
```

## Interrupt Generation

Two modes:
- **Level-sensitive**: IRQ if input is HIGH
- **Edge-sensitive**: IRQ if edgecapture bit is set

IRQ condition:
```cpp
if ((edgecapture & interruptmask) != 0)
    trigger_interrupt();
```

## Optional Registers

| Register | Function |
|----------|----------|
| outset | Set individual output bits |
| outclear | Clear individual output bits |

```cpp
output |= outset;
output &= ~outclear;
```

## HADES Memory Integration

```cpp
uint32_t CPU::read(uint32_t addr) {
    if (addr == GPIO_BASE) return gpio.read_data();
}

void CPU::write(uint32_t addr, uint32_t value) {
    if (addr == GPIO_BASE) gpio.write_data(value);
}
```

## Security Relevance

- **Timing leakage**: Interrupts introduce non-deterministic execution time
- **Fault injection**: External signals can flip input bits
- **Attack triggering**: GPIO serves as synchronization/trigger signal for measurements
