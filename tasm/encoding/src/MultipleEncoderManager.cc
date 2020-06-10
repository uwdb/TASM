#include "MultipleEncoderManager.h"

namespace tasm {

void TileEncoder::updateConfiguration(unsigned int newWidth, unsigned int newHeight) {
    NvEncPictureCommand picCommand;
    picCommand.bResolutionChangePending = true;
    picCommand.newWidth = newWidth;
    picCommand.newHeight = newHeight;

    auto result = encoder_.api().NvEncReconfigureEncoder(&picCommand);
    assert(result == NV_ENC_SUCCESS);
}

std::unique_ptr<std::vector<char>> TileEncoder::getEncodedFrames() {
    unsigned int numberOfFrames;
    return writer_.dequeueToPtr(numberOfFrames);
}

void TileEncoder::encodeFrame(Frame &frame, unsigned int top, unsigned int left, bool isKeyframe) {
    encodeSession_.Encode(frame, top, left, isKeyframe);
}

void TileEncoder::flush() {
    encodeSession_.Flush();
}

} // namespace tasm