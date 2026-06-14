# Hardware Mutex

A hardware mutex device for multi-processor synchronization. Provides atomic test-and-set to protect shared resources.

## Register Map

| Offset | Register | Function |
|--------|----------|----------|
| 0 | mutex | Lock state (OWNER + VALUE) |
| 1 | reset | Reset control |

## Mutex Register Fields

| Field | Description |
|-------|-------------|
| OWNER | CPU ID of current lock holder |
| VALUE | 0 = unlocked, non-zero = locked |

Encoding: `(owner << 16) | value`

## Lock Acquisition (Test-and-Set)

A write succeeds only if:
1. VALUE == 0 (mutex is free), OR
2. OWNER matches the writing CPU

Process:
1. CPU writes its ID as OWNER with VALUE = 1
2. CPU reads back the register
3. If OWNER matches its ID → lock acquired

```cpp
bool try_lock(int cpu_id) {
    if (value == 0 || owner == cpu_id) {
        owner = cpu_id;
        value = 1;
        return true;
    }
    return false;
}
```

## Unlock

Only the owner can release:
```cpp
void unlock(int cpu_id) {
    if (owner == cpu_id)
        value = 0;
}
```

## Access Modes

| Mode | Behavior |
|------|----------|
| trylock (non-blocking) | Attempt once, fail if locked |
| lock (blocking) | Spin until acquired |

## HADES Integration

Memory-mapped (e.g., `0x40000C0` on DTEK-V):
```cpp
uint32_t read() {
    return (owner << 16) | value;
}

void write(uint32_t data) {
    int new_owner = data >> 16;
    int new_value = data & 0xFFFF;
    if (value == 0 || owner == new_owner) {
        owner = new_owner;
        value = new_value;
    }
}
```

Multi-core execution model:
```cpp
for each cycle:
    cpu0.step();
    cpu1.step();
```

## Security Relevance

- **Timing attacks**: Lock contention increases wait time, leaking resource usage patterns
- **Contention side-channel**: Attacker measures lock acquisition delay to infer victim activity
- **Covert channel**: One CPU locks/unlocks while another measures timing
- **Fault injection**: Corrupting owner field breaks synchronization
