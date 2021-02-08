#ifndef TASM_SCANTILEDVIDEOOPERATOR_H
#define TASM_SCANTILEDVIDEOOPERATOR_H

#include "Operator.h"

#include "EncodedData.h"
#include "Rectangle.h"
#include "SemanticDataManager.h"
#include "TileLocationProvider.h"
#include "StitchContext.h"

namespace tasm {

class ScanTiledVideoOperator : public Operator<CPUEncodedFrameDataPtr> {
public:
    ScanTiledVideoOperator(
            std::shared_ptr<TiledEntry> entry,
            std::shared_ptr<SemanticDataManager> semanticDataManager,
            std::shared_ptr<TileLocationProvider> tileLocationProvider,
            bool shouldReadEntireGOPs = false)
            : isComplete_(false), entry_(entry), semanticDataManager_(semanticDataManager),
            tileLocationProvider_(tileLocationProvider),
            shouldReadEntireGOPs_(shouldReadEntireGOPs),
            totalVideoWidth_(0), totalVideoHeight_(0),
            totalNumberOfPixels_(0), totalNumberOfFrames_(0),
            totalNumberOfBytes_(0), numberOfTilesRead_(0),
            didSignalEOS_(false),
            currentTileNumber_(0), currentTileArea_(0)
    {
        preprocess();
    }

    bool isComplete() override { return isComplete_; }
    std::optional<CPUEncodedFrameDataPtr> next() override;

private:
    void preprocess();
    void setUpNextEncodedFrameReader();
    std::shared_ptr<std::vector<int>> nextGroupOfFramesWithTheSameLayoutAndFromTheSameFile(std::vector<int>::const_iterator &frameIt, std::vector<int>::const_iterator &endIt);
    std::unique_ptr<std::unordered_map<unsigned int, std::shared_ptr<std::vector<int>>>> filterToTileFramesThatContainObject(std::shared_ptr<std::vector<int>> possibleFrames);

    bool isComplete_;
    std::shared_ptr<TiledEntry> entry_;
    std::shared_ptr<SemanticDataManager> semanticDataManager_;
    std::shared_ptr<TileLocationProvider> tileLocationProvider_;
    bool shouldReadEntireGOPs_;

    unsigned int totalVideoWidth_;
    unsigned int totalVideoHeight_;

    unsigned long long int totalNumberOfPixels_;
    unsigned long long int totalNumberOfFrames_;
    unsigned long long int totalNumberOfBytes_;
    unsigned int numberOfTilesRead_;
    bool didSignalEOS_;

    std::shared_ptr<const TileLayout> currentTileLayout_;
    std::unique_ptr<std::experimental::filesystem::path> currentTilePath_;
    unsigned int currentTileNumber_;
    std::unique_ptr<EncodedFrameReader> currentEncodedFrameReader_;
    std::unordered_map<std::string, Configuration> tilePathToConfiguration_;

    struct TileInformation {
        std::experimental::filesystem::path filename;
        int tileNumber;
        unsigned int width;
        unsigned int height;
        std::shared_ptr<std::vector<int>> framesToRead;
        unsigned int frameOffsetInFile;
        Rectangle tileRect;

        // Considers only dimensions for the purposes of ordering reads.
        bool operator<(const TileInformation &other) {
            if (height < other.height)
                return true;
            else if (height > other.height)
                return false;
            else if (width < other.width)
                return true;
            else
                return false;
        }
    };

    std::vector<TileInformation> orderedTileInformation_;
    std::vector<TileInformation>::const_iterator orderedTileInformationIt_;
    unsigned int currentTileArea_;
};

class ScanFullFramesFromTiledVideoOperator : public Operator<CPUEncodedFrameDataPtr> {
public:
    ScanFullFramesFromTiledVideoOperator(
            std::shared_ptr<TiledEntry> entry,
            std::shared_ptr<SemanticDataManager> semanticDataManager,
            std::shared_ptr<TileLocationProvider> tileLocationProvider)
                : isComplete_(false),
                entry_(entry),
                semanticDataManager_(semanticDataManager),
                tileLocationProvider_(tileLocationProvider),
                didSignalEOS_(false),
                frameIt_(semanticDataManager_->orderedFrames().begin()),
                endFrameIt_(semanticDataManager_->orderedFrames().end()),
                ppsId_(1),
                  fullFrameConfig_(fullFrameConfig())
    { }

    bool isComplete() override { return isComplete_; }
    std::optional<CPUEncodedFrameDataPtr> next() override;

    const Configuration &configuration() { return *fullFrameConfig_; }
private:
    void setUpNextEncodedFrameReaders();
    GOPReaderPacket stitchedDataForNextGOP();
    std::experimental::filesystem::path pathForFrame(int frame) {
        return tileLocationProvider_->locationOfTileForFrame(0, frame).parent_path();
    }
    std::unique_ptr<Configuration> fullFrameConfig();

    bool isComplete_;
    std::shared_ptr<TiledEntry> entry_;
    std::shared_ptr<SemanticDataManager> semanticDataManager_;
    std::shared_ptr<TileLocationProvider> tileLocationProvider_;
    bool didSignalEOS_;
    std::vector<int>::const_iterator frameIt_;
    std::vector<int>::const_iterator endFrameIt_;

    std::vector<std::unique_ptr<EncodedFrameReader>> currentEncodedFrameReaders_;
    std::unique_ptr<stitching::StitchContext> currentContext_;
    unsigned int ppsId_;

    std::unique_ptr<Configuration> fullFrameConfig_;
};

} // namespace tasm

#endif //TASM_SCANTILEDVIDEOOPERATOR_H
