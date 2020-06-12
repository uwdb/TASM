#include <boost/python.hpp>
#include <boost/python/numpy.hpp>

#include "wrappers.h"

namespace p = boost::python;
namespace np = boost::python::numpy;

BOOST_PYTHON_MODULE(_tasm) {
    using namespace boost::python;

    Py_Initialize();
    np::initialize();

    class_<tasm::PythonImage>("Image", no_init)
            .def("is_empty", &tasm::PythonImage::isEmpty)
            .def("width", &tasm::PythonImage::width)
            .def("height", &tasm::PythonImage::height)
            .def("array", &tasm::PythonImage::array);

    class_<tasm::SelectionResults>("ObjectIterator", no_init)
            .def("next", &tasm::SelectionResults::next);

    class_<tasm::PythonTASM, boost::noncopyable>("TASM")
        .def("addMetadata", &tasm::PythonTASM::addMetadata)
        .def("store", &tasm::PythonTASM::store)
        .def("select", &tasm::PythonTASM::pythonSelect);
}