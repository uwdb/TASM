#ifndef LIGHTDB_SELECTPIXELSKERNEL_H
#define LIGHTDB_SELECTPIXELSKERNEL_H

#include "CudaKernel.h"
#include "Frame.h"

namespace lightdb::video {

class GPUSelectPixels {
public:
    class NV12 : public CudaKernel {
    public:
        NV12(const GPUContext &context, const std::experimental::filesystem::path &module_path)
            : CudaKernel(context, module_path, module_filename, function_name)
        { }

        CudaFrameReference select(VideoLock &lock, const CudaFrameReference &input,
                                const std::vector<Rectangle> &boxes, unsigned int xOffset, unsigned int yOffset) const {
            auto output = GPUFrameReference::make<CudaFrame>(static_cast<Frame&>(*input));

            if (!boxes.size())
                return output;

            auto &cuda = output.downcast<CudaFrame>();

            cuda.copy(lock, *input);
            select(lock,
                    cuda.handle(), cuda.height(), cuda.width(), cuda.pitch(),
                    boxes.data(), boxes.size(), xOffset, yOffset);

            return output;
        }

        CudaFrameReference draw(VideoLock &lock, const CudaFrameReference &input,
                const std::vector<Rectangle> &boxes, unsigned int xOffset, unsigned int yOffset) const {
            auto output = GPUFrameReference::make<CudaFrame>(static_cast<Frame&>(*input));

            if (!boxes.size())
                return output;

            auto &cuda = output.downcast<CudaFrame>();

            cuda.copy(lock, *input);
            select(lock,
                   cuda.handle(), cuda.codedHeight(), cuda.codedWidth(), cuda.pitch(),
                   boxes.data(), boxes.size(), xOffset, yOffset);

            return output;
        }

        void select(VideoLock &lock, CUdeviceptr frame,
                unsigned int height, unsigned int width, unsigned int pitch,
                const Rectangle *boxes, const size_t box_count, unsigned int xOffset, unsigned int yOffset) const {
            CUresult result;
            CUdeviceptr device_boxes;

            if((result = cuMemAlloc(&device_boxes, sizeof(Rectangle) * box_count)) != CUDA_SUCCESS)
                throw GpuCudaRuntimeError("cuMemAlloc failure", result);
            else if((result = cuMemcpyHtoD(device_boxes, boxes, sizeof(Rectangle) * box_count)) != CUDA_SUCCESS)
                throw GpuCudaRuntimeError("cuMemCpy failure", result);

            try {
                select(lock, frame, height, width, pitch, device_boxes, static_cast<unsigned int>(box_count), xOffset, yOffset);
            } catch(errors::_GpuCudaRuntimeError&) {
                if ((result == cuMemFree(device_boxes)) != CUDA_SUCCESS)
                    LOG(ERROR) << "Swallowed failed cuMemFree invocation with result " << result;
                throw;
            }

            if ((result = cuMemFree(device_boxes)) != CUDA_SUCCESS)
                throw GpuCudaRuntimeError("cuMemFree failure", result);

        }

        void select(VideoLock &lock, CUdeviceptr frame,
                unsigned int height, unsigned int width, unsigned int pitch,
                CUdeviceptr boxes, unsigned int box_count, unsigned int xOffset, unsigned int yOffset) const {
            dim3 block(32u, 32u, 1u);
            dim3 grid(width / block.x + 1, height / block.y + 1, 1u);

            void *args[] = {
                    &frame,
                    &height, &width,
                    &pitch,
                    &boxes, &box_count,
                    &xOffset, &yOffset };

            invoke(lock, block, grid, args);
        }

    private:
        constexpr static const char* function_name = "select_pixels";
        constexpr static const char* module_filename = "select_pixels.ptx";
    };

    explicit GPUSelectPixels(const GPUContext &context)
        : GPUSelectPixels(context, {"modules"})
    { }

    GPUSelectPixels(const GPUContext &context, const std::experimental::filesystem::path &module_path)
        : nv12_(context, module_path)
    { }

    const NV12 &nv12() const { return nv12_; }

private:
    NV12 nv12_;
};

static std::weak_ptr<GPUSelectPixels> SELECTPIXELSKERNEL = {};
static std::shared_ptr<GPUSelectPixels> GetSelectPixelsKernel(const GPUContext &context) {
    if (SELECTPIXELSKERNEL.use_count())
        return SELECTPIXELSKERNEL.lock();

    auto shared = std::make_shared<GPUSelectPixels>(context);
    SELECTPIXELSKERNEL = std::weak_ptr<GPUSelectPixels>(shared);
    return shared;
}
} // namespace lightdb::video

#endif //LIGHTDB_SELECTPIXELSKERNEL_H
