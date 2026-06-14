#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "cpu.h"
#include "pipelined_cpu.h"
#include "cpu_concept.h"

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
        .def("read_mem", &CPU::read_mem, py::arg("addr"), py::arg("len"));

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
        .def("get_perf_counters", &PipelinedCPU::get_perf_counters);
}