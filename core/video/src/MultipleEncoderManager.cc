#include "MultipleEncoderManager.h"

namespace lightdb {

void TileEncoder::updateConfiguration(unsigned int newWidth, unsigned int newHeight) {
    if (newWidth == width_ && newHeight == height_)
        return;

    width_ = newWidth;
    height_ = newHeight;

    NvEncPictureCommand picCommand;
    picCommand.bResolutionChangePending = true;
    picCommand.newWidth = newWidth;
    picCommand.newHeight = newHeight;

    auto result = encoder_.api().NvEncReconfigureEncoder(&picCommand);
    assert(result == NV_ENC_SUCCESS);
}

std::unique_ptr<bytestring> TileEncoder::getEncodedFrames() {
    unsigned int numberOfFrames;
    return writer_.dequeueToPtr(numberOfFrames);
}

void TileEncoder::encodeFrame(Frame &frame, unsigned int top, unsigned int left, bool isKeyframe) {
    encodeSession_.Encode(frame, top, left, isKeyframe);
}

void TileEncoder::flush() {
    encodeSession_.Flush();
}

} // namespace lightdb