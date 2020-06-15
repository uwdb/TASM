#include "TransformToImage.h"

#include "ColorSpace.h"
#include "NvCodecUtils.h"
#include <fstream>

namespace tasm {

void GetImage(CUdeviceptr dpSrc, uint8_t *pDst, int nWidth, int nHeight, int srcXOffset, int srcYOffset, int srcPitch)
{
    CUDA_MEMCPY2D m;
    memset(&m, 0, sizeof(m));
    m.WidthInBytes = nWidth;
    m.Height = nHeight;
    m.srcMemoryType = CU_MEMORYTYPE_DEVICE;
    m.srcDevice = (CUdeviceptr)dpSrc;
    m.srcPitch = srcPitch;
    m.srcXInBytes = srcXOffset;
    m.srcY = srcYOffset;
    m.dstMemoryType = CU_MEMORYTYPE_HOST;
    m.dstDevice = (CUdeviceptr)(m.dstHost = pDst);
    m.dstPitch = m.WidthInBytes;
    cuMemcpy2D(&m);
}

std::optional<std::unique_ptr<std::vector<ImagePtr>>> TransformToImage::next() {
    if (isComplete_)
        return std::nullopt;

    auto objectPixels = parent_->next();
    if (parent_->isComplete()) {
        isComplete_ = true;
        return std::nullopt;
    }
    assert(objectPixels.has_value());

    auto images = std::make_unique<std::vector<ImagePtr>>();

    for (auto object : **objectPixels) {
        if (!tmpImage_) {
            auto result = cuMemAlloc(&tmpImage_, maxWidth_ * maxHeight_ * numChannels_);
            assert(result == CUDA_SUCCESS);
        }

        auto width = object->width();
        auto height = object->height();
        auto frameSize = width * height * numChannels_;

        // TODO: It's inefficient to do this work for every bounding box in the same tile.
        Nv12ToColor32<RGBA32>((uint8_t *)object->handle(), object->pitch(), (uint8_t *)tmpImage_, tmpImagePitch_, maxWidth_, maxHeight_);

        std::unique_ptr<uint8_t[]> pImage(new uint8_t[frameSize]);
        GetImage(tmpImage_, reinterpret_cast<uint8_t*>(pImage.get()), numChannels_ * width, height, numChannels_ * object->xOffset(), object->yOffset(), tmpImagePitch_);

        images->emplace_back(std::make_unique<Image>(width, height, std::move(pImage)));
    }
    return images;
}

} // namespace tasm