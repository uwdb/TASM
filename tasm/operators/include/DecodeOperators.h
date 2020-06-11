#ifndef TASM_DECODEOPERATORS_H
#define TASM_DECODEOPERATORS_H

#include "Operator.h"

#include "EncodedData.h"
#include "VideoDecoder.h"
#include "VideoDecoderSession.h"

namespace tasm {

class GPUDecodeFromCPU : public ConfigurationOperator<GPUDecodedFrameData> {
public:
    GPUDecodeFromCPU(std::shared_ptr<Operator<CPUEncodedFrameDataPtr>> scan,
            const Configuration &configuration,
            std::shared_ptr<GPUContext> context,
            std::shared_ptr<VideoLock> lock,
            unsigned int largestWidth = 0,
            unsigned int largestHeight = 0)
        : isComplete_(false),
        configuration_(configuration),
        frameNumberQueue_(std::make_shared<spsc_queue<int>>(50000)),
        tileNumberQueue_(std::make_shared<spsc_queue<int>>(50000)),
        context_(context),
        lock_(lock),
        largestWidth_(largestWidth ?: configuration_.codedWidth),
        largestHeight_(largestHeight ?: configuration_.codedHeight),
        decoder_(configuration_, lock_, frameNumberQueue_, tileNumberQueue_),
        session_(decoder_, scan),
          numberOfFramesDecoded_(0)
    {
        decoder_.preallocateArraysForDecodedFrames(largestWidth_, largestHeight_);
    }

    const Configuration &configuration() override { return configuration_; }

    bool isComplete() override { return isComplete_; }

    std::optional<GPUDecodedFrameData> next() override {
        if (isComplete_)
            return {};

        auto frames = std::make_unique<std::vector<GPUFramePtr>>();

        if (!session_.isComplete() || decoder_.decodedPictureQueue().read_available()) {
            do {
                auto frame = session_.decode(std::chrono::milliseconds(1u));
                if (frame)
                    frames->emplace_back(frame);
            } while (decoder_.decodedPictureQueue().read_available() &&
                     //                         !decoder_.frame_queue().isEndOfDecode() &&
                     frames->size() <= decoder_.createInfo().ulNumOutputSurfaces / 4);
        }

        if (!frames->empty() || !session_.isComplete()) {
            numberOfFramesDecoded_ += frames->size();
            return {GPUDecodedFrameData(configuration_, std::move(frames))};
        } else {
            std::cout << "Num-frames-from-decoder: " << numberOfFramesDecoded_ << std::endl;
            isComplete_ = true;
            return std::nullopt;
        }
    }

private:
    bool isComplete_;

    const Configuration configuration_;
    std::shared_ptr<spsc_queue<int>> frameNumberQueue_;
    std::shared_ptr<spsc_queue<int>> tileNumberQueue_;

    std::shared_ptr<GPUContext> context_;
    std::shared_ptr<VideoLock> lock_;
    unsigned int largestWidth_;
    unsigned int largestHeight_;

    VideoDecoder decoder_;
    VideoDecoderSession session_;
    int numberOfFramesDecoded_;
};

} // namespace tasm


#endif //TASM_DECODEOPERATORS_H
