#ifndef TASM_VIDEODECODER_H
#define TASM_VIDEODECODER_H

#include "Configuration.h"
#include "GPUContext.h"
#include "VideoLock.h"

#include "cuviddec.h"
#include "nvcuvid.h"
#include <mutex>
#include <unordered_map>
#include <vector>
#include <boost/lockfree/spsc_queue.hpp>

template<typename T>
using spsc_queue = boost::lockfree::spsc_queue<T>;

static const unsigned int NUMBER_OF_PREALLOCATED_FRAMES = 150;

class VideoDecoder {
public:
    VideoDecoder(const Configuration &configuration,
            std::shared_ptr<VideoLock> lock,
            std::shared_ptr<spsc_queue<int>> frameNumberQueue,
            std::shared_ptr<spsc_queue<int>> tileNumberQueue)
            : lock_(lock),
                frameNumberQueue_(frameNumberQueue),
                tileNumberQueue_(tileNumberQueue),
                decodedPictureQueue_(100),
                picId_(0),
                availableFrameArrays_(NUMBER_OF_PREALLOCATED_FRAMES),
                pitchOfPreallocatedFrameArrays_(0),
                heightOfPreallocatedFrameArrays_(0) {
        CUresult result;
        creationInfo_ = CreateInfoFromConfiguration(configuration, lock->get());
        currentFormat_ = FormatFromCreateInfo(creationInfo_);

        if ((result = cuvidCreateDecoder(&handle_, &creationInfo_)) != CUDA_SUCCESS) {
            throw std::runtime_error("Call to cuvidCreateDecoder failes: " + std::to_string(result));
        }
    }

    ~VideoDecoder() {
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
        if(handle_ != nullptr) {
            cuvidDestroyDecoder(handle_);
        }
    }

    void preallocateArraysForDecodedFrames(unsigned int largestWidth, unsigned int largestHeight);
    bool reconfigureDecoderIfNecessary(CUVIDEOFORMAT *newFormat);

    void mapFrame(CUVIDPARSERDISPINFO *frame, CUVIDEOFORMAT format);
    void unmapFrame(unsigned int picIndex);

    std::shared_ptr<spsc_queue<int>> frameNumberQueue() const { return frameNumberQueue_; }
    std::shared_ptr<spsc_queue<int>> tileNumberQueue() const { return tileNumberQueue_; }
    CUVIDDECODECREATEINFO createInfo() const { return creationInfo_; }
    CUvideodecoder handle() const { return handle_; }
    CUVIDEOFORMAT currentFormat() const { return currentFormat_; }

private:
    static CUVIDDECODECREATEINFO CreateInfoFromConfiguration(const Configuration &configuration, CUvideoctxlock lock);
    static CUVIDEOFORMAT FormatFromCreateInfo(CUVIDDECODECREATEINFO createInfo);

    CUvideodecoder handle_;
    std::shared_ptr<VideoLock> lock_;
    std::shared_ptr<spsc_queue<int>> frameNumberQueue_;
    std::shared_ptr<spsc_queue<int>> tileNumberQueue_;
    spsc_queue<std::shared_ptr<CUVIDPARSERDISPINFO>> decodedPictureQueue_;

    CUVIDDECODECREATEINFO creationInfo_;
    int picId_;

    std::vector<CUdeviceptr> preallocatedFrameArrays_;
    spsc_queue<CUdeviceptr> availableFrameArrays_;
    size_t pitchOfPreallocatedFrameArrays_;
    size_t heightOfPreallocatedFrameArrays_;

    CUVIDEOFORMAT currentFormat_;

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
};

#endif //TASM_VIDEODECODER_H
