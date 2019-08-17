#ifndef LIGHTDB_VIDEODECODER_H
#define LIGHTDB_VIDEODECODER_H

#include "Configuration.h"
#include "FrameQueue.h"
#include "VideoLock.h"
#include "errors.h"
#include "spsc_queue.h"

class VideoDecoder {
public:
    const DecodeConfiguration& configuration() const { return configuration_; }
    FrameQueue &frame_queue() const {return frame_queue_; }

protected:
    //TODO push frame_queue into session
    VideoDecoder(DecodeConfiguration configuration, FrameQueue& frame_queue)
        : configuration_(std::move(configuration)), frame_queue_(frame_queue)
    { }

private:
    const DecodeConfiguration configuration_;
    FrameQueue& frame_queue_;
};


class CudaDecoder: public VideoDecoder {
public:
  CudaDecoder(const DecodeConfiguration &configuration, FrameQueue& frame_queue, VideoLock& lock, lightdb::spsc_queue<int>& frameNumberQueue)
          : VideoDecoder(configuration, frame_queue), handle_(nullptr),
            lock_(lock),
            frameNumberQueue_(frameNumberQueue)
  {
      CUresult result;
      auto decoderCreateInfo = this->configuration().AsCuvidCreateInfo(lock);

      if((result = cuvidCreateDecoder(&handle_, &decoderCreateInfo)) != CUDA_SUCCESS) {
          throw GpuCudaRuntimeError("Call to cuvidCreateDecoder failed", result);
      }
  }

  CudaDecoder(const CudaDecoder&) = delete;
  CudaDecoder(CudaDecoder&& other) noexcept
          : VideoDecoder(std::move(other)),
            handle_(other.handle_),
            lock_(other.lock_),
            frameNumberQueue_(other.frameNumberQueue_) {
      other.handle_ = nullptr;
  }

  virtual ~CudaDecoder() {
      // FIXME: This chokes when doing:
      // input = Load(file)
      // input2 = Load(file)
      // input.Map(yolo).Union(input2).Save()
      if(handle() != nullptr)
          cuvidDestroyDecoder(handle());
  }

  CUvideodecoder handle() const { return handle_; }
  VideoLock &lock() const {return lock_; }
  lightdb::spsc_queue<int> &frameNumberQueue() const { return frameNumberQueue_; }

protected:
  CUvideodecoder handle_;
  VideoLock &lock_;
  lightdb::spsc_queue<int> &frameNumberQueue_;
};

#endif // LIGHTDB_VIDEODECODER_H
