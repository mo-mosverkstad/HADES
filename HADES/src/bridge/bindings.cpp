#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "cpu.h"
#include "pipelined_cpu.h"
#include "multicore.h"

namespace py = pybind11;

PYBIND11_MODULE(hades, m) {
    m.doc() = "HADES: Hardware Attack & Defense Experimental Simulator";

    py::class_<CPU>(m, "CPU")
        .def(py::init<>())
        .def("load_program", &CPU::load_program,
             py::arg("binary"), py::arg("base_addr") = 0x1000)
        .def("load_data", &CPU::load_data,
             py::arg("data"), py::arg("base_addr") = 0x0000)
        .def("run", &CPU::run, py::arg("max_instructions") = 1000000)
        .def("stop", &CPU::stop)
        .def("is_running", &CPU::is_running)
        .def("is_halted", &CPU::is_halted)
        .def("reset", &CPU::reset)
        .def("get_cycles", &CPU::get_cycles)
        .def("get_pc", &CPU::get_pc)
        .def("get_reg", &CPU::get_reg, py::arg("idx"))
        .def("read_mem", &CPU::read_mem, py::arg("addr"), py::arg("len"))
        .def("set_cache_enabled", &CPU::set_cache_enabled, py::arg("enabled"))
        .def("set_miss_penalty", &CPU::set_miss_penalty, py::arg("cycles"))
        .def("get_icache_misses", &CPU::get_icache_misses)
        .def("get_dcache_misses", &CPU::get_dcache_misses)
        .def("set_mem_hierarchy_enabled", &CPU::set_mem_hierarchy_enabled, py::arg("enabled"))
        .def("get_sdram_row_hits", &CPU::get_sdram_row_hits)
        .def("get_sdram_row_misses", &CPU::get_sdram_row_misses)
        .def("set_io_enabled", &CPU::set_io_enabled, py::arg("enabled"))
        .def("get_io_enabled", &CPU::get_io_enabled)
        .def("uart_send", &CPU::uart_send, py::arg("data"))
        .def("uart_recv", &CPU::uart_recv)
        .def("gpio_set_input", &CPU::gpio_set_input, py::arg("value"))
        .def("gpio_get_output", &CPU::gpio_get_output)
        .def("vga_get_framebuffer", &CPU::vga_get_framebuffer)
        .def("vga_get_char_buffer", &CPU::vga_get_char_buffer)
        .def("vga_get_color_buffer", &CPU::vga_get_color_buffer)
        .def("vga_get_char_row", &CPU::vga_get_char_row, py::arg("row"))
        .def("set_mmu_satp", &CPU::set_mmu_satp, py::arg("satp"))
        .def("get_mmu_satp", &CPU::get_mmu_satp)
        .def("mmu_flush_tlb", &CPU::mmu_flush_tlb)
        .def("get_tlb_hits", &CPU::get_tlb_hits)
        .def("get_tlb_misses", &CPU::get_tlb_misses)
        .def("get_page_faults", &CPU::get_page_faults);

    py::class_<PerfCounters>(m, "PerfCounters")
        .def_readonly("mcycle", &PerfCounters::mcycle)
        .def_readonly("minstret", &PerfCounters::minstret)
        .def_readonly("stalls_data", &PerfCounters::stalls_data)
        .def_readonly("stalls_branch", &PerfCounters::stalls_branch);

    py::class_<PipelinedCPU>(m, "PipelinedCPU")
        .def(py::init<>())
        .def("load_program", &PipelinedCPU::load_program,
             py::arg("binary"), py::arg("base_addr") = 0x1000)
        .def("load_data", &PipelinedCPU::load_data,
             py::arg("data"), py::arg("base_addr") = 0x0000)
        .def("run", &PipelinedCPU::run, py::arg("max_instructions") = 1000000)
        .def("stop", &PipelinedCPU::stop)
        .def("is_running", &PipelinedCPU::is_running)
        .def("is_halted", &PipelinedCPU::is_halted)
        .def("reset", &PipelinedCPU::reset)
        .def("get_cycles", &PipelinedCPU::get_cycles)
        .def("get_instret", &PipelinedCPU::get_instret)
        .def("get_pc", &PipelinedCPU::get_pc)
        .def("get_reg", &PipelinedCPU::get_reg, py::arg("idx"))
        .def("read_mem", &PipelinedCPU::read_mem, py::arg("addr"), py::arg("len"))
        .def("get_perf_counters", &PipelinedCPU::get_perf_counters)
        .def("set_cache_enabled", &PipelinedCPU::set_cache_enabled, py::arg("enabled"))
        .def("set_miss_penalty", &PipelinedCPU::set_miss_penalty, py::arg("cycles"))
        .def("get_icache_misses", &PipelinedCPU::get_icache_misses)
        .def("get_dcache_misses", &PipelinedCPU::get_dcache_misses)
        .def("set_mem_hierarchy_enabled", &PipelinedCPU::set_mem_hierarchy_enabled, py::arg("enabled"))
        .def("get_sdram_row_hits", &PipelinedCPU::get_sdram_row_hits)
        .def("get_sdram_row_misses", &PipelinedCPU::get_sdram_row_misses)
        .def("set_io_enabled", &PipelinedCPU::set_io_enabled, py::arg("enabled"))
        .def("get_io_enabled", &PipelinedCPU::get_io_enabled)
        .def("uart_send", &PipelinedCPU::uart_send, py::arg("data"))
        .def("uart_recv", &PipelinedCPU::uart_recv)
        .def("gpio_set_input", &PipelinedCPU::gpio_set_input, py::arg("value"))
        .def("gpio_get_output", &PipelinedCPU::gpio_get_output)
        .def("vga_get_framebuffer", &PipelinedCPU::vga_get_framebuffer)
        .def("vga_get_char_buffer", &PipelinedCPU::vga_get_char_buffer)
        .def("vga_get_color_buffer", &PipelinedCPU::vga_get_color_buffer)
        .def("vga_get_char_row", &PipelinedCPU::vga_get_char_row, py::arg("row"))
        .def("set_mmu_satp", &PipelinedCPU::set_mmu_satp, py::arg("satp"))
        .def("get_mmu_satp", &PipelinedCPU::get_mmu_satp)
        .def("mmu_flush_tlb", &PipelinedCPU::mmu_flush_tlb)
        .def("get_tlb_hits", &PipelinedCPU::get_tlb_hits)
        .def("get_tlb_misses", &PipelinedCPU::get_tlb_misses)
        .def("get_page_faults", &PipelinedCPU::get_page_faults);

    py::class_<MultiCore>(m, "MultiCore")
        .def(py::init<>())
        .def("reset", &MultiCore::reset)
        .def("load_program", &MultiCore::load_program,
             py::arg("core_id"), py::arg("binary"), py::arg("base_addr"))
        .def("load_data", &MultiCore::load_data,
             py::arg("data"), py::arg("base_addr") = 0x0000)
        .def("run", &MultiCore::run, py::arg("max_cycles") = 100000)
        .def("get_reg", &MultiCore::get_reg, py::arg("core_id"), py::arg("idx"))
        .def("get_cycles", &MultiCore::get_cycles, py::arg("core_id"))
        .def("get_instret", &MultiCore::get_instret, py::arg("core_id"))
        .def("get_global_cycles", &MultiCore::get_global_cycles)
        .def("is_halted", &MultiCore::is_halted, py::arg("core_id"))
        .def("read_mem", &MultiCore::read_mem, py::arg("addr"), py::arg("len"))
        .def("get_mutex_contentions", &MultiCore::get_mutex_contentions)
        .def("get_mutex_locked", &MultiCore::get_mutex_locked)
        .def("get_mutex_owner", &MultiCore::get_mutex_owner)
        .def("uart_send", &MultiCore::uart_send, py::arg("data"))
        .def("uart_recv", &MultiCore::uart_recv)
        .def("gpio_set_input", &MultiCore::gpio_set_input, py::arg("value"))
        .def("gpio_get_output", &MultiCore::gpio_get_output);
}
