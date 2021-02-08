#ifndef TASM_WRAPPERS_H
#define TASM_WRAPPERS_H

#include "ImageUtilities.h"
#include "utilities.h"
#include "Tasm.h"
#include "Video.h"
#include <boost/python/numpy.hpp>

namespace p = boost::python;
namespace np = boost::python::numpy;

namespace tasm::python {
class PythonImage {
public:
    PythonImage(ImagePtr image)
            : image_(image) {}

    bool isEmpty() const { return !image_; }
    unsigned int width() const { return image_->width(); }
    unsigned int height() const { return image_->height(); }

    np::ndarray array() { return makeArray(); }

private:
    np::ndarray makeArray() {
        np::dtype dt = np::dtype::get_builtin<uint8_t>();
        p::tuple shape = p::make_tuple(height() * width() * 4);
        p::tuple stride = p::make_tuple(sizeof(uint8_t));
        return np::from_data(image_->pixels(), dt, shape, stride, own_);
    }

    ImagePtr image_;
    p::object own_;
};

class SelectionResults {
public:
    SelectionResults(std::unique_ptr<ImageIterator> imageIterator)
            : imageIterator_(std::move(imageIterator)) {}

    PythonImage next() {
        return PythonImage(imageIterator_->next());
    }

private:
    std::shared_ptr<ImageIterator> imageIterator_;
};

class PythonTASM : public TASM {
public:
    PythonTASM()
        : TASM()
    {}

    PythonTASM(const std::string dbPath, TASM::IndexType indexType)
        : TASM(dbPath, indexType)
    {}

    void addBulkMetadataFromList(boost::python::list metadataInfo) {
        addBulkMetadata(extract<MetadataInfo>(metadataInfo));
    }

    void pythonStoreWithNonUniformLayout(const std::string &videoPath, const std::string &savedName, const std::string &metadataIdentifier, const std::string &labelToTileAround) {
        // If "force" isn't specified, do the tiling.
        storeWithNonUniformLayout(videoPath, savedName, metadataIdentifier, labelToTileAround, true);
    }

    void pythonStoreWithNonUniformLayout(const std::string &videoPath, const std::string &savedName, const std::string &metadataIdentifier, const std::string &labelToTileAround, bool force) {
        storeWithNonUniformLayout(videoPath, savedName, metadataIdentifier, labelToTileAround, force);
    }

    SelectionResults pythonSelect(const std::string &video,
                                       const std::string &label,
                                       unsigned int firstFrameInclusive,
                                       unsigned int lastFrameExclusive) {
        return SelectionResults(select(video, label, firstFrameInclusive, lastFrameExclusive));
    }

    SelectionResults pythonSelect(const std::string &video,
                                  const std::string &label) {
        return SelectionResults(select(video, label));
    }

    SelectionResults pythonSelect(const std::string &video,
                                  const std::string &label,
                                  unsigned int frame) {
        return SelectionResults(select(video, label, frame));
    }

    SelectionResults pythonSelect(const std::string &video,
                                  const std::string &metadataIdentifier,
                                  const std::string &label,
                                  unsigned int firstFrameInclusive,
                                  unsigned int lastFrameExclusive) {
        return SelectionResults(select(video, label, firstFrameInclusive, lastFrameExclusive, metadataIdentifier));
    }

    SelectionResults pythonSelect(const std::string &video,
                                  const std::string &metadataIdentifier,
                                  const std::string &label) {
        return SelectionResults(select(video, label, metadataIdentifier));
    }

    SelectionResults pythonSelect(const std::string &video,
                                  const std::string &metadataIdentifier,
                                  const std::string &label,
                                  unsigned int frame) {
        return SelectionResults(select(video, label, frame, metadataIdentifier));
    }

    SelectionResults pythonSelectTiles(const std::string &video,
                                        const std::string &metadataIdentifier,
                                        const std::string &label,
                                        unsigned int firstFrameInclusive,
                                        unsigned int lastFrameExclusive) {
        return SelectionResults(selectTiles(video, label, firstFrameInclusive, lastFrameExclusive, metadataIdentifier));
    }

    SelectionResults pythonSelectTiles(const std::string &video,
                                       const std::string &metadataIdentifier,
                                       const std::string &label) {
        return SelectionResults(selectTiles(video, label, metadataIdentifier));
    }

    SelectionResults pythonSelectFrames(const std::string &video,
                                        const std::string &metadataIdentifier,
                                        const std::string &label) {
        return SelectionResults(selectFrames(video, label, metadataIdentifier));
    }

    SelectionResults pythonSelectFrames(const std::string &video,
                                        const std::string &metadataIdentifier,
                                        const std::string &label,
                                        unsigned int firstFrameInclusive,
                                        unsigned int lastFrameExclusive) {
        return SelectionResults(selectFrames(video, label, firstFrameInclusive, lastFrameExclusive, metadataIdentifier));
    }

    void pythonActivateRegretBasedTilingForVideo(const std::string &video) {
        return activateRegretBasedTilingForVideo(video);
    }

    void pythonActivateRegretBasedTilingForVideo(const std::string &video,
                                                const std::string &metadataIdentifier) {
        return activateRegretBasedTilingForVideo(video, metadataIdentifier);
    }

    void pythonActivateRegretBasedTilingForVideo(const std::string &video,
                                                 const std::string &metadataIdentifier,
                                                 double threshold) {
        return activateRegretBasedTilingForVideo(video, metadataIdentifier, threshold);
    }

};

PythonTASM *tasmFromWH(const std::string &whDBPath) {
    return new PythonTASM(whDBPath, TASM::IndexType::WH);
}

void setCatalogPath(const std::string &resourcesPath) {
    tasm::CatalogConfiguration::SetCatalogPath(resourcesPath);
}

} // namespace tasm::python;

#endif //TASM_WRAPPERS_H
