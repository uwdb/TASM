#ifndef LIGHTDB_VIDEODECODER_H
#define LIGHTDB_VIDEODECODER_H

#include "Configuration.h"
#include "FrameQueue.h"
#include "VideoLock.h"
#include "errors.h"
#include "spsc_queue.h"
#include "cuviddec.h"

#include <chrono>
#include <iostream>
#include <mutex>

#define START_TIMER auto start = std::chrono::high_resolution_clock::now();
#define STOP_TIMER(print_message) std::cout << std::endl << print_message << \
    std::chrono::duration_cast<std::chrono::milliseconds>( \
    std::chrono::high_resolution_clock::now() - start).count() \
    << " ms " << std::endl;

class VideoDecoder {
public:
    const DecodeConfiguration& configuration() const { return configuration_; }
    FrameQueue &frame_queue() const {return frame_queue_; }

protected:
    //TODO push frame_queue into session
    VideoDecoder(DecodeConfiguration configuration, FrameQueue& frame_queue)
        : configuration_(std::move(configuration)), frame_queue_(frame_queue)
    { }

    DecodeConfiguration configuration_;

private:
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
      creationInfo_ = this->configuration().AsCuvidCreateInfo(lock);

      if((result = cuvidCreateDecoder(&handle_, &creationInfo_)) != CUDA_SUCCESS) {
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
      // I tried calling the destructor at the last call, but it segfaulted.
      // I think it has to be called the first time.
      if(handle() != nullptr) {
          cuvidDestroyDecoder(handle());
          CudaDecoder::DECODER_DESTROYED = true;
      }
  }

  void reconfigureDecoder(CUVIDEOFORMAT *newFormat) {
      if (newFormat->coded_width > creationInfo_.ulMaxWidth)
          assert(false);
      if (newFormat->coded_height > creationInfo_.ulMaxHeight)
          assert(false);

      CUVIDRECONFIGUREDECODERINFO reconfigParams;
      memset(&reconfigParams, 0, sizeof(CUVIDRECONFIGUREDECODERINFO));
      reconfigParams.ulWidth = newFormat->coded_width;
      reconfigParams.ulHeight = newFormat->coded_height;

      // Assume top and left offsets are 0.
      reconfigParams.display_area.top = 0;
      reconfigParams.display_area.left = 0;
      reconfigParams.display_area.right = newFormat->coded_width;
      reconfigParams.display_area.bottom = newFormat->coded_height;

      reconfigParams.ulTargetWidth = newFormat->coded_width;
      reconfigParams.ulTargetHeight = newFormat->coded_height;

      reconfigParams.ulNumDecodeSurfaces = creationInfo_.ulNumDecodeSurfaces;

      START_TIMER
      lock().lock();
      CUresult result;
      if ((result = cuvidReconfigureDecoder(handle_, &reconfigParams)) != CUDA_SUCCESS)
          throw GpuCudaRuntimeError("Failed to reconfigure decoder", result);
      lock().unlock();
      STOP_TIMER("*** Time to reconfigure decoder: ")
  }

  void updateConfiguration(DecodeConfiguration &newConfiguration) {
      // Will this mess up if pictures are currently being decoded?
      CUresult result;
      // As a hack for now, just update the configuration's width and height.
      configuration_.width = newConfiguration.width;
      configuration_.height = newConfiguration.height;
      auto decoderCreateInfo = this->configuration().AsCuvidCreateInfo(lock_);

      if ((result = cuvidDestroyDecoder(handle_)) != CUDA_SUCCESS) {
          throw GpuCudaRuntimeError("Call to cuvidDestroyDecoder failed", result);
      }

      handle_ = nullptr;
      if ((result = cuvidCreateDecoder(&handle_, &decoderCreateInfo)) != CUDA_SUCCESS) {
          throw GpuCudaRuntimeError("Call to cuvidCreateDecoder failed", result);
      } else {
          LOG(INFO) << "made decoder";
      }
  }

  CUvideodecoder handle() const { return handle_; }
  VideoLock &lock() const {return lock_; }
  lightdb::spsc_queue<int> &frameNumberQueue() const { return frameNumberQueue_; }

  void mapFrame(CUVIDPARSERDISPINFO *frame);
  void unmapFrame(unsigned int picIndex) const;
  std::pair<CUdeviceptr , unsigned int> &frameInfoForPicIndex(unsigned int picIndex) const;

protected:
  CUvideodecoder handle_;
  VideoLock &lock_;
  lightdb::spsc_queue<int> &frameNumberQueue_;
  CUVIDDECODECREATEINFO creationInfo_;

  // pic Index -> handle + pitch.
  mutable std::mutex picIndexMutex_;
  mutable std::unordered_map<unsigned int, std::pair<CUdeviceptr, unsigned int>> picIndexToMappedFrameInfo_;


private:
    static bool DECODER_DESTROYED;
};

#endif // LIGHTDB_VIDEODECODER_H
