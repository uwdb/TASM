#ifndef TASM_ENCODEDDATA_H
#define TASM_ENCODEDDATA_H

#include "Configuration.h"
#include "DecodeReader.h"
#include "Frame.h"

namespace tasm {
class CPUEncodedFrameData {
public:
    CPUEncodedFrameData(Configuration configuration,
                        const DecodeReaderPacket &packet)
            : configuration_(configuration),
              packet_(packet),
              firstFrameIndex_(-1),
              numberOfFrames_(-1),
              tileNumber_(-1) {}

    const DecodeReaderPacket &packet() const { return packet_; }

    void setFirstFrameIndexAndNumberOfFrames(int firstFrameIndex, int numberOfFrames) {
        // Should only set this once.
        assert(firstFrameIndex_ == -1);
        assert(numberOfFrames_ == -1);
        firstFrameIndex_ = firstFrameIndex;
        numberOfFrames_ = numberOfFrames;
    }

    void setTileNumber(int tileNumber) {
        assert(tileNumber_ == -1);
        tileNumber_ = tileNumber;
    }

    bool getFirstFrameIndexIfSet(int &outFirstFrameIndex) const {
        if (firstFrameIndex_ == -1)
            return false;

        outFirstFrameIndex = firstFrameIndex_;
        return true;
    }

    bool getNumberOfFramesIfSet(int &outNumberOfFrames) const {
        if (numberOfFrames_ == -1)
            return false;

        outNumberOfFrames = numberOfFrames_;
        return true;
    }

    bool getTileNumberIfSet(int &outTileNumber) const {
        if (tileNumber_ == -1)
            return false;

        outTileNumber = tileNumber_;
        return true;
    }

    const Configuration &configuration() const { return configuration_; }

private:
    const Configuration configuration_;
    const DecodeReaderPacket packet_;
    int firstFrameIndex_;
    int numberOfFrames_;
    int tileNumber_;
};

using CPUEncodedFrameDataPtr = std::shared_ptr<CPUEncodedFrameData>;

using GPUFramePtr = std::shared_ptr<DecodedFrame>;
class GPUDecodedFrameData {
public:
    GPUDecodedFrameData(const Configuration &configuration, std::unique_ptr<std::vector<GPUFramePtr>> frames)
        : configuration_(configuration),
        frames_(std::move(frames))
    {
        assert(frames_);
    }

    const Configuration &configuration() const { return configuration_; }
    std::vector<GPUFramePtr> &frames() { return *frames_; }

private:
    Configuration configuration_;
    std::unique_ptr<std::vector<GPUFramePtr>> frames_;
};

} // namespace tasm

#endif //TASM_ENCODEDDATA_H
