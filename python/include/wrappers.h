#ifndef TASM_WRAPPERS_H
#define TASM_WRAPPERS_H

#include "ImageUtilities.h"
#include "Tasm.h"

namespace p = boost::python;
namespace np = boost::python::numpy;

namespace tasm {
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
    SelectionResults pythonSelect(const std::string &video,
                                  const std::string &label,
                                  unsigned int firstFrameInclusive,
                                  unsigned int lastFrameExclusive) {
        return SelectionResults(select(video, label, firstFrameInclusive, lastFrameExclusive));
    }
};

} // namespace tasm;

#endif //TASM_WRAPPERS_H
