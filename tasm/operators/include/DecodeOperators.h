#ifndef TASM_DECODEOPERATORS_H
#define TASM_DECODEOPERATORS_H

#include "Operator.h"

#include "EncodedData.h"
#include "VideoDecoder.h"
#include "VideoDecoderSession.h"

namespace tasm {

class GPUDecodeFromCPU : public Operator<GPUDecodedFrameData> {
public:
    GPUDecodeFromCPU(std::shared_ptr<Operator<CPUEncodedFrameDataPtr>> scan,
            const Configuration &configuration,
            std::shared_ptr<GPUContext> context,
            std::shared_ptr<VideoLock> lock)
        : isComplete_(false),
        configuration_(configuration),
        frameNumberQueue_(std::make_shared<spsc_queue<int>>(50000)),
        tileNumberQueue_(std::make_shared<spsc_queue<int>>(50000)),
        context_(context),
        lock_(lock),
        decoder_(configuration_, lock_, frameNumberQueue_, tileNumberQueue_),
        session_(decoder_, scan),
          numberOfFramesDecoded_(0)
    {
        // TODO: Use largest width/height from tiles.
        decoder_.preallocateArraysForDecodedFrames(configuration_.codedWidth, configuration_.codedHeight);
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

    VideoDecoder decoder_;
    VideoDecoderSession session_;
    int numberOfFramesDecoded_;

};

} // namespace tasm


#endif //TASM_DECODEOPERATORS_H
