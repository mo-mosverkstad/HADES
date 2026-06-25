"""
AssemblyLoader: assembles .S files and loads into CPU
"""
import os
import subprocess
import tempfile

# ═══════════════════════════════════════════════════════════════════════════
# AssemblyLoader: assembles .S files and loads into CPU
# ═══════════════════════════════════════════════════════════════════════════

class AssemblyLoader:
    """Assembles RISC-V .S files using the cross-compiler and loads into CPU."""

    def __init__(self, link_script=None):
        if link_script is None:
            self.link_script = os.path.join(
                os.path.dirname(__file__), '..', 'programs', 'link.ld')
        else:
            self.link_script = link_script

    def check_toolchain(self):
        """Check if riscv64-unknown-elf-gcc is available. Returns True/False."""
        try:
            result = subprocess.run(['riscv64-unknown-elf-gcc', '--version'],
                                   capture_output=True, text=True)
            return result.returncode == 0
        except FileNotFoundError:
            return False

    def assemble(self, asm_file):
        """Assemble .S file to raw binary. Returns bytes or None on failure."""
        with tempfile.TemporaryDirectory() as tmpdir:
            elf_file = os.path.join(tmpdir, 'output.elf')
            bin_file = os.path.join(tmpdir, 'output.bin')

            # Assemble + link
            result = subprocess.run([
                'riscv64-unknown-elf-gcc',
                '-march=rv32i', '-mabi=ilp32', '-nostdlib',
                '-T', self.link_script,
                '-o', elf_file, asm_file
            ], capture_output=True, text=True)

            if result.returncode != 0:
                print(f"Assembly loader error: Assembly failed:\n{result.stderr}")
                return None

            # Convert to binary
            result = subprocess.run([
                'riscv64-unknown-elf-objcopy', '-O', 'binary',
                elf_file, bin_file
            ], capture_output=True, text=True)

            if result.returncode != 0:
                print(f"Assembly loader error: objcopy failed:\n{result.stderr}")
                return None

            with open(bin_file, 'rb') as f:
                return list(f.read())

    def load(self, cpu, asm_file, base_addr=0x1000):
        """Assemble and load into CPU. Returns binary size or 0 on failure."""
        binary = self.assemble(asm_file)
        if binary is None:
            return 0
        cpu.load_program(binary, base_addr)
        return len(binary)