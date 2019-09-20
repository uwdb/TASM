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

void CudaDecoder::mapFrame(CUVIDPARSERDISPINFO *frame, CUVIDEOFORMAT format) {
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
    picIndexToMappedFrameInfo_.emplace(std::pair<unsigned int, DecodedFrameInformation>(
            std::piecewise_construct,
            std::forward_as_tuple(frame->picture_index),
            std::forward_as_tuple(mappedHandle, pitch, format)));
}

void CudaDecoder::unmapFrame(unsigned int picIndex) const {
    std::scoped_lock lock(picIndexMutex_);
    assert(picIndexToMappedFrameInfo_.count(picIndex));
    CUresult result = cuvidUnmapVideoFrame(handle(), picIndexToMappedFrameInfo_.at(picIndex).handle);
    assert(result == CUDA_SUCCESS);

    picIndexToMappedFrameInfo_.erase(picIndex);
}

std::pair<CUdeviceptr, unsigned int> CudaDecoder::frameInfoForPicIndex(unsigned int picIndex) const {
    std::scoped_lock lock(picIndexMutex_);
    auto &decodedInfo = picIndexToMappedFrameInfo_.at(picIndex);
    return std::make_pair(decodedInfo.handle, decodedInfo.pitch);
}

CudaDecoder::DecodedDimensions CudaDecoder::decodedDimensionsForPicIndex(unsigned int picIndex) const {
    std::scoped_lock lock(picIndexMutex_);
    auto &format = picIndexToMappedFrameInfo_.at(picIndex).format;

    return {static_cast<unsigned int>(format.display_area.right - format.display_area.left),
            static_cast<unsigned int>(format.display_area.bottom - format.display_area.top),
            format.coded_width,
            format.coded_height};
}
