#ifndef LIGHTDB_VIDEODECODER_H
#define LIGHTDB_VIDEODECODER_H

#include "Configuration.h"
#include "FrameQueue.h"
#include "VideoLock.h"
#include "errors.h"
#include "spsc_queue.h"
#include "cuviddec.h"
#include "nvcuvid.h"

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
            frameNumberQueue_(frameNumberQueue),
            currentBitrate_(0)
  {
      CUresult result;
      creationInfo_ = this->configuration().AsCuvidCreateInfo(lock);
      currentFormat_ = FormatFromCreateInfo(creationInfo_);

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

  bool reconfigureDecoderIfNecessary(CUVIDEOFORMAT *newFormat) {
      assert(newFormat->coded_width <= creationInfo_.ulMaxWidth);
      assert(newFormat->coded_height <= creationInfo_.ulMaxHeight);

      // For now, only support reconfiguring dimensions.
      if (!currentBitrate_)
          currentBitrate_ = newFormat->bitrate;
      else
          assert(currentBitrate_ == newFormat->bitrate);

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

//      START_TIMER
      lock().lock();
      CUresult result;
      if ((result = cuvidReconfigureDecoder(handle_, &reconfigParams)) != CUDA_SUCCESS)
          throw GpuCudaRuntimeError("Failed to reconfigure decoder", result);
      lock().unlock();
//      STOP_TIMER("*** Time to reconfigure decoder: ")

      return true;
  }

  CUvideodecoder handle() const { return handle_; }
  VideoLock &lock() const {return lock_; }
  lightdb::spsc_queue<int> &frameNumberQueue() const { return frameNumberQueue_; }

  CUVIDEOFORMAT currentFormat() const { return currentFormat_; }
  void mapFrame(CUVIDPARSERDISPINFO *frame, CUVIDEOFORMAT format);
  void unmapFrame(unsigned int picIndex) const;
  std::pair<CUdeviceptr, unsigned int> frameInfoForPicIndex(unsigned int picIndex) const;

  struct DecodedDimensions {
      unsigned int displayWidth;
      unsigned int displayHeight;
      unsigned int codedWidth;
      unsigned int codedHeight;
  };
  DecodedDimensions decodedDimensionsForPicIndex(unsigned int picIndex) const;

protected:
  CUvideodecoder handle_;
  VideoLock &lock_;
  lightdb::spsc_queue<int> &frameNumberQueue_;
  CUVIDDECODECREATEINFO creationInfo_;

  CUVIDEOFORMAT currentFormat_;
  unsigned int currentBitrate_;

  // pic Index -> handle + pitch.
  mutable std::mutex picIndexMutex_;
  struct DecodedFrameInformation {
      DecodedFrameInformation(CUdeviceptr handle, unsigned int pitch, CUVIDEOFORMAT format)
        : handle(handle),
        pitch(pitch),
        format(format)
      { }

      CUdeviceptr handle;
      unsigned int pitch;
      CUVIDEOFORMAT format;
  };
  mutable std::unordered_map<unsigned int, DecodedFrameInformation> picIndexToMappedFrameInfo_;


private:
    static CUVIDEOFORMAT FormatFromCreateInfo(CUVIDDECODECREATEINFO);

    static bool DECODER_DESTROYED;
};

#endif // LIGHTDB_VIDEODECODER_H
