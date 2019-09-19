#include "VideoDecoder.h"

#include <cassert>

bool CudaDecoder::DECODER_DESTROYED = false;

void CudaDecoder::mapFrame(CUVIDPARSERDISPINFO *frame) {
    CUresult result;
    CUdeviceptr mappedHandle;
    unsigned int pitch;
    CUVIDPROCPARAMS mapParameters;
    memset(&mapParameters, 0, sizeof(CUVIDPROCPARAMS));
    mapParameters.progressive_frame = frame->progressive_frame;
    mapParameters.top_field_first = frame->top_field_first;
    mapParameters.unpaired_field = frame->progressive_frame == 1 || frame->repeat_first_field <= 1;

    if((result = cuvidMapVideoFrame(handle(), frame->picture_index,
                                    &mappedHandle, &pitch, &mapParameters)) != CUDA_SUCCESS)
        throw GpuCudaRuntimeError("Call to cuvidMapVideoFrame failed", result);

    std::scoped_lock lock(picIndexMutex_);
    assert(!picIndexToMappedFrameInfo_.count(frame->picture_index));
    picIndexToMappedFrameInfo_.emplace(frame->picture_index, std::make_pair(mappedHandle, pitch));
}

void CudaDecoder::unmapFrame(unsigned int picIndex) const {
    std::scoped_lock lock(picIndexMutex_);
    assert(picIndexToMappedFrameInfo_.count(picIndex));
    CUresult result = cuvidUnmapVideoFrame(handle(), picIndexToMappedFrameInfo_.at(picIndex).first);
    assert(result == CUDA_SUCCESS);

    picIndexToMappedFrameInfo_.erase(picIndex);
}

std::pair<CUdeviceptr, unsigned int> &CudaDecoder::frameInfoForPicIndex(unsigned int picIndex) const {
    std::scoped_lock lock(picIndexMutex_);
    return picIndexToMappedFrameInfo_.at(picIndex);
}
