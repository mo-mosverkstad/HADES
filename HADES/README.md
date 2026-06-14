# HADES — Hardware Attack & Defense Experimental Simulator

HADES is a RISC-V hardware security simulator that models a complete embedded system with observable side-channels and built for learning computer architecture (IS1200) and hardware security (IL1333).

## What It Does
HADES simulates a RISC-V CPU with pipeline, cache, memory hierarchy, and I/O devices. Every register write emits a power trace sample. This lets you:

- **Run programs** on a simulated RISC-V processor
- **Attack** AES encryption using Correlation Power Analysis (CPA)
- **Observe** pipeline stalls, cache misses, and memory timing
- **Inject faults** and perform Differential Fault Analysis (DFA)
- **Evaluate countermeasures** (masking, constant-time, shuffling)
- **Play games** with UART input and VGA display