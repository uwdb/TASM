#ifndef TASM_VIDEOLOCK_H
#define TASM_VIDEOLOCK_H

#include "GPUContext.h"

#include <cassert>
#include <iostream>
#include <memory>

class VideoLock
{
public:
    explicit VideoLock(std::shared_ptr<GPUContext> context) : context_(context) {
        CUresult result;

        if ((result = cuvidCtxLockCreate(&lock_, context->get())) != CUDA_SUCCESS) {
            throw std::runtime_error("Call to cuvidCtxLockCreate failed: " + std::to_string(result));
        }
    }
    ~VideoLock() {
        if(lock_ != nullptr)
            cuvidCtxLockDestroy(lock_);
    }

    VideoLock(const VideoLock&) = delete;
    VideoLock(VideoLock&& other) noexcept
            : context_(other.context_), lock_(other.lock_)
    { other.lock_ = nullptr; }

    const std::shared_ptr<GPUContext> context() const { return context_; }
    CUvideoctxlock get() const { return lock_; }

    void lock() {
        CUresult result;
        if ((result = cuvidCtxLock(get(), 0)) != CUDA_SUCCESS) {
            throw std::runtime_error("Call to cuvidCtxLock failed: " + std::to_string(result));
        }
    }

    void unlock() {
        CUresult result;
        if ((result = cuvidCtxUnlock(get(), 0)) != CUDA_SUCCESS) {
            throw std::runtime_error("Call to cuvidCtxUnlock failed: " + std::to_string(result));
        }
    }

private:
    std::shared_ptr<GPUContext> context_;
    CUvideoctxlock lock_ = nullptr;
};

#endif //TASM_VIDEOLOCK_H
