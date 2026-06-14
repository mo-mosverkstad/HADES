# Timer (Interval Timer)

A countdown counter with programmable period and interrupt generation. Provides precise time measurement — essential for timing attacks and system scheduling.

## Core Operation

```
Load period → start → counter decrements each cycle → reaches 0 → event/reload
```

## Register Map

### Control Register

| Bit | Field | Description |
|-----|-------|-------------|
| START | - | Start timer |
| STOP | - | Stop timer |
| CONT | - | Continuous mode (auto-reload) |
| ITO | - | Enable interrupt on timeout |

### Status Register

| Bit | Field | Description |
|-----|-------|-------------|
| RUN | - | Timer is running |
| TO | - | Timeout occurred |

### Period Registers

Define the countdown value (32 or 64-bit counter).

### Snapshot Registers

Safely capture current counter value (avoids mid-read changes).

## Modes of Operation

| Mode | Behavior |
|------|----------|
| One-shot | Count to 0, then stop |
| Continuous | Count to 0, reload period, repeat |

```cpp
if (counter == 0) {
    if (continuous)
        counter = period;
    else
        running = false;
}
```

## Interrupt Behavior

When counter reaches zero and ITO is enabled:
```cpp
if (counter == 0) {
    status.TO = 1;
    if (control.ITO)
        cpu.interrupt();
}
```

Clear by writing to the TO status bit.

## Timer Frequency

Timer ticks at CPU clock rate:
- Clock = 30 MHz → 1 tick = ~33 ns

## Snapshot Usage

```cpp
snap = counter; // atomic capture
return snap;
```

## Watchdog Mode

If enabled, timeout triggers system reset:
```cpp
if (watchdog && counter == 0)
    cpu.reset();
```

## HADES Implementation

```cpp
struct Timer {
    uint64_t counter;
    uint64_t period;
    bool running;
    bool continuous;
    bool irq_enable;
};

void tick() {
    if (!running) return;
    counter--;
    if (counter == 0) {
        if (irq_enable) raise_irq();
        if (continuous) counter = period;
        else running = false;
    }
}
```

Integration with CPU loop:
```cpp
each cycle:
    cpu.step();
    timer.tick();
```

## Measurement Usage

```
Start timer → run code → read timer → elapsed cycles
```

This is the primary mechanism for timing attack measurements.

## Security Relevance

- **Timing attacks**: Precise measurement of execution time differences
- **Interrupt-based leakage**: Timer interrupts disturb CPU, creating observable patterns
- **Synchronization**: Provides attack trigger and trace alignment
- **Side-channel extraction**: Crypto operation → timing difference → key recovery
