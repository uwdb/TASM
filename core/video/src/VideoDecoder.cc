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
    CUresult result;
    CUdeviceptr mappedHandle;
    unsigned int pitch;
    CUVIDPROCPARAMS mapParameters;
    memset(&mapParameters, 0, sizeof(CUVIDPROCPARAMS));
    mapParameters.progressive_frame = frame->progressive_frame;
    mapParameters.top_field_first = frame->top_field_first;
    mapParameters.unpaired_field = frame->progressive_frame == 1 || frame->repeat_first_field <= 1;
    {
        std::scoped_lock lock(lock_);
        if ((result = cuvidMapVideoFrame(handle(), frame->picture_index,
                                         &mappedHandle, &pitch, &mapParameters)) != CUDA_SUCCESS)
            throw GpuCudaRuntimeError("Call to cuvidMapVideoFrame failed", result);

        auto width = format.display_area.right - format.display_area.left; // format.coded_width;
        auto height = format.display_area.bottom - format.display_area.top; // format.coded_height;

        while (!availableFrameArrays_.read_available())
            std::this_thread::yield();

        CUdeviceptr newHandle = availableFrameArrays_.front();
        availableFrameArrays_.pop();

        CUDA_MEMCPY2D m;
        memset(&m, 0, sizeof(m));
        m.srcMemoryType = CU_MEMORYTYPE_DEVICE;
        m.srcDevice = mappedHandle;
        m.srcPitch = pitch;
        m.dstMemoryType = CU_MEMORYTYPE_DEVICE;
        m.dstDevice = newHandle;
        m.dstPitch = pitchOfPreallocatedFrameArrays_;
        m.WidthInBytes = width;
        m.Height = height * 3 / 2;
        result = cuMemcpy2D(&m);
        assert(result == CUDA_SUCCESS);

        if ((result = cuvidUnmapVideoFrame(handle(), mappedHandle)) != CUDA_SUCCESS)
            throw GpuCudaRuntimeError("Call to unmap failed", result);

        if (isDecodingDifferentSizes_)
            frame->picture_index = picId_++;

        std::shared_ptr<CUVIDPARSERDISPINFO> data(new CUVIDPARSERDISPINFO, [this](CUVIDPARSERDISPINFO *data) {
            this->unmapFrame(data->picture_index);
            delete data;
        });
        *data = *frame;

        {
            std::scoped_lock lock(picIndexMutex_);

            assert(!picIndexToMappedFrameInfo_.count(frame->picture_index));
            picIndexToMappedFrameInfo_.emplace(std::pair<unsigned int, DecodedFrameInformation>(
                    std::piecewise_construct,
                    std::forward_as_tuple(frame->picture_index),
                    std::forward_as_tuple(newHandle, pitchOfPreallocatedFrameArrays_, format)));

            decodedPictureQueue_.push(data);
        }
    }
}

// Why is this const?
void CudaDecoder::unmapFrame(unsigned int picIndex) const {
    CUdeviceptr frameHandle;
    {
        std::scoped_lock lock(picIndexMutex_);
        assert(picIndexToMappedFrameInfo_.count(picIndex));
        auto frameInfo = picIndexToMappedFrameInfo_.at(picIndex);
        frameHandle = frameInfo.handle;

        maxNumberOfAllocatedFrames_ = std::max(maxNumberOfAllocatedFrames_, picIndexToMappedFrameInfo_.size());

        // This is an unideal ordering to erase before unmapping, but I think it will be fine.
        picIndexToMappedFrameInfo_.erase(picIndex);
    }

    {
        std::scoped_lock lock(lock_); // Is it ok to not be holding this?
//             Should we memset the handle? Adds a lot of time, and shouldn't be necessary if we only look at the copied width/height.
//            CUresult result = cuMemsetD2D8(frameHandle, pitchOfPreallocatedFrameArrays_, 0, width, height);
//            assert(result == CUDA_SUCCESS);
        availableFrameArrays_.push(frameHandle);
//            CUresult result = cuMemFree(frameHandle);
//            assert(result == CUDA_SUCCESS);
    }
}

std::pair<CUdeviceptr, unsigned int> CudaDecoder::frameInfoForPicIndex(unsigned int picIndex) const {
    std::scoped_lock lock(picIndexMutex_);
    auto &decodedInfo = picIndexToMappedFrameInfo_.at(picIndex);
    return std::make_pair(decodedInfo.handle, decodedInfo.pitch);
}

CudaDecoder::DecodedDimensions CudaDecoder::decodedDimensionsForPicIndex(unsigned int picIndex) const {
    if (isDecodingDifferentSizes_) {
        std::scoped_lock lock(picIndexMutex_);
        auto &format = picIndexToMappedFrameInfo_.at(picIndex).format;

        return {static_cast<unsigned int>(format.display_area.right - format.display_area.left),
                static_cast<unsigned int>(format.display_area.bottom - format.display_area.top),
                format.coded_width,
                format.coded_height};
    } else {
        return { static_cast<unsigned int>(currentFormat_.display_area.right - currentFormat_.display_area.left),
                 static_cast<unsigned int>(currentFormat_.display_area.bottom - currentFormat_.display_area.top),
                 currentFormat_.coded_width,
                 currentFormat_.coded_height };
    }
}
