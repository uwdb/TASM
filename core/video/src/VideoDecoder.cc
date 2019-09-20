#include "VideoDecoder.h"

#include <cassert>
#include <nvcuvid.h>

bool CudaDecoder::DECODER_DESTROYED = false;

CUVIDEOFORMAT CudaDecoder::FormatFromCreateInfo(CUVIDDECODECREATEINFO createInfo) {
    CUVIDEOFORMAT format;
    memset(&format, 0, sizeof(format));
    format.codec = createInfo.CodecType;
    // create info = max number of surfases; format = min required for this frame.
    format.min_num_decode_surfaces = createInfo.ulNumDecodeSurfaces;
    format.coded_width = createInfo.ulWidth;
    format.coded_height = createInfo.ulHeight;
    // Format and create info use different types for display_area, so can't memcpy.
    format.display_area.top = createInfo.display_area.top;
    format.display_area.left = createInfo.display_area.left;
    format.display_area.right = createInfo.display_area.right;
    format.display_area.bottom = createInfo.display_area.bottom;

    format.chroma_format = createInfo.ChromaFormat;

    return format;
}

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
