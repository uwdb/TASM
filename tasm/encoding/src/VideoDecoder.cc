#include "VideoDecoder.h"
#include "cuviddec.h"
#include "Configuration.h"

#include <cstring>
#include <nvcuvid.h>
#include <thread>

cudaVideoCodec CudaCodecFromCodec(Codec codec) {
    switch (codec) {
        case Codec::H264:
            return cudaVideoCodec_H264;
        case Codec::HEVC:
            return cudaVideoCodec_HEVC;
        default:
            std::cerr << "Unrecognized codec" << std::endl;
            assert(false);
    }
}

unsigned int DefaultDecodeSurfaces(const Configuration &configuration) {
    unsigned int decode_surfaces;

    if (configuration.codec == Codec::H264) {
        // Assume worst-case of 20 decode surfaces for H264
        decode_surfaces = 20;
        //} else if (codec.cudaId() == cudaVideoCodec_VP9) {
        //    decode_surfaces = 12;
    } else if (configuration.codec == Codec::HEVC) {
        // ref HEVC spec: A.4.1 General tier and level limits
        auto maxLumaPS = 35651584u; // currently assuming level 6.2, 8Kx4K
        auto maxDpbPicBuf = 6u;
        auto picSizeInSamplesY = configuration.displayWidth * configuration.displayHeight;
        unsigned int MaxDpbSize;
        if (picSizeInSamplesY <= (maxLumaPS >> 2))
            MaxDpbSize = maxDpbPicBuf * 4;
        else if (picSizeInSamplesY <= (maxLumaPS >> 1))
            MaxDpbSize = maxDpbPicBuf * 2;
        else if (picSizeInSamplesY <= ((3 * maxLumaPS) >> 2))
            MaxDpbSize = (maxDpbPicBuf * 4) / 3;
        else
            MaxDpbSize = maxDpbPicBuf;
        MaxDpbSize = MaxDpbSize < 16 ? MaxDpbSize : 16;
        decode_surfaces = MaxDpbSize + 4;
    } else {
        decode_surfaces = 8;
    }

    return decode_surfaces;
}

CUVIDDECODECREATEINFO VideoDecoder::CreateInfoFromConfiguration(const Configuration &configuration, CUvideoctxlock lock) {
    CUVIDDECODECREATEINFO info;
    memset(&info, 0, sizeof(info));
    info.ulWidth = configuration.codedWidth;
    info.ulHeight = configuration.codedHeight;
    info.ulNumDecodeSurfaces = DefaultDecodeSurfaces(configuration);
    info.CodecType = CudaCodecFromCodec(configuration.codec);
    info.ChromaFormat = cudaVideoChromaFormat_420;
    info.OutputFormat = cudaVideoSurfaceFormat_NV12;
    info.ulCreationFlags = cudaVideoCreate_PreferCUVID;
    info.display_area = {
            .left = 0,
            .top = 0,
            .right = static_cast<short>(configuration.displayWidth),
            .bottom = static_cast<short>(configuration.displayHeight),
    };
    info.DeinterlaceMode = cudaVideoDeinterlaceMode_Weave;
    info.ulTargetWidth = configuration.displayWidth;
    info.ulTargetHeight = configuration.displayHeight;
    info.ulNumOutputSurfaces = 32;
    info.vidLock = lock;

    info.ulMaxWidth = configuration.maxWidth;
    info.ulMaxHeight = configuration.maxHeight;

    return info;
}

CUVIDEOFORMAT VideoDecoder::FormatFromCreateInfo(CUVIDDECODECREATEINFO createInfo) {
    CUVIDEOFORMAT format;
    memset(&format, 0, sizeof(format));

    format.codec = createInfo.CodecType;
    format.min_num_decode_surfaces = createInfo.ulNumDecodeSurfaces;
    format.coded_width = createInfo.ulWidth;
    format.coded_height = createInfo.ulHeight;
    format.display_area.top = createInfo.display_area.top;
    format.display_area.left = createInfo.display_area.left;
    format.display_area.right = createInfo.display_area.right;
    format.display_area.bottom = createInfo.display_area.bottom;
    format.chroma_format = createInfo.ChromaFormat;

    return format;
}

void VideoDecoder::preallocateArraysForDecodedFrames(unsigned int largestWidth, unsigned int largestHeight) {
    auto codedDim = 32;
    assert(!(largestWidth % codedDim));
    assert(!(largestHeight % codedDim));

    preallocatedFrameArrays_.resize(NUMBER_OF_PREALLOCATED_FRAMES);
    heightOfPreallocatedFrameArrays_ = largestHeight * 3 / 2;

    for (unsigned int i = 0u; i < NUMBER_OF_PREALLOCATED_FRAMES; ++i) {
        CUdeviceptr handle;
        size_t pitch;
        CUresult result = cuMemAllocPitch(&handle, &pitch, largestWidth, heightOfPreallocatedFrameArrays_, 16);
        assert(result == CUDA_SUCCESS);
        if (pitchOfPreallocatedFrameArrays_)
            assert(pitch == pitchOfPreallocatedFrameArrays_);
        else
            pitchOfPreallocatedFrameArrays_ = pitch;

        preallocatedFrameArrays_[i] = handle;
        availableFrameArrays_.push(handle);
    }
}

bool VideoDecoder::reconfigureDecoderIfNecessary(CUVIDEOFORMAT *newFormat) {
    assert(newFormat->coded_width <= creationInfo_.ulMaxWidth);
    assert(newFormat->coded_height <= creationInfo_.ulMaxHeight);

    if (currentFormat_.coded_width == newFormat->coded_width
        && currentFormat_.coded_height == newFormat->coded_height
        && currentFormat_.display_area.top == newFormat->display_area.top
        && currentFormat_.display_area.left == newFormat->display_area.left
        && currentFormat_.display_area.right == newFormat->display_area.right
        && currentFormat_.display_area.bottom == newFormat->display_area.bottom)
        return false;

    memcpy(&currentFormat_, newFormat, sizeof(currentFormat_));

    CUVIDRECONFIGUREDECODERINFO reconfigParams;
    memset(&reconfigParams, 0, sizeof(CUVIDRECONFIGUREDECODERINFO));
    reconfigParams.ulWidth = newFormat->coded_width;
    reconfigParams.ulHeight = newFormat->coded_height;

    reconfigParams.display_area.top = newFormat->display_area.top;
    reconfigParams.display_area.left = newFormat->display_area.left;
    reconfigParams.display_area.right = newFormat->display_area.right;
    reconfigParams.display_area.bottom = newFormat->display_area.bottom;

    reconfigParams.ulTargetWidth = newFormat->coded_width;
    reconfigParams.ulTargetHeight = newFormat->coded_height;

    reconfigParams.ulNumDecodeSurfaces = newFormat->min_num_decode_surfaces;

    lock_->lock();
    CUresult result;
    if ((result = cuvidReconfigureDecoder(handle_, &reconfigParams)) != CUDA_SUCCESS)
        throw std::runtime_error("Failed to reconfigure decoder" + std::to_string(result));
    lock_->unlock();

    return true;
}

void VideoDecoder::mapFrame(CUVIDPARSERDISPINFO *frame, CUVIDEOFORMAT format) {
    while (!decodedPictureQueue_.write_available()) {
        std::this_thread::yield();
    }

    CUresult result;
    CUdeviceptr mappedHandle;
    unsigned int pitch;
    CUVIDPROCPARAMS mapParameters;
    memset(&mapParameters, 0, sizeof(CUVIDPROCPARAMS));
    mapParameters.progressive_frame = frame->progressive_frame;
    mapParameters.top_field_first = frame->top_field_first;
    mapParameters.unpaired_field = frame->progressive_frame == 1 || frame->repeat_first_field <= 1;
    {
        std::scoped_lock lock(*lock_);
        if ((result = cuvidMapVideoFrame(handle_, frame->picture_index,
                                         &mappedHandle, &pitch, &mapParameters)) != CUDA_SUCCESS)
            throw std::runtime_error("Call to cuvidMapVideoFrame failed: " + std::to_string(result));

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

        if ((result = cuvidUnmapVideoFrame(handle_, mappedHandle)) != CUDA_SUCCESS)
            throw std::runtime_error("Call to unmap failed: " + std::to_string(result));

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

            assert(decodedPictureQueue_.push(data));
        }
    }
}

void VideoDecoder::unmapFrame(unsigned int picIndex) {
    CUdeviceptr frameHandle;
    {
        std::scoped_lock lock(picIndexMutex_);
        assert(picIndexToMappedFrameInfo_.count(picIndex));
        auto frameInfo = picIndexToMappedFrameInfo_.at(picIndex);
        frameHandle = frameInfo.handle;

        picIndexToMappedFrameInfo_.erase(picIndex);
    }

    {
        std::scoped_lock lock(*lock_); // Is it ok to not be holding this?
        availableFrameArrays_.push(frameHandle);
    }
}

std::pair<CUdeviceptr, unsigned int> VideoDecoder::frameInfoForPicIndex(unsigned int picIndex) const {
    std::scoped_lock lock(picIndexMutex_);
    auto &decodedInfo = picIndexToMappedFrameInfo_.at(picIndex);
    return std::make_pair(decodedInfo.handle, decodedInfo.pitch);
}

VideoDecoder::DecodedDimensions VideoDecoder::decodedDimensionsForPicIndex(unsigned int picIndex) const {
    std::scoped_lock lock(picIndexMutex_);
    auto &format = picIndexToMappedFrameInfo_.at(picIndex).format;

    return {static_cast<unsigned int>(format.display_area.right - format.display_area.left),
            static_cast<unsigned int>(format.display_area.bottom - format.display_area.top),
            format.coded_width,
            format.coded_height};
}
