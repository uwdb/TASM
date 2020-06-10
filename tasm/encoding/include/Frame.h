#ifndef TASM_FRAME_H
#define TASM_FRAME_H

#include "nvEncodeAPI.h"
#include "VideoDecoder.h"
#include "VideoLock.h"
#include <iostream>

class Frame {
public:
    Frame(unsigned int height, unsigned int width, NV_ENC_PIC_STRUCT type, int frameNumber)
            : height_(height), width_(width), type_(type), frameNumber_(frameNumber)
    { }

    Frame(unsigned int height, unsigned int width, NV_ENC_PIC_STRUCT type)
            : height_(height), width_(width), type_(type)
    { }

    Frame(const Configuration &configuration, NV_ENC_PIC_STRUCT type)
            : Frame(configuration.displayHeight, configuration.displayWidth, type)
    { }

    Frame(const Configuration &configuration, NV_ENC_PIC_STRUCT type, int frameNumber)
            : Frame(configuration.displayHeight, configuration.displayWidth, type, frameNumber)
    { }

    Frame(const Frame&) = default;
    Frame(Frame &&) noexcept = default;

    virtual ~Frame() = default;

    virtual unsigned int height() const { return height_; }
    virtual unsigned int width() const { return width_; }
    virtual unsigned int codedHeight() const { return 0; }
    virtual unsigned int codedWidth() const { return 0; }

    //TODO move this to GPUFrame
    NV_ENC_PIC_STRUCT type() const { return type_; }
    bool getFrameNumber(int &outFrameNumber) const {
        if (frameNumber_.has_value()) {
            outFrameNumber = *frameNumber_;
            return true;
        } else
            return false;

    }

protected:
    const unsigned int height_, width_;
    NV_ENC_PIC_STRUCT type_;
    std::optional<int> frameNumber_;
};

class CudaFrame;

class GPUFrame : public Frame {
public:
    using Frame::Frame;
    explicit GPUFrame(const Frame& frame) : Frame(frame) { }
    explicit GPUFrame(Frame && frame) noexcept : Frame(frame) { }

public:
    virtual std::shared_ptr<CudaFrame> cuda() = 0;
};

class CudaFrame: public GPUFrame {
public:
    explicit CudaFrame(const Frame &frame)
            : CudaFrame(frame, std::tuple_cat(allocate_frame(frame), std::make_tuple(true)))
    { }

    CudaFrame(const Frame &frame, const CUdeviceptr handle, const unsigned int pitch)
            : CudaFrame(frame, handle, pitch, false)
    { }

    CudaFrame(const unsigned int height, const unsigned int width, const NV_ENC_PIC_STRUCT type)
            : CudaFrame(Frame{height, width, type})
    { }

    CudaFrame(const unsigned int height, const unsigned int width,
              const NV_ENC_PIC_STRUCT type, const CUdeviceptr handle, const unsigned int pitch)
            : GPUFrame(height, width, type), handle_(handle), pitch_(pitch), owner_(false)
    { }

    CudaFrame(const CudaFrame&) = delete;
    CudaFrame(CudaFrame&& frame) noexcept
            : GPUFrame(std::move(frame)), handle_(frame.handle_), pitch_(frame.pitch_), owner_(frame.owner_)
    { frame.owner_ = false; }

    ~CudaFrame() override {
        CUresult result;

        if (owner_) {
            if (owner_ && (result = cuMemFree(handle_)) != CUDA_SUCCESS)
                std::cerr << "Swallowed failure to free CudaFrame resources (" << result << ")" << std::endl;
        }
    }

    virtual CUdeviceptr handle() const { return handle_; }
    virtual unsigned int pitch() const { return pitch_; }

    std::shared_ptr<CudaFrame> cuda() override {
        return std::make_shared<CudaFrame>(*this, handle_, pitch_);
    }

    void copy(VideoLock &lock, const CudaFrame &frame) {
        if(frame.width() != width() ||
           frame.height() != height()) {
            throw std::runtime_error("Frame sizes do not match");
        }

        copy(lock, {
                .srcXInBytes = 0,
                .srcY = 0,
                .srcMemoryType = CU_MEMORYTYPE_DEVICE,
                .srcHost = nullptr,
                .srcDevice = frame.handle(),
                .srcArray = nullptr,
                .srcPitch = frame.pitch(),

                .dstXInBytes = 0,
                .dstY = 0,

                .dstMemoryType = CU_MEMORYTYPE_DEVICE,
                .dstHost = nullptr,
                .dstDevice = handle(),
                .dstArray = nullptr,
                .dstPitch = pitch(),

                .WidthInBytes = width(),
                .Height = height() * 3 / 2 //TODO this assumes NV12 format
        });
    }

    void copy(VideoLock &lock, const CudaFrame &source,
              size_t source_top, size_t source_left,
              size_t destination_top=0, size_t destination_left=0,
              size_t coded_height=0) {
        auto actualHeight = coded_height ?: source.height();

        CUDA_MEMCPY2D lumaPlaneParameters = {
                .srcXInBytes   =   source_left,
                .srcY          = source_top,
                .srcMemoryType = CU_MEMORYTYPE_DEVICE,
                .srcHost       = nullptr,
                .srcDevice     = source.handle(),
                .srcArray      = nullptr,
                .srcPitch      = source.pitch(),

                .dstXInBytes   = destination_left,
                .dstY          = destination_top,
                .dstMemoryType = CU_MEMORYTYPE_DEVICE,
                .dstHost       = nullptr,
                .dstDevice     = handle(),
                .dstArray      = nullptr,
                .dstPitch      = pitch(),

                .WidthInBytes  = std::min(width() - destination_left, source.width() - source_left),
                // Only copy the display-height worth of luma/chroma values.
                .Height        = std::min(height() - destination_top, source.height() - source_top) ,
        };

        CUDA_MEMCPY2D chromaPlaneParameters = {
                .srcXInBytes   = source_left,
                // Use the coded height to find the start of the chroma plane.
                .srcY          = actualHeight + source_top / 2,
                .srcMemoryType = CU_MEMORYTYPE_DEVICE,
                .srcHost       = nullptr,
                .srcDevice     = source.handle(),
                .srcArray      = nullptr,
                .srcPitch      = source.pitch(),

                .dstXInBytes   = destination_left,
                .dstY          = height() + destination_top / 2,
                .dstMemoryType = CU_MEMORYTYPE_DEVICE,
                .dstHost       = nullptr,
                .dstDevice     = handle(),
                .dstArray      = nullptr,
                .dstPitch      = pitch(),

                .WidthInBytes  = std::min(width() - destination_left, source.width() - source_left),
                .Height        = std::min(height() - destination_top, source.height() - source_top) / 2
        };

        copy(lock, {lumaPlaneParameters, chromaPlaneParameters});
    }

protected:
    CudaFrame(const Frame &frame, const std::pair<CUdeviceptr, unsigned int> pair)
            : CudaFrame(frame, pair.first, pair.second, false)
    { }

    void copy(VideoLock &lock, const CUDA_MEMCPY2D &parameters) {
        CUresult result;
        std::scoped_lock l{lock};

        if ((result = cuMemcpy2D(&parameters)) != CUDA_SUCCESS) {
            throw std::runtime_error("Call to cuMemcpy2D failed: " + std::to_string(result));
        }
    }

    void copy(VideoLock &lock, const std::vector<CUDA_MEMCPY2D> &parameters) {
        std::scoped_lock l{lock};
        std::for_each(parameters.begin(), parameters.end(), [](const CUDA_MEMCPY2D &parameters) {
            CUresult result;
            if ((result = cuMemcpy2D(&parameters)) != CUDA_SUCCESS) {
                throw std::runtime_error("Call to cuMemcpy2D failed: " + std::to_string(result));
            }
        });
    }

private:
    CudaFrame(const Frame &frame, const std::tuple<CUdeviceptr, unsigned int, bool> tuple)
            : CudaFrame(frame, std::get<0>(tuple), std::get<1>(tuple), std::get<2>(tuple))
    { }

    CudaFrame(const Frame &frame, const CUdeviceptr handle, const unsigned int pitch, const bool owner)
            : GPUFrame(frame), handle_(handle), pitch_(pitch), owner_(owner)
    { }

    static std::pair<CUdeviceptr, unsigned int> allocate_frame(const Frame &frame)
    {
        CUresult result;
        CUdeviceptr handle;
        size_t pitch;

        if((result = cuMemAllocPitch(&handle,
                                     &pitch,
                                     frame.width(),
                                     frame.height() * 3 / 2,
                                     16)) != CUDA_SUCCESS)
            throw std::runtime_error("Call to cuMemAllocPitch failed: " + std::to_string(result));
        else {
            return std::make_pair(handle, static_cast<unsigned int>(pitch));
        }
    }

    const CUdeviceptr handle_;
    const unsigned int pitch_;
    bool owner_;
};

class DecodedFrame : public GPUFrame {
public:
    DecodedFrame(const VideoDecoder& decoder, const std::shared_ptr<CUVIDPARSERDISPINFO> &parameters, int frameNumber, int tileNumber)
            : GPUFrame(decoder.configuration(), extract_type(parameters), frameNumber), decoder_(decoder), parameters_(parameters),
              frameDimensions_(decoder_.decodedDimensionsForPicIndex(parameters_->picture_index)),
              tileNumber_(tileNumber)
    {
    }

    DecodedFrame(const VideoDecoder& decoder, const std::shared_ptr<CUVIDPARSERDISPINFO> &parameters, int frameNumber)
            : GPUFrame(decoder.configuration(), extract_type(parameters), frameNumber), decoder_(decoder), parameters_(parameters),
              frameDimensions_(decoder_.decodedDimensionsForPicIndex(parameters_->picture_index)),
              tileNumber_(-1)
    {
    }

    DecodedFrame(const VideoDecoder& decoder, const std::shared_ptr<CUVIDPARSERDISPINFO> &parameters)
            : GPUFrame(decoder.configuration(), extract_type(parameters)), decoder_(decoder), parameters_(parameters),
              frameDimensions_(decoder_.decodedDimensionsForPicIndex(parameters_->picture_index)),
              tileNumber_(-1)
    {
    }

    DecodedFrame(const DecodedFrame&) = default;
    DecodedFrame(DecodedFrame &&other) noexcept = default;

    std::shared_ptr<CudaFrame> cuda() override;

    const VideoDecoder &decoder() const { return decoder_; }
    const CUVIDPARSERDISPINFO& parameters() const { return *parameters_; }
    unsigned int height() const override { return frameDimensions_.displayHeight; }
    unsigned int width() const override { return frameDimensions_.displayWidth; }

    unsigned int codedHeight() const override { return frameDimensions_.codedHeight; }
    unsigned int codedWidth() const override { return frameDimensions_.codedWidth; }

    int tileNumber() const { return tileNumber_; }

private:
    static NV_ENC_PIC_STRUCT extract_type(const std::shared_ptr<CUVIDPARSERDISPINFO> &parameters) {
        return (parameters == nullptr || parameters->progressive_frame || parameters->repeat_first_field >= 2
                ? NV_ENC_PIC_STRUCT_FRAME
                : (parameters->top_field_first
                   ? NV_ENC_PIC_STRUCT_FIELD_TOP_BOTTOM
                   : NV_ENC_PIC_STRUCT_FIELD_BOTTOM_TOP));
    }

    const VideoDecoder &decoder_;
    const std::shared_ptr<CUVIDPARSERDISPINFO> parameters_;
    std::shared_ptr<CudaFrame> cuda_;
    VideoDecoder::DecodedDimensions frameDimensions_;
    int tileNumber_;
};

class CudaDecodedFrame: public DecodedFrame, public CudaFrame {
public:
    explicit CudaDecodedFrame(const DecodedFrame &frame)
            : DecodedFrame(frame), CudaFrame(frame, map_frame(frame))
    { }

    CudaDecodedFrame(CudaDecodedFrame &) noexcept = delete;
    CudaDecodedFrame(CudaDecodedFrame &&) noexcept = delete;

    unsigned int height() const override { return DecodedFrame::height(); }
    unsigned int width() const override { return DecodedFrame::width(); }
    unsigned int codedHeight() const override { return DecodedFrame::codedHeight(); }
    unsigned int codedWidth() const override { return DecodedFrame::codedWidth(); }

private:
    static std::pair<CUdeviceptr, unsigned int> map_frame(const DecodedFrame &frame) {
        return frame.decoder().frameInfoForPicIndex(frame.parameters().picture_index);
    }
};

#endif //TASM_FRAME_H
