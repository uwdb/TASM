#include "Frame.h"

std::shared_ptr<CudaFrame> DecodedFrame::cuda() {
    return cuda_ ?: (cuda_ = std::make_shared<CudaDecodedFrame>(*this));
}
