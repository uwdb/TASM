#ifndef TASM_IMAGEITERATOR_H
#define TASM_IMAGEITERATOR_H

#include "Operator.h"

#include <memory>

using PixelPtr = uint8_t[];
class Image {
public:
    Image(unsigned int width, unsigned int height, std::unique_ptr<PixelPtr> pixels)
            : width_(width), height_(height), pixels_(std::move(pixels))
    {}

    unsigned int width() const { return width_; }
    unsigned int height() const { return height_; }
    uint8_t* pixels() const { return pixels_.get(); }

private:
    unsigned int width_;
    unsigned int height_;
    std::unique_ptr<PixelPtr> pixels_;
};
using ImagePtr = std::shared_ptr<Image>;

class ImageIterator {
public:
    ImageIterator(std::shared_ptr<Operator<std::unique_ptr<std::vector<ImagePtr>>>> parent)
    : parent_(parent) {}

    ImagePtr next() {
        if (!currentImages_ || imageIterator_ == currentImages_->end())
            getNextSetOfImages();

        if (!currentImages_) {
            // Done.
            return ImagePtr();
        }

        return *imageIterator_++;
    }

private:
    void getNextSetOfImages() {
        currentImages_.reset();
        while (!currentImages_ && !parent_->isComplete()) {
            auto next = parent_->next();
            if (next.has_value() && (**next).size() > 0)
                currentImages_ = std::move(*next);
        }
        if (currentImages_)
            imageIterator_ = currentImages_->begin();
    }

    std::shared_ptr<Operator<std::unique_ptr<std::vector<ImagePtr>>>> parent_;
    std::unique_ptr<std::vector<ImagePtr>> currentImages_;
    std::vector<ImagePtr>::const_iterator imageIterator_;
};

#endif //TASM_IMAGEITERATOR_H
