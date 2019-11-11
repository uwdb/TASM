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
#include "timer.h"

#include "nvToolsExtCuda.h"
#include <sstream>

#define START_TIMER auto start = std::chrono::high_resolution_clock::now();
#define STOP_TIMER(print_message) std::cout << std::endl << print_message << \
    std::chrono::duration_cast<std::chrono::milliseconds>( \
    std::chrono::high_resolution_clock::now() - start).count() \
    << " ms " << std::endl;

static const unsigned int NUMBER_OF_PREALLOCATED_FRAMES = 150;

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
  CudaDecoder(const DecodeConfiguration &configuration, FrameQueue& frame_queue, VideoLock& lock, lightdb::spsc_queue<int>& frameNumberQueue, lightdb::spsc_queue<int>& tileNumberQueue, bool isDecodingDifferentSizes)
          : VideoDecoder(configuration, frame_queue), handle_(nullptr),
            lock_(lock),
            frameNumberQueue_(frameNumberQueue),
            tileNumberQueue_(tileNumberQueue),
            decodedPictureQueue_(100),
            picId_(0),
            isDecodingDifferentSizes_(isDecodingDifferentSizes),
            currentBitrate_(0),
            numberOfReconfigures_(0),
            maxNumberOfAllocatedFrames_(0),
            availableFrameArrays_(NUMBER_OF_PREALLOCATED_FRAMES),
            pitchOfPreallocatedFrameArrays_(0),
            heightOfPreallocatedFrameArrays_(0)
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
            frameNumberQueue_(other.frameNumberQueue_),
            tileNumberQueue_(other.tileNumberQueue_),
            decodedPictureQueue_(100),
            picId_(other.picId_),
            isDecodingDifferentSizes_(other.isDecodingDifferentSizes_),
            numberOfReconfigures_(other.numberOfReconfigures_),
            preallocatedFrameArrays_(other.preallocatedFrameArrays_),
            availableFrameArrays_(NUMBER_OF_PREALLOCATED_FRAMES) {
      other.handle_ = nullptr;
  }

  virtual ~CudaDecoder() {
      std::cout << "ANALYSIS number-of-reconfigures " << numberOfReconfigures_ << std::endl;
      std::cout << "ANALYSIS maxNumberOfAllocatedFrames " << maxNumberOfAllocatedFrames_ << std::endl;

      // Try emptying the decoded picture queue before the picIndexToMappedFrameInfo_ gets cleared, or the decoder
      // gets destroyed.
      decodedPictureQueue_.reset();

      // Deallocate all preallocated frames.
      for (const auto &handle : preallocatedFrameArrays_) {
          CUresult result = cuMemFree(handle);
          assert(result == CUDA_SUCCESS);
      }

      // I tried calling the destructor at the last call, but it segfaulted.
      // I think it has to be called the first time.
      if(handle() != nullptr) {
          cuvidDestroyDecoder(handle());
          CudaDecoder::DECODER_DESTROYED = true;
      }
  }

  void preallocateArraysForDecodedFrames(unsigned int largestWidth, unsigned int largestHeight) {
      // TODO: should translate into coded width/height.
      // Bump up width/height to be a multiple of 32, which is what I think is required for coded width/height.
      auto codedDim = 32;
      if (largestWidth % codedDim) {
          largestWidth = (largestWidth / codedDim + 1) * codedDim;
      }
      if (largestHeight % codedDim) {
          largestHeight = (largestHeight / codedDim + 1) * codedDim;
      }

      // Should be part of init, but oh well.
      preallocatedFrameArrays_.resize(NUMBER_OF_PREALLOCATED_FRAMES);
      heightOfPreallocatedFrameArrays_ = largestHeight * 3 / 2;

      for (int i = 0; i < NUMBER_OF_PREALLOCATED_FRAMES; ++i) {
          CUdeviceptr handle;
          size_t pitch;
          CUresult result = cuMemAllocPitch(&handle, &pitch, largestWidth, heightOfPreallocatedFrameArrays_, 16);
          if (pitchOfPreallocatedFrameArrays_)
              assert(pitch == pitchOfPreallocatedFrameArrays_);
          else
              pitchOfPreallocatedFrameArrays_ = pitch;

          preallocatedFrameArrays_[i] = handle;
          availableFrameArrays_.push(handle);
      }

  }

  bool isDecodingDifferentSizes() const { return isDecodingDifferentSizes_; }

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

      ++numberOfReconfigures_;

//      auto oldCodedWidth = currentFormat_.coded_width;
      auto oldDisplayWidth = currentFormat_.display_area.right - currentFormat_.display_area.left;
//      auto oldCodedHeight = currentFormat_.coded_height;
      auto oldDisplayHeight = currentFormat_.display_area.bottom - currentFormat_.display_area.top;


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
      lightdb::RECONFIGURE_DECODER_TIMER.startSection("reconfigureDecoder");
      nvtxNameOsThread(std::hash<std::thread::id>()(std::this_thread::get_id()), "DECODE");

      // Include old width/new width old, height/new height in message.
      unsigned int newDisplayWidth = newFormat->display_area.right - newFormat->display_area.left;
      unsigned int newDisplayHeight = newFormat->display_area.bottom - newFormat->display_area.top;
      std::stringstream markMessage;
      markMessage << "ReconfigureDecoder width: " << oldDisplayWidth << "->" << newDisplayWidth << ", equals-coded: " << (newDisplayWidth == newFormat->coded_width)
                    << ", height: " << oldDisplayHeight << "->" << newDisplayHeight << ", equals-coded: " << (newDisplayHeight == newFormat->coded_height);
      nvtxMark(markMessage.str().c_str());

      lock().lock();
      CUresult result;
      if ((result = cuvidReconfigureDecoder(handle_, &reconfigParams)) != CUDA_SUCCESS)
          throw GpuCudaRuntimeError("Failed to reconfigure decoder", result);
      lock().unlock();
      lightdb::RECONFIGURE_DECODER_TIMER.endSection("reconfigureDecoder");
//      STOP_TIMER("*** Time to reconfigure decoder: ")

      return true;
  }

  CUvideodecoder handle() const { return handle_; }
  VideoLock &lock() const {return lock_; }
  lightdb::spsc_queue<int> &frameNumberQueue() const { return frameNumberQueue_; }
  lightdb::spsc_queue<int> &tileNumberQueue() const { return tileNumberQueue_; }
  lightdb::spsc_queue<std::shared_ptr<CUVIDPARSERDISPINFO>> &decodedPictureQueue() { return decodedPictureQueue_; }

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
  lightdb::spsc_queue<int> &tileNumberQueue_;
  lightdb::spsc_queue<std::shared_ptr<CUVIDPARSERDISPINFO>> decodedPictureQueue_; // FIXME: should be prt for copy constructor.
  CUVIDDECODECREATEINFO creationInfo_;
  int picId_;
  bool isDecodingDifferentSizes_;
  mutable unsigned long maxNumberOfAllocatedFrames_;

  std::vector<CUdeviceptr> preallocatedFrameArrays_;
  mutable lightdb::spsc_queue<CUdeviceptr> availableFrameArrays_; // FIXME: should be ptr for copy constructor.
  size_t pitchOfPreallocatedFrameArrays_;
  size_t heightOfPreallocatedFrameArrays_;

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
    unsigned long int numberOfReconfigures_;

    static CUVIDEOFORMAT FormatFromCreateInfo(CUVIDDECODECREATEINFO);
    static bool DECODER_DESTROYED;
};

#endif // LIGHTDB_VIDEODECODER_H
