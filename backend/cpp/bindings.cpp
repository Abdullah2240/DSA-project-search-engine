#include <pybind11/pybind.h>
#include <pybind11/stl.h>
#include "engine.hpp"


PYBIND11_MODULE(engine, m) {
    m.def("start", &start, "Searches for the results of a query")
    m.def("fef", &fefe, )
}