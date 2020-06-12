#ifndef TASM_DECODEDPIXELDATA_H
#define TASM_DECODEDPIXELDATA_H

#include "EncodedData.h"

namespace tasm {

class GPUPixelData {
public:
    virtual CUdeviceptr handle() const = 0;

    virtual unsigned int pitch() const = 0;

    virtual unsigned int width() const = 0;

    virtual unsigned int height() const = 0;

    virtual unsigned int xOffset() const = 0;

    virtual unsigned int yOffset() const = 0;

    virtual ~GPUPixelData() = default;
};

using GPUPixelDataPtr = std::shared_ptr<GPUPixelData>;
using GPUPixelDataContainer = std::unique_ptr<std::vector<GPUPixelDataPtr>>;

class GPUPixelDataFromDecodedFrame : public GPUPixelData {
public:
    GPUPixelDataFromDecodedFrame(GPUFramePtr frame, unsigned int width, unsigned int height,
                                 unsigned int xOffset, unsigned int yOffset)
            : frame_(frame), width_(width), height_(height),
              xOffset_(xOffset), yOffset_(yOffset) {}

    CUdeviceptr handle() const override { return frame_->cuda()->handle(); }
    unsigned int pitch() const override { return frame_->cuda()->pitch(); }
    unsigned int width() const override { return width_; }
    unsigned int height() const override { return height_; }
    unsigned int xOffset() const override { return xOffset_; }
    unsigned int yOffset() const override { return yOffset_; }

private:
    GPUFramePtr frame_;
    unsigned int width_;
    unsigned int height_;
    unsigned int xOffset_;
    unsigned int yOffset_;
};

} // namespace tasm

#endif //TASM_DECODEDPIXELDATA_H
