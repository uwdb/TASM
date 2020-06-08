#include <boost/python.hpp>

#include "Tasm.h"

BOOST_PYTHON_MODULE(pyTasm) {
    using namespace boost::python;
    class_<tasm::TASM>("TASM")
        .def("addMetadata", &tasm::TASM::addMetadata);
}