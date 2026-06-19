#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "cpu.h"
#include "pipelined_cpu.h"

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
        .def("gpio_get_output", &CPU::gpio_get_output);

    py::class_<PerfCounters>(m, "PerfCounters")
        .def_readonly("mcycle", &PerfCounters::mcycle)
        .def_readonly("minstret", &PerfCounters::minstret)
        .def_readonly("stalls_data", &PerfCounters::stalls_data)
        .def_readonly("stalls_branch", &PerfCounters::stalls_branch);

    py::class_<PipelinedCPU>(m, "PipelinedCPU")
        .def(py::init<>())
        .def("load_program", &PipelinedCPU::load_program, py::arg("binary"), py::arg("base_addr") = 0x1000)
        .def("load_data", &PipelinedCPU::load_data, py::arg("data"), py::arg("base_addr") = 0x0000)
        .def("run", &PipelinedCPU::run, py::arg("max_instructions") = 1000000)
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
        .def("gpio_get_output", &PipelinedCPU::gpio_get_output);
}
