#ifndef TASM_SCANOPERATORS_H
#define TASM_SCANOPERATORS_H

#include "Operator.h"

#include "DecodeReader.h"
#include "EncodedData.h"
#include "Video.h"

namespace tasm {

class ScanFileDecodeReader : public ConfigurationOperator<CPUEncodedFrameDataPtr> {
public:
    ScanFileDecodeReader(std::shared_ptr<Video> video)
        : video_(video),
        reader_(video->path()),
        isComplete_(false)
    {}

    std::optional<CPUEncodedFrameDataPtr> next() override {
        auto packet = reader_.read();
        if (!packet.has_value()) {
            isComplete_ = true;
            return std::nullopt;
        }

        return {std::make_shared<CPUEncodedFrameData>(video_->configuration(), packet.value())};
    }

    bool isComplete() override { return isComplete_; }

    const Configuration &configuration() override { return video_->configuration(); }

private:
    std::shared_ptr<Video> video_;
    FileDecodeReader reader_;
    bool isComplete_;
};

class ScanFramesFromFileDecodeReader : public ConfigurationOperator<CPUEncodedFrameDataPtr> {
public:
    ScanFramesFromFileDecodeReader(std::shared_ptr<Video> video, std::shared_ptr<std::vector<int>> framesToRead, bool shouldReadEntireGOPS=false)
        : video_(video), framesToRead_(framesToRead), shouldReadEntireGOPs_(shouldReadEntireGOPS),
        frameReader_(video_->path(), *framesToRead_, 0, shouldReadEntireGOPs_),
        isComplete_(false), numberOfFramesRead_(0) {}

    std::optional<CPUEncodedFrameDataPtr> next() override {
        if (frameReader_.isEos()) {
            std::cout << "Number of frames read: " << numberOfFramesRead_ << std::endl;
            isComplete_ = true;
            return {};
        }

        auto gopPacket = frameReader_.read();
        assert(gopPacket.has_value());
        unsigned long flags = CUVID_PKT_DISCONTINUITY;
        if (frameReader_.isEos())
            flags |= CUVID_PKT_ENDOFSTREAM;

        auto data =std::make_shared<CPUEncodedFrameData>(video_->configuration(), DecodeReaderPacket(*gopPacket->data(), flags));
        data->setFirstFrameIndexAndNumberOfFrames(gopPacket->firstFrameIndex(), gopPacket->numberOfFrames());
        numberOfFramesRead_ += gopPacket->numberOfFrames();
        return {data};
    }

    bool isComplete() override { return isComplete_; }

    const Configuration &configuration() override { return video_->configuration(); }

private:
    std::shared_ptr<Video> video_;
    std::shared_ptr<std::vector<int>> framesToRead_;
    bool shouldReadEntireGOPs_;
    EncodedFrameReader frameReader_;
    bool isComplete_;
    unsigned int numberOfFramesRead_;
};

} // namespace tasm


#endif //TASM_SCANOPERATORS_H
