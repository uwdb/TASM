#ifndef TASM_GPUCONTEXT_H
#define TASM_GPUCONTEXT_H

#include <cuda.h>
#include "nvcuvid.h"

#include <cassert>
#include <iostream>
#include <stdexcept>

class GPUContext {
public:
    explicit GPUContext(const unsigned int deviceId): device_(0) {
        CUresult result;

        if(device_count() == 0)
            throw std::runtime_error("No CUDA devices were found");
        else if((result = cuCtxGetCurrent(&context_)) != CUDA_SUCCESS)
            throw std::runtime_error("Call to cuCtxGetCurrent failed: " + std::to_string(result));
        else if((result = cuDeviceGet(&device_, deviceId)) != CUDA_SUCCESS)
            throw std::runtime_error(std::string("Call to cuDeviceGet failed for device ") + std::to_string(deviceId) + ": " + std::to_string(result));
        else if((result = cuCtxCreate(&context_, CU_CTX_SCHED_AUTO, device_)) != CUDA_SUCCESS)
            throw std::runtime_error(std::string("Call to cuCtxCreate failed for device ") + std::to_string(deviceId) + ": " + std::to_string(result));
    }

    GPUContext(const GPUContext& other) = delete;
    GPUContext(GPUContext&& other) = delete;

    ~GPUContext() {
        CUresult result;

        if(context_ != nullptr && (result = cuCtxDestroy(context_)) != CUDA_SUCCESS)
            std::cerr << "Swallowed failure to destroy CUDA context (CUresult " << result << ") in destructor" << std::endl;
    }

    CUdevice device() const noexcept { return device_; }
    CUcontext get() const { return context_; }

    void AttachToThread() const {
        CUresult result;

        if((result = cuCtxSetCurrent(context_)) != CUDA_SUCCESS) {
            throw std::runtime_error("Call to cuCtxSetCurrent failed: " + std::to_string(result));
        }
    }

    static size_t device_count() {
        CUresult result;
        int count;

        try {
            if(!Initialize())
                throw std::runtime_error("GPU context initialization failed");
            if((result = cuDeviceGetCount(&count)) != CUDA_SUCCESS)
                throw std::runtime_error("Call to cuDeviceGetCount failed: " + std::to_string(result));
            else
                assert(count >= 0);
        } catch(const std::exception&) {
            std::cerr << "GPU context initialization failed; assuming no GPUs on host" << std::endl;
            return 0;
        }

        return static_cast<size_t>(count);
    }

private:
    static bool Initialize() {
        CUresult result;

        if(isInitialized)
            return true;
        else if((result = cuInit(0)) != CUDA_SUCCESS)
            throw std::runtime_error("Call to cuInit failed" + std::to_string(result));
        else
            return (isInitialized = true);
    }

    static bool isInitialized;

    CUdevice device_;
    CUcontext context_ = nullptr;
};

#endif //TASM_GPUCONTEXT_H
