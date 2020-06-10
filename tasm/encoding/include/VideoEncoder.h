#ifndef TASM_VIDEOENCODER_H
#define TASM_VIDEOENCODER_H

#include "Configuration.h"
#include "EncodeAPI.h"
#include "VideoLock.h"
#include <memory>

class EncodeBuffer;

class VideoEncoder {
public:
    VideoEncoder(GPUContext &context, const EncodeConfiguration &configuration, VideoLock &lock)
            : configuration_(configuration), context_(context), api_(std::make_shared<EncodeAPI>(context)), lock_(lock),
              encoderHandle_(VideoEncoderHandle(context, *api_, configuration)),
              buffers_(CreateBuffers(minimumBufferCount())) {
        if (api().ValidatePresetGUID(configuration) != NV_ENC_SUCCESS)
            throw std::runtime_error("Invalid preset guid");
    }

    VideoEncoder(VideoEncoder &) = delete;

    VideoEncoder(VideoEncoder &&other) = delete;

    EncodeAPI &api() { return *api_; }

    const EncodeConfiguration &configuration() const { return configuration_; }

    std::vector<std::shared_ptr<EncodeBuffer>> &buffers() { return buffers_; }
    VideoLock &lock() const { return lock_; }

protected:
    const EncodeConfiguration &configuration_;
    GPUContext &context_;
    std::shared_ptr<EncodeAPI> api_;
    VideoLock &lock_;

private:
    class VideoEncoderHandle {
    public:
        VideoEncoderHandle(GPUContext &context, EncodeAPI &api, const EncodeConfiguration &configuration)
                : context_(context), api_(api), configuration_(configuration) {
            NVENCSTATUS status;

            if ((status = api_.CreateEncoder(&configuration_)) != NV_ENC_SUCCESS)
                throw std::runtime_error("Call to api.CreateEncoder failed: " + std::to_string(status));
        }

        VideoEncoderHandle(VideoEncoderHandle &) = delete;

        VideoEncoderHandle(VideoEncoderHandle &&other) noexcept
                : context_(other.context_), api_(other.api_), configuration_(other.configuration_) {
            other.moved_ = true;
        }

        ~VideoEncoderHandle() {
            NVENCSTATUS result;
            if (!moved_ && (result = api_.NvEncDestroyEncoder()) != NV_ENC_SUCCESS)
                std::cerr << "Swallowed failure to destroy encoder (NVENCSTATUS " << result << ") in destructor"
                          << std::endl;
        }

    private:
        GPUContext &context_;
        EncodeAPI &api_;
        const EncodeConfiguration &configuration_;
        bool moved_ = false;
    } encoderHandle_;

    std::vector<std::shared_ptr<EncodeBuffer>> buffers_;

    size_t minimumBufferCount() const { return configuration().numB + 4; }

private:
    std::vector<std::shared_ptr<EncodeBuffer>> CreateBuffers(size_t size);
};

#endif //TASM_VIDEOENCODER_H
