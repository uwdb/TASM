#include "MultipleEncoderManager.h"

namespace lightdb {

void TileEncoder::updateConfiguration(unsigned int newWidth, unsigned int newHeight, unsigned int newQuantizationParameter) {
    if (newWidth == width_
                && newHeight == height_
                && newQuantizationParameter == quantizationParameter_)
        return;

    width_ = newWidth;
    height_ = newHeight;
    quantizationParameter_ = newQuantizationParameter;

    encoder_.reconfigureEncoder(newWidth, newHeight);
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