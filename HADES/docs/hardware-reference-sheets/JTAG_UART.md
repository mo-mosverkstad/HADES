# JTAG UART

Serial communication channel between the CPU and a host PC via JTAG. Provides console I/O and host-CPU interaction.

Data flow:
```
CPU writes → write FIFO → host reads
Host writes → read FIFO → CPU reads
```

## Register Map

| Offset | Register |
|--------|----------|
| 0 | DATA |
| 4 | CONTROL |

Memory-mapped at `0x4000040` on DTEK-V.

## DATA Register

| Bits | Field | Description |
|------|-------|-------------|
| [7:0] | DATA | Data byte |
| [15] | RVALID | 1 if read data is valid |
| [31:16] | RAVAIL | Bytes remaining in read FIFO |

- **Read**: Returns next byte from read FIFO with validity and count metadata
- **Write**: Pushes byte into write FIFO for host consumption

### HADES Implementation

```cpp
struct JTAG_UART {
    std::queue<uint8_t> read_fifo;
    std::queue<uint8_t> write_fifo;
    uint32_t control;

    uint32_t read_data() {
        if (read_fifo.empty()) return 0;
        uint8_t v = read_fifo.front();
        read_fifo.pop();
        return v | (1 << 15); // RVALID
    }

    void write_data(uint8_t v) {
        write_fifo.push(v);
    }
};
```

## CONTROL Register

| Bit | Field | Description |
|-----|-------|-------------|
| 0 | RE | Enable read interrupt |
| 1 | WE | Enable write interrupt |
| 8 | RI | Read interrupt pending |
| 9 | WI | Write interrupt pending |
| 10 | AC | Activity flag (host connected) |
| [31:16] | WSPACE | Free space in write FIFO |

### Interrupt Logic

```cpp
if ((control.RE && RI) || (control.WE && WI))
    trigger_interrupt();
```

## FIFO System

| FIFO | Direction | Purpose |
|------|-----------|---------|
| Read FIFO | Host → CPU | Input from host |
| Write FIFO | CPU → Host | Output to host |

- Limited size (e.g., 64 bytes)
- Overflow drops data

```cpp
const int FIFO_SIZE = 64;
if (write_fifo.size() >= FIFO_SIZE)
    drop_data();
```

## HADES Host Interface

In HADES, the host side is replaced by Python:
```
Python input → read_fifo
write_fifo → Python console
```

This enables interactive programs, printf debugging, logging, and attack data extraction.
