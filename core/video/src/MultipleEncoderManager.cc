#include "MultipleEncoderManager.h"

namespace lightdb {

void TileEncoder::updateConfiguration(unsigned int newWidth, unsigned int newHeight) {
    NvEncPictureCommand picCommand;
    picCommand.bResolutionChangePending = true;
    picCommand.newWidth = newWidth;
    picCommand.newHeight = newHeight;

    auto result = encoder_.api().NvEncReconfigureEncoder(&picCommand);
    assert(result == NV_ENC_SUCCESS);
}

std::unique_ptr<bytestring> TileEncoder::getEncodedFrames() {
    unsigned int numberOfFrames;
    std::unique_ptr<bytestring> encodedData;
    threadPool_.push([&]() {
        writer_.dequeueToPtr(encodedData, numberOfFrames);
    });
    threadPool_.waitAll();
    return encodedData;
}

void TileEncoder::encodeFrame(GPUFrameReference frame, unsigned int top, unsigned int left, bool isKeyframe) {
    threadPool_.push([=]() {
        encodeSession_.Encode(*frame, top, left, isKeyframe);
    });
}

void TileEncoder::flush() {
    threadPool_.push([&]() {
        encodeSession_.Flush();
    });
}

} // namespace lightdb