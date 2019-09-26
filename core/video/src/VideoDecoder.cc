#include "VideoDecoder.h"

#include "Frame.h"
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
//    CUresult result;
//    CUdeviceptr mappedHandle;
//    unsigned int pitch;
//    CUVIDPROCPARAMS mapParameters;
//    memset(&mapParameters, 0, sizeof(CUVIDPROCPARAMS));
//    mapParameters.progressive_frame = frame->progressive_frame;
//    mapParameters.top_field_first = frame->top_field_first;
//    mapParameters.unpaired_field = frame->progressive_frame == 1 || frame->repeat_first_field <= 1;
    {
        std::scoped_lock lock(lock_);
        std::scoped_lock lock2(picIndexMutex_);

//        if ((result = cuvidMapVideoFrame(handle(), frame->picture_index,
//                                         &mappedHandle, &pitch, &mapParameters)) != CUDA_SUCCESS)
//            throw GpuCudaRuntimeError("Call to cuvidMapVideoFrame failed", result);

//        FrameWriter::writeUpdate("map", mappedHandle);

/*
        CUdeviceptr newHandle;
        size_t newPitch;
        auto width = format.coded_width; //  format.display_area.right - format.display_area.left;
        auto height = format.coded_height; // format.display_area.bottom - format.display_area.top;
        result = cuMemAllocPitch(&newHandle, &newPitch, width, height * 3 / 2, 16);
        assert(result == CUDA_SUCCESS);
//        FrameWriter::writeUpdate("alloc", newHandle);

        CUDA_MEMCPY2D m;
        memset(&m, 0, sizeof(m));
        m.srcMemoryType = CU_MEMORYTYPE_DEVICE;
        m.srcDevice = mappedHandle;
        m.srcPitch = pitch;
        m.dstMemoryType = CU_MEMORYTYPE_DEVICE;
        m.dstDevice = newHandle;
        m.dstPitch = newPitch;
        m.WidthInBytes = width;
        m.Height = height * 3 / 2;
        result = cuMemcpy2D(&m);
        assert(result == CUDA_SUCCESS);

//        FrameWriter::writeUpdate("unmap", mappedHandle);
        if ((result = cuvidUnmapVideoFrame(handle(), mappedHandle)) != CUDA_SUCCESS)
            throw GpuCudaRuntimeError("Call to unmap failed", result);

 */
//        assert(!picIndexToMappedFrameInfo_.count(frame->picture_index));
        picIndexToMappedFrameInfo_.emplace(std::pair<unsigned int, DecodedFrameInformation>(
                std::piecewise_construct,
                std::forward_as_tuple(frame->picture_index),
                std::forward_as_tuple(0, 0, format)));
    }
}

void CudaDecoder::unmapFrame(unsigned int picIndex) const {
    CUdeviceptr frameHandle;
    {
        std::scoped_lock lock1(lock_);
        std::scoped_lock lock(picIndexMutex_);
        assert(picIndexToMappedFrameInfo_.count(picIndex));
        frameHandle = picIndexToMappedFrameInfo_.at(picIndex).handle;

        picIndexToMappedFrameInfo_.erase(picIndex);

//        FrameWriter::writeUpdate("free", frameHandle);
//        CUresult result = cuMemFree(frameHandle);
        CUresult result = cuvidUnmapVideoFrame(handle(), frameHandle);
        assert(result == CUDA_SUCCESS);
    }
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
