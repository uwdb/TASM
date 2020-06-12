#include <boost/python.hpp>
#include <boost/python/numpy.hpp>

#include "wrappers.h"

namespace p = boost::python;
namespace np = boost::python::numpy;

tasm::python::SelectionResults (tasm::python::PythonTASM::*selectRange)(const std::string&, const std::string&, unsigned int, unsigned int) = &tasm::python::PythonTASM::pythonSelect;
tasm::python::SelectionResults (tasm::python::PythonTASM::*selectEqual)(const std::string&, const std::string&, unsigned int) = &tasm::python::PythonTASM::pythonSelect;
tasm::python::SelectionResults (tasm::python::PythonTASM::*selectAll)(const std::string&, const std::string&) = &tasm::python::PythonTASM::pythonSelect;
tasm::python::SelectionResults (tasm::python::PythonTASM::*selectRangeWithMetadataID)(const std::string&, const std::string&, const std::string&, unsigned int, unsigned int) = &tasm::python::PythonTASM::pythonSelect;
tasm::python::SelectionResults (tasm::python::PythonTASM::*selectEqualWithMetadataID)(const std::string&, const std::string&, const std::string&, unsigned int) = &tasm::python::PythonTASM::pythonSelect;
tasm::python::SelectionResults (tasm::python::PythonTASM::*selectAllWithMetadataID)(const std::string&, const std::string&, const std::string&) = &tasm::python::PythonTASM::pythonSelect;

BOOST_PYTHON_MODULE(_tasm) {
    using namespace boost::python;

    Py_Initialize();
    np::initialize();

    class_<tasm::python::PythonImage>("Image", no_init)
            .def("is_empty", &tasm::python::PythonImage::isEmpty)
            .def("width", &tasm::python::PythonImage::width)
            .def("height", &tasm::python::PythonImage::height)
            .def("array", &tasm::python::PythonImage::array);

    class_<tasm::python::SelectionResults>("ObjectIterator", no_init)
            .def("next", &tasm::python::SelectionResults::next);

    class_<tasm::python::PythonTASM, boost::noncopyable>("TASM")
        .def("add_metadata", &tasm::python::PythonTASM::addMetadata)
        .def("store", &tasm::python::PythonTASM::store)
        .def("store_with_uniform_layout", &tasm::python::PythonTASM::storeWithUniformLayout)
        .def("select", selectRange)
        .def("select", selectEqual)
        .def("select", selectAll)
        .def("select", selectRangeWithMetadataID)
        .def("select", selectEqualWithMetadataID)
        .def("select", selectAllWithMetadataID);
}