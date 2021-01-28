#include <boost/python.hpp>
#include <boost/python/numpy.hpp>
#include <boost/python/operators.hpp>

#include "TasmWrappers.h"
#include "WorkloadWrappers.h"

#include "TileLayout.h"

namespace p = boost::python;
namespace np = boost::python::numpy;

tasm::python::SelectionResults (tasm::python::PythonTASM::*selectRange)(const std::string&, const std::string&, unsigned int, unsigned int) = &tasm::python::PythonTASM::pythonSelect;
tasm::python::SelectionResults (tasm::python::PythonTASM::*selectEqual)(const std::string&, const std::string&, unsigned int) = &tasm::python::PythonTASM::pythonSelect;
tasm::python::SelectionResults (tasm::python::PythonTASM::*selectAll)(const std::string&, const std::string&) = &tasm::python::PythonTASM::pythonSelect;
tasm::python::SelectionResults (tasm::python::PythonTASM::*selectRangeWithMetadataID)(const std::string&, const std::string&, const std::string&, unsigned int, unsigned int) = &tasm::python::PythonTASM::pythonSelect;
tasm::python::SelectionResults (tasm::python::PythonTASM::*selectEqualWithMetadataID)(const std::string&, const std::string&, const std::string&, unsigned int) = &tasm::python::PythonTASM::pythonSelect;
tasm::python::SelectionResults (tasm::python::PythonTASM::*selectAllWithMetadataID)(const std::string&, const std::string&, const std::string&) = &tasm::python::PythonTASM::pythonSelect;
void (tasm::python::PythonTASM::*storeForceNonUniformLayout)(const std::string&, const std::string&, const std::string&, const std::string&) = &tasm::python::PythonTASM::pythonStoreWithNonUniformLayout;
void (tasm::python::PythonTASM::*storeDoNotForceNonUniformLayout)(const std::string&, const std::string&, const std::string&, const std::string&, bool) = &tasm::python::PythonTASM::pythonStoreWithNonUniformLayout;
void (tasm::python::PythonTASM::*activateRegretBasedTilingWithoutMetadataIdentifier)(const std::string&) = &tasm::python::PythonTASM::pythonActivateRegretBasedTilingForVideo;
void (tasm::python::PythonTASM::*activateRegretBasedTilingWithMetadataIdentifier)(const std::string&, const std::string&) = &tasm::python::PythonTASM::pythonActivateRegretBasedTilingForVideo;
void (tasm::python::PythonTASM::*activateRegretBasedTilingWithThreshold)(const std::string&, const std::string&, double) = &tasm::python::PythonTASM::pythonActivateRegretBasedTilingForVideo;

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

    class_<tasm::MetadataInfo>("MetadataInfo", init<std::string, std::string, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int>())
            .def_readonly("video", &tasm::MetadataInfo::video)
            .def_readonly("label", &tasm::MetadataInfo::label)
            .def_readonly("frame", &tasm::MetadataInfo::frame)
            .def_readonly("x1", &tasm::MetadataInfo::x1)
            .def_readonly("y1", &tasm::MetadataInfo::y1)
            .def_readonly("x2", &tasm::MetadataInfo::x2)
            .def_readonly("y2", &tasm::MetadataInfo::y2);

    class_<tasm::TASM, boost::noncopyable>("BaseTASM", no_init);

    def("tasm_from_db", &tasm::python::tasmFromWH, return_value_policy<manage_new_object>());
    def("set_resources_path", &tasm::python::setCatalogPath);

    class_<tasm::python::PythonTASM, std::shared_ptr<tasm::python::PythonTASM>, bases<tasm::TASM>, boost::noncopyable>("TASM")
        .def("add_metadata", &tasm::python::PythonTASM::addMetadata)
        .def("add_bulk_metadata", &tasm::python::PythonTASM::addBulkMetadataFromList)
        .def("store", &tasm::python::PythonTASM::store)
        .def("store_with_uniform_layout", &tasm::python::PythonTASM::storeWithUniformLayout)
        .def("store_with_nonuniform_layout", storeForceNonUniformLayout)
        .def("store_with_nonuniform_layout", storeDoNotForceNonUniformLayout)
        .def("select", selectRange)
        .def("select", selectEqual)
        .def("select", selectAll)
        .def("select", selectRangeWithMetadataID)
        .def("select", selectEqualWithMetadataID)
        .def("select", selectAllWithMetadataID)
        .def("select_tiles", &tasm::python::PythonTASM::pythonSelectTiles)
        .def("activate_regret_based_tiling", activateRegretBasedTilingWithMetadataIdentifier)
        .def("activate_regret_based_tiling", activateRegretBasedTilingWithoutMetadataIdentifier)
        .def("activate_regret_based_tiling", activateRegretBasedTilingWithThreshold)
        .def("deactivate_regret_based_tiling", &tasm::python::PythonTASM::deactivateRegretBasedTilingForVideo)
        .def("retile_based_on_regret", &tasm::python::PythonTASM::retileVideoBasedOnRegret);

    class_<tasm::python::Query>("Query", init<std::string, std::string, unsigned int, unsigned int>())
        .def(init<std::string, std::string>())
        .def(init<std::string, std::string, unsigned int>())
        .def(init<std::string, std::string, std::string>())
        .def(init<std::string, std::string, std::string, unsigned int, unsigned int>());

    class_<tasm::python::PythonWorkload>("Workload", init<list, list>())
        .def(init<tasm::python::Query>())
        .def("counts", &tasm::python::PythonWorkload::list_counts);

    class_<tasm::TileLayout, std::shared_ptr<tasm::TileLayout>>("BaseTileLayout", no_init)
        .def("num_rows", &tasm::TileLayout::numberOfRows)
        .def("num_cols", &tasm::TileLayout::numberOfColumns);

    class_<tasm::python::PythonTileLayout, std::shared_ptr<tasm::python::PythonTileLayout>, bases<tasm::TileLayout>>("TileLayout",  no_init)
        .def("heights_of_rows", &tasm::python::PythonTileLayout::heightsAsList)
        .def("widths_of_cols", &tasm::python::PythonTileLayout::widthsAsList);

    class_<tasm::python::PythonTiledVideo>("TiledVideo", init<std::string, std::string, unsigned int>())
         .def("gop_length", &tasm::python::PythonTiledVideo::gopLength)
         .def("layout_for_frame", &tasm::python::PythonTiledVideo::tileLayoutForFrame);

    class_<tasm::CostElements>("CostElements", init<unsigned int, unsigned int>())
        .def_readonly("num_pixels", &tasm::CostElements::numPixels)
        .def_readonly("num_tiles", &tasm::CostElements::numTiles)
        .def(self_ns::str(self_ns::self))
        .def(self_ns::repr(self_ns::self));

    class_<tasm::python::PythonWorkloadCostEstimator>("WorkloadCostEstimator", no_init)
        .def("estimate_cost", &tasm::python::PythonWorkloadCostEstimator::estimateCostForWorkload)
        .staticmethod("estimate_cost");
}