#ifndef TASM_SCANOPERATORS_H
#define TASM_SCANOPERATORS_H

#include "Operator.h"

#include "DecodeReader.h"
#include "EncodedData.h"
#include "Video.h"

namespace tasm {

class ScanFileDecodeReader : public Operator<CPUEncodedFrameDataPtr> {
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

} // namespace tasm


#endif //TASM_SCANOPERATORS_H
