"""
HADES - CPU Runner
Loads a RISC-V binary and executes it on the simulated CPU.
"""
import sys
import argparse

try:
    import hades
except ImportError:
    print("ERROR: hades module not found. Run 'make engine' first.")
    sys.exit(1)


def load_binary(path):
    with open(path, 'rb') as f:
        return list(f.read())


def main():
    parser = argparse.ArgumentParser(description='HADES CPU Runner')
    parser.add_argument('binary', help='Path to RISC-V binary (.bin)')
    parser.add_argument('--data', help='Path to data file to load at 0x0000')
    parser.add_argument('--max-instr', type=int, default=1000000, help='Max instructions')
    parser.add_argument('--noise', type=float, default=0.0, help='Leakage noise stddev')
    parser.add_argument('--model', choices=['hw', 'hd'], default='hw', help='Leakage model')
    parser.add_argument('--dump-regs', action='store_true', help='Dump registers after execution')
    parser.add_argument('--dump-mem', type=str, help='Dump memory range (addr:len)')
    parser.add_argument('--dump-trace', action='store_true', help='Print power trace')
    args = parser.parse_args()

    cpu = hades.CPU()

    # Configure
    if args.model == 'hd':
        cpu.set_leakage_model(hades.LeakageModel.HAMMING_DISTANCE)

    # Load program
    prog = load_binary(args.binary)
    cpu.load_program(prog)

    # Load data
    if args.data:
        data = load_binary(args.data)
        cpu.load_data(data)

    # Run
    cpu.run(args.max_instr)

    # Output
    print(f"Cycles: {cpu.get_cycles()}")
    print(f"PC:     {cpu.get_pc():#010x}")

    if args.dump_regs:
        print("\nRegisters:")
        names = ['zero', 'ra', 'sp', 'gp', 'tp',
                 't0', 't1', 't2', 's0', 's1',
                 'a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7',
                 's2', 's3', 's4', 's5', 's6', 's7', 's8', 's9', 's10', 's11',
                 't3', 't4', 't5', 't6']
        for i in range(32):
            val = cpu.get_reg(i)
            if val != 0:
                print(f"  x{i:2d} ({names[i]:4s}) = {val:#010x} ({val})")

    if args.dump_mem:
        addr_str, len_str = args.dump_mem.split(':')
        addr = int(addr_str, 0)
        length = int(len_str, 0)
        data = cpu.read_mem(addr, length)
        print(f"\nMemory [{addr:#06x}:{addr+length:#06x}]:")
        for i in range(0, len(data), 16):
            hex_str = ' '.join(f'{b:02x}' for b in data[i:i+16])
            print(f"  {addr+i:#06x}: {hex_str}")


if __name__ == '__main__':
    main()