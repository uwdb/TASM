#include "ScanTiledVideoOperator.h"

#include "VideoConfiguration.h"

namespace tasm {

void ScanTiledVideoOperator::preprocess() {
    auto frameIt = semanticDataManager_->orderedFrames().cbegin();
    auto end = semanticDataManager_->orderedFrames().cend();
    while (frameIt != end) {
        auto possibleFramesToRead = nextGroupOfFramesWithTheSameLayoutAndFromTheSameFile(frameIt, end);
        auto tileToFrames = filterToTileFramesThatContainObject(possibleFramesToRead);

        for (auto tileNumberIt = tileToFrames->begin(); tileNumberIt != tileToFrames->end(); ++tileNumberIt) {
            if (tileNumberIt->second.empty())
                continue;
            auto rectangleForTile = currentTileLayout_->rectangleForTile(tileNumberIt->first);
            orderedTileInformation_.emplace_back<TileInformation>(
                    {TileFiles::tileFilename(currentTilePath_->parent_path(), tileNumberIt->first),
                     static_cast<int>(tileNumberIt->first),
                     rectangleForTile.width,
                     rectangleForTile.height,
                     tileNumberIt->second,
                     tileLocationProvider_->frameOffsetInTileFile(*currentTilePath_),
                     rectangleForTile});
        }
    }

    std::sort(orderedTileInformation_.begin(), orderedTileInformation_.end());
    orderedTileInformationIt_ = orderedTileInformation_.begin();
}

std::vector<int> ScanTiledVideoOperator::nextGroupOfFramesWithTheSameLayoutAndFromTheSameFile(std::vector<int>::const_iterator &frameIt, std::vector<int>::const_iterator &endIt) {
    assert(frameIt != endIt);

    auto fakeTileNumber = 0;
    // Get the configuration and location for the next frame.
    // While the path is the same, it must have the same configuration.
    currentTilePath_ = std::make_unique<std::experimental::filesystem::path>(tileLocationProvider_->locationOfTileForFrame(fakeTileNumber, *frameIt));
    currentTileLayout_ = std::make_unique<TileLayout>(tileLocationProvider_->tileLayoutForFrame(*frameIt));

    if (!totalVideoWidth_) {
        assert(!totalVideoHeight_);
        totalVideoWidth_ = currentTileLayout_->totalWidth();
        totalVideoHeight_ = currentTileLayout_->totalHeight();
    }

    std::vector<int> framesWithSamePathAndConfiguration({ *frameIt++ });
    while (frameIt != endIt) {
        if (tileLocationProvider_->locationOfTileForFrame(fakeTileNumber, *frameIt) == *currentTilePath_)
            framesWithSamePathAndConfiguration.push_back(*frameIt++);
        else
            break;
    }

    return framesWithSamePathAndConfiguration;
}

std::unique_ptr<std::unordered_map<unsigned int, std::vector<int>>> ScanTiledVideoOperator::filterToTileFramesThatContainObject(std::vector<int> &possibleFrames) {
    auto tileNumberToFrames = std::make_unique<std::unordered_map<unsigned int, std::vector<int>>>();

    // currentTileLayout is set in nextGroupOfFramesWithTheSameLayoutAndFromTheSameFile().
    if (currentTileLayout_->numberOfTiles() == 1) {
        (*tileNumberToFrames)[0] = possibleFrames;
        return tileNumberToFrames;
    }

    for (auto i = 0u; i < currentTileLayout_->numberOfTiles(); ++i) {
        auto tileRect = currentTileLayout_->rectangleForTile(i);
        (*tileNumberToFrames)[i].reserve(possibleFrames.size());
        for (auto frame = possibleFrames.begin(); frame != possibleFrames.end(); ++frame) {
            auto &rectanglesForFrame = semanticDataManager_->rectanglesForFrame(*frame);
            bool anyIntersect = std::any_of(rectanglesForFrame.begin(), rectanglesForFrame.end(), [&](auto &rectangle) {
                return tileRect.intersects(rectangle);
            });
            if (anyIntersect) {
                (*tileNumberToFrames)[i].push_back(*frame);
            }
        }
    }
    return tileNumberToFrames;
}

void ScanTiledVideoOperator::setUpNextEncodedFrameReader() {
    if (orderedTileInformationIt_ == orderedTileInformation_.end()) {
        currentEncodedFrameReader_ = nullptr;
    } else {
        ++numberOfTilesRead_;
        currentEncodedFrameReader_ = std::make_unique<EncodedFrameReader>(
                orderedTileInformationIt_->filename,
                orderedTileInformationIt_->framesToRead,
                orderedTileInformationIt_->frameOffsetInFile,
                shouldReadEntireGOPs_);

        currentTileArea_ = orderedTileInformationIt_->width * orderedTileInformationIt_->height;
        currentTilePath_ = std::make_unique<std::experimental::filesystem::path>(orderedTileInformationIt_->filename);
        currentTileNumber_ = orderedTileInformationIt_->tileNumber;

        ++orderedTileInformationIt_;
    }
}

std::optional<CPUEncodedFrameDataPtr> ScanTiledVideoOperator::next() {
    if (isComplete_)
        return {};

    if (didSignalEOS_) {
        std::cout << "\nANALYSIS: num-pixels-decoded " << totalNumberOfPixels_ << std::endl;
        std::cout << "ANALYSIS: num-frames-decoded " << totalNumberOfFrames_ << std::endl;
        std::cout << "ANALYSIS: num-bytes-decoded " << totalNumberOfBytes_ << std::endl;
        std::cout << "ANALYSIS: num-tiles-read " << numberOfTilesRead_ << std::endl;
        isComplete_ = true;
        return {};
    }

    if (!currentEncodedFrameReader_ || currentEncodedFrameReader_->isEos()) {
        setUpNextEncodedFrameReader();

        // If the frame reader is still empty, then there are no more frames to read.
        // Flush the decoder.
        if (!currentEncodedFrameReader_) {
            didSignalEOS_ = true;
            CUVIDSOURCEDATAPACKET packet;
            memset(&packet, 0, sizeof(packet));
            packet.flags = CUVID_PKT_ENDOFSTREAM;
            Configuration configuration;
            return std::make_shared<CPUEncodedFrameData>(
                    configuration,
                    DecodeReaderPacket(packet));
        }
    }

    // Otherwise, read and decode frames.
    auto gopPacket = currentEncodedFrameReader_->read();
    assert(gopPacket.has_value());

    Configuration configuration;
    if (tilePathToConfiguration_.count(*currentTilePath_))
        configuration = tilePathToConfiguration_.at(*currentTilePath_);
    else {
        configuration = *tasm::video::GetConfiguration(*currentTilePath_);
        tilePathToConfiguration_[*currentTilePath_] = configuration;
    }

    totalNumberOfPixels_ += gopPacket->numberOfFrames() * currentTileArea_;
    totalNumberOfFrames_ += gopPacket->numberOfFrames();
    totalNumberOfBytes_ += gopPacket->data()->size();

    unsigned long flags = 0;
    auto data = std::make_shared<CPUEncodedFrameData>(configuration, DecodeReaderPacket(*gopPacket->data(), flags));
    data->setFirstFrameIndexAndNumberOfFrames(gopPacket->firstFrameIndex(), gopPacket->numberOfFrames());
    data->setTileNumber(currentTileNumber_);

    return {data};
}

} // namespace tasm