#include <pybind11/pybind.h>
#include <pybind11/stl.h>
#include "engine.hpp"

PYBIND11_MODULE(engine, m) {
    m.def("search", &search, "Searches for results of a query");
    m.def("get_lexicon_stats", &get_lexicon_stats, "Get lexicon statistics");
    m.def("tokenize_query", &tokenize_query, "Tokenize query into word indices");
}