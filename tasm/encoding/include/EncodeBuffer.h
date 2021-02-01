#ifndef TASM_ENCODEBUFFER_H
#define TASM_ENCODEBUFFER_H

#include "Configuration.h"
#include "Frame.h"
#include "VideoEncoder.h"

struct EncodeInputBuffer
{
    unsigned int      width;
    unsigned int      height;
#if defined (NV_WINDOWS)
    IDirect3DSurface9 *NV12Surface;
#endif
    CUdeviceptr       NV12devPtr;
    uint32_t          NV12Stride;
    CUdeviceptr       NV12TempdevPtr;
    uint32_t          NV12TempStride;
    void*             registered_resource;
    NV_ENC_INPUT_PTR  input_surface;
    NV_ENC_BUFFER_FORMAT buffer_format;
};

struct EncodeOutputBuffer
{
    unsigned int          bitstreamBufferSize;
    NV_ENC_OUTPUT_PTR     bitstreamBuffer;
    void                  *outputEvent;
    bool                  waitOnEvent;
    bool                  EOSFlag;
};

struct EncodeBuffer
{
    EncodeOutputBuffer      output_buffer;
    EncodeInputBuffer       input_buffer;
    VideoEncoder&           encoder;
    const size_t            size;

    //TODO why is this deleted?
    EncodeBuffer(const EncodeBuffer&& other) = delete;

    EncodeBuffer(const EncodeBuffer& other)
            : EncodeBuffer(other.encoder, other.size)
    { }

    // TODO is this size reasonable?
    explicit EncodeBuffer(VideoEncoder &encoder, size_t size=4*1024*1024)
            : output_buffer{},
              input_buffer{},
              encoder(encoder), size(size) {
        NVENCSTATUS status;
        CUresult result;

        if(encoder.configuration().codedHeight % 2 != 0)
            throw std::runtime_error("Buffer height must be even");
        else if(encoder.configuration().codedWidth % 2 != 0)
            throw std::runtime_error("Buffer width must be even");
        else if(!encoder.api().encoderCreated())
            throw std::runtime_error("Encoder not created in API");
        else if((result = cuMemAllocPitch(&input_buffer.NV12devPtr,
                                          (size_t*)&input_buffer.NV12Stride,
                                          encoder.configuration().codedWidth,
                                          encoder.configuration().codedHeight * 3 / 2,
                                          16)) != CUDA_SUCCESS)
            throw std::runtime_error("Call to cuMemAllocPitch failed: " + std::to_string(result));
        else if((status = encoder.api().NvEncRegisterResource(
                NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR, (void *)input_buffer.NV12devPtr,
                encoder.configuration().codedWidth, encoder.configuration().codedHeight,
                input_buffer.NV12Stride, &input_buffer.registered_resource)) != NV_ENC_SUCCESS)
            throw std::runtime_error("Call to api.NvEncRegisterResource failed: " + std::to_string(status));
        else if((status = encoder.api().NvEncCreateBitstreamBuffer(
                size, &output_buffer.bitstreamBuffer)) != NV_ENC_SUCCESS)
            throw std::runtime_error("Call to api.NvEncCreateBitstreamBuffer failed: " + std::to_string(status));

        input_buffer.buffer_format = NV_ENC_BUFFER_FORMAT_NV12_PL;
        input_buffer.width = encoder.configuration().codedWidth;
        input_buffer.height = encoder.configuration().codedHeight;
        output_buffer.bitstreamBufferSize = size;
        output_buffer.outputEvent = nullptr;
    }

    ~EncodeBuffer() {
        NVENCSTATUS status;
        CUresult result;

//        FrameWriter::writeUpdate("free", input_buffer.NV12devPtr);

        if((status = encoder.api().NvEncDestroyBitstreamBuffer(output_buffer.bitstreamBuffer)) != NV_ENC_SUCCESS)
            std::cerr << "Call to NvEncDestroyBitstreamBuffer failed with status " << status << std::endl;
        else if((status = encoder.api().NvEncUnregisterResource(input_buffer.registered_resource)) != NV_ENC_SUCCESS)
            std::cerr << "Call to NvEncUnregisterResource failed with status " << status << std::endl;
        else if((result = cuMemFree(input_buffer.NV12devPtr)) != CUDA_SUCCESS)
            std::cerr << "Call to cuMemFree failed with result " << result << std::endl;
    }

    //TODO These arguments should be GPUFrame
    void copy(VideoLock &lock, Frame &frame) {
        if(frame.width() != input_buffer.width ||
           frame.height() != input_buffer.height) {
            throw std::runtime_error("Frame size does not match buffer size");
        }

        //TODO create new overloads that accept CudaDecodedFrames and avoid unsafe cast
        auto cudaFrame = dynamic_cast<GPUFrame&>(frame).cuda();

        //CudaDecodedFrame cudaFrame(frame);
        copy(lock, {
                .srcXInBytes = 0,
                .srcY = 0,
                .srcMemoryType = CU_MEMORYTYPE_DEVICE,
                .srcHost = nullptr,
                .srcDevice = cudaFrame->handle(),
                .srcArray = nullptr,
                .srcPitch = cudaFrame->pitch(),

                .dstXInBytes = 0,
                .dstY = 0,

                .dstMemoryType = CU_MEMORYTYPE_DEVICE,
                .dstHost = nullptr,
                .dstDevice = static_cast<CUdeviceptr>(input_buffer.NV12devPtr),
                .dstArray = nullptr,
                .dstPitch = input_buffer.NV12Stride,

                .WidthInBytes = input_buffer.width,
                .Height = input_buffer.height * 3 / 2
        });
    }

    void copy(VideoLock &lock, Frame &frame,
              size_t frame_top, size_t frame_left,
              size_t buffer_top=0, size_t buffer_left=0) {
        auto cudaFrame = dynamic_cast<GPUFrame&>(frame).cuda();
        //CudaDecodedFrame cudaFrame(frame);
        CUDA_MEMCPY2D lumaPlaneParameters{
                srcXInBytes:   frame_left,
                srcY:          frame_top,
                srcMemoryType: CU_MEMORYTYPE_DEVICE,
                srcHost:       nullptr,
                srcDevice:     cudaFrame->handle(),
                srcArray:      nullptr,
                srcPitch:      cudaFrame->pitch(),

                dstXInBytes:   buffer_left,
                dstY:          buffer_top,
                dstMemoryType: CU_MEMORYTYPE_DEVICE,
                dstHost:       nullptr,
                dstDevice:     static_cast<CUdeviceptr>(input_buffer.NV12devPtr),
                dstArray:      nullptr,
                dstPitch:      input_buffer.NV12Stride,

                WidthInBytes:  std::min(input_buffer.width - buffer_left, cudaFrame->width() - frame_left),
                Height:        std::min(input_buffer.height - buffer_top, cudaFrame->height() - frame_top) ,
        };

        CUDA_MEMCPY2D chromaPlaneParameters{
                srcXInBytes:   frame_left,
                srcY:          frame.height() + frame_top / 2,
                srcMemoryType: CU_MEMORYTYPE_DEVICE,
                srcHost:       nullptr,
                srcDevice:     cudaFrame->handle(),
                srcArray:      nullptr,
                srcPitch:      cudaFrame->pitch(),

                dstXInBytes:   buffer_left,
                dstY:          input_buffer.height + buffer_top / 2,
                dstMemoryType: CU_MEMORYTYPE_DEVICE,
                dstHost:       nullptr,
                dstDevice:     static_cast<CUdeviceptr>(input_buffer.NV12devPtr),
                dstArray:      nullptr,
                dstPitch:      input_buffer.NV12Stride,

                WidthInBytes:  std::min(input_buffer.width - buffer_left, cudaFrame->width() - frame_left),
                Height:        std::min(input_buffer.height - buffer_top, cudaFrame->height() - frame_top) / 2
        };

        copy(lock, {lumaPlaneParameters, chromaPlaneParameters});
    }

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

    void lock() {
        NVENCSTATUS status;
        if((status = encoder.api().NvEncMapInputResource(input_buffer.registered_resource,
                                                         &input_buffer.input_surface)) != NV_ENC_SUCCESS)
            throw std::runtime_error("Call to api.NvEncMapInputResource failed: " + std::to_string(status));
    }

    void unlock() {
        NVENCSTATUS status;
        if((status = encoder.api().NvEncUnmapInputResource(input_buffer.input_surface)) != NV_ENC_SUCCESS)
            throw std::runtime_error("Call to api.NvEncUnmapInputResource failed: " + std::to_string(status));
    }
};

struct MotionEstimationBuffer
{
    EncodeOutputBuffer      output_buffer;
    EncodeInputBuffer       input_buffer[2];
    unsigned int            inputFrameIndex;
    unsigned int            referenceFrameIndex;
};

#endif //TASM_ENCODEBUFFER_H
