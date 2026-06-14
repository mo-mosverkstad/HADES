#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "cpu.h"

namespace py = pybind11;

PYBIND11_MODULE(hades, m) {
    m.doc() = "HADES: Hardware Attack & Defense Experimental Simulator";

    py::enum_<LeakageModel>(m, "LeakageModel")
        .value("HAMMING_WEIGHT", LeakageModel::HAMMING_WEIGHT)
        .value("HAMMING_DISTANCE", LeakageModel::HAMMING_DISTANCE);

    py::class_<CPU>(m, "CPU")
        .def(py::init<>())
        .def("load_program", &CPU::load_program,
             py::arg("binary"), py::arg("base_addr") = 0x1000)
        .def("load_data", &CPU::load_data,
             py::arg("data"), py::arg("base_addr") = 0x0000)
        .def("run", &CPU::run, py::arg("max_instructions") = 1000000)
        .def("reset", &CPU::reset)
        .def("get_power_trace", &CPU::get_power_trace)
        .def("get_cycles", &CPU::get_cycles)
        .def("get_pc", &CPU::get_pc)
        .def("get_reg", &CPU::get_reg, py::arg("idx"))
        .def("read_mem", &CPU::read_mem, py::arg("addr"), py::arg("len"))
        .def("set_leakage_model", &CPU::set_leakage_model, py::arg("model"))
        .def("set_noise", &CPU::set_noise, py::arg("stddev"))
        .def("set_seed", &CPU::set_seed, py::arg("seed"));
}