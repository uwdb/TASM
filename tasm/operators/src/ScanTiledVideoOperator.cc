#include "ScanTiledVideoOperator.h"

#include "VideoConfiguration.h"
#include "Stitcher.h"

namespace tasm {

static const unsigned int MAX_PPS_ID = 64;
static const unsigned int ALIGNMENT = 32;

void ScanTiledVideoOperator::preprocess() {
    auto frameIt = semanticDataManager_->orderedFrames().cbegin();
    auto end = semanticDataManager_->orderedFrames().cend();
    while (frameIt != end) {
        auto possibleFramesToRead = nextGroupOfFramesWithTheSameLayoutAndFromTheSameFile(frameIt, end);
        auto tileToFrames = filterToTileFramesThatContainObject(possibleFramesToRead);

        for (auto tileNumberIt = tileToFrames->begin(); tileNumberIt != tileToFrames->end(); ++tileNumberIt) {
            if (tileNumberIt->second->empty())
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

std::shared_ptr<std::vector<int>> ScanTiledVideoOperator::nextGroupOfFramesWithTheSameLayoutAndFromTheSameFile(std::vector<int>::const_iterator &frameIt, std::vector<int>::const_iterator &endIt) {
    assert(frameIt != endIt);

    auto fakeTileNumber = 0;
    // Get the configuration and location for the next frame.
    // While the path is the same, it must have the same configuration.
    currentTilePath_ = std::make_unique<std::experimental::filesystem::path>(tileLocationProvider_->locationOfTileForFrame(fakeTileNumber, *frameIt));
    currentTileLayout_ = tileLocationProvider_->tileLayoutForFrame(*frameIt);

    if (!totalVideoWidth_) {
        assert(!totalVideoHeight_);
        totalVideoWidth_ = currentTileLayout_->totalWidth();
        totalVideoHeight_ = currentTileLayout_->totalHeight();
    }

    auto framesWithSamePathAndConfiguration = std::make_shared<std::vector<int>>();
    framesWithSamePathAndConfiguration->push_back(*frameIt++);
    while (frameIt != endIt) {
        if (tileLocationProvider_->locationOfTileForFrame(fakeTileNumber, *frameIt) == *currentTilePath_)
            framesWithSamePathAndConfiguration->push_back(*frameIt++);
        else
            break;
    }

    return framesWithSamePathAndConfiguration;
}

std::unique_ptr<std::unordered_map<unsigned int, std::shared_ptr<std::vector<int>>>> ScanTiledVideoOperator::filterToTileFramesThatContainObject(std::shared_ptr<std::vector<int>> possibleFrames) {
    auto tileNumberToFrames = std::make_unique<std::unordered_map<unsigned int, std::shared_ptr<std::vector<int>>>>();

    // currentTileLayout is set in nextGroupOfFramesWithTheSameLayoutAndFromTheSameFile().
    if (currentTileLayout_->numberOfTiles() == 1) {
        (*tileNumberToFrames)[0] = possibleFrames;
        return tileNumberToFrames;
    }

    for (auto i = 0u; i < currentTileLayout_->numberOfTiles(); ++i) {
        auto tileRect = currentTileLayout_->rectangleForTile(i);
        if (!tileNumberToFrames->count(i))
            (*tileNumberToFrames)[i] = std::make_shared<std::vector<int>>();
        (*tileNumberToFrames)[i]->reserve(possibleFrames->size());
        for (auto frame = possibleFrames->begin(); frame != possibleFrames->end(); ++frame) {
            auto &rectanglesForFrame = semanticDataManager_->rectanglesForFrame(*frame);
            bool anyIntersect = std::any_of(rectanglesForFrame.begin(), rectanglesForFrame.end(), [&](auto &rectangle) {
                return tileRect.intersects(rectangle);
            });
            if (anyIntersect) {
                (*tileNumberToFrames)[i]->push_back(*frame);
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

static std::vector<unsigned int> ToCtbs(const std::vector<unsigned int> &pixelVals) {
    std::vector<unsigned int> ctbs(pixelVals.size() - 1);
    std::transform(pixelVals.begin(), std::prev(pixelVals.end(), 1), ctbs.begin(), [](auto &v) { return std::ceil(v / ALIGNMENT); });
    return ctbs;
}

void ScanFullFramesFromTiledVideoOperator::setUpNextEncodedFrameReaders() {
    currentEncodedFrameReaders_.clear();
    if (frameIt_ == endFrameIt_)
        return;

    // Get the group of frames with the same layout.
    auto frames = std::make_shared<std::vector<int>>();
    frames->push_back(*frameIt_);
    auto pathOfNextFrameGroup = pathForFrame(*frameIt_++);
    while (frameIt_ != endFrameIt_) {
        if (pathForFrame(*frameIt_) == pathOfNextFrameGroup)
            frames->push_back(*frameIt_++);
    }

    // Create a reader for each tile.
    auto frame = frames->front();
    auto layout = tileLocationProvider_->tileLayoutForFrame(frame);
    for (auto t = 0u; t < layout->numberOfTiles(); ++t) {
        auto tilePath = TileFiles::tileFilename(pathOfNextFrameGroup, t);
        currentEncodedFrameReaders_.push_back(std::make_unique<EncodedFrameReader>(
                tilePath,
                frames,
                tileLocationProvider_->frameOffsetInTileFile(tilePath),
                false));
    }

    // Create the context for this layout.
    std::pair<unsigned int, unsigned int> tileDimensions{layout->numberOfRows(), layout->numberOfColumns()};
    std::pair<unsigned int, unsigned int> videoCodedDimensions{layout->codedHeight(), layout->codedWidth()};
    std::pair<unsigned int, unsigned int> videoDisplayDimensions{layout->totalHeight(), layout->totalWidth()};
    bool shouldUseUniformTiles = false;
    currentContext_ = std::make_unique<stitching::StitchContext>(tileDimensions,
                                videoCodedDimensions,
                                videoDisplayDimensions,
                                shouldUseUniformTiles,
                                ToCtbs(layout->heightsOfRows()),
                                ToCtbs(layout->widthsOfColumns()),
                                ppsId_++);
    if (ppsId_ >= MAX_PPS_ID)
        ppsId_ = 1;
}

GOPReaderPacket ScanFullFramesFromTiledVideoOperator::stitchedDataForNextGOP() {
    // Load the data for each tile.
    std::vector<std::shared_ptr<std::vector<char>>> dataForGOP;
    int numberOfFrames = -1;
    int firstFrameIndex = -1;
    for (auto &reader : currentEncodedFrameReaders_) {
        auto gopPacket = reader->read();
        assert(gopPacket.has_value());
        dataForGOP.push_back(std::shared_ptr(std::move(gopPacket->data())));
        if (numberOfFrames == -1) {
            numberOfFrames = gopPacket->numberOfFrames();
            firstFrameIndex = gopPacket->firstFrameIndex();
        } else {
            assert(gopPacket->numberOfFrames() == numberOfFrames);
            assert(gopPacket->firstFrameIndex() == firstFrameIndex);
        }
    }
    // Reset readers if we're done reading from this tile layout.
    bool doneReadingThisBatch = std::all_of(currentEncodedFrameReaders_.begin(), currentEncodedFrameReaders_.end(),
            [] (const auto &reader) { return reader->isEos(); });
    if (doneReadingThisBatch) {
        assert(!std::any_of(currentEncodedFrameReaders_.begin(), currentEncodedFrameReaders_.end(),
                [] (const auto &reader) { return !reader->isEos(); }));
        currentEncodedFrameReaders_.clear();
    }

    // Stitch the data for the different GOPs.
    stitching::Stitcher stitcher(*currentContext_, dataForGOP);
    return GOPReaderPacket(stitcher.GetStitchedSegments(), firstFrameIndex, numberOfFrames);
}

std::optional<CPUEncodedFrameDataPtr> ScanFullFramesFromTiledVideoOperator::next() {
    if (isComplete_)
        return {};

    if (didSignalEOS_) {
        isComplete_ = true;
        return {};
    }

    // Set up frame readers for next group of frames with the same layout.
    if (currentEncodedFrameReaders_.empty()) {
        setUpNextEncodedFrameReaders();

        // If the readers list is still empty after the set up call, then we are done reading frames.
        // Flush the decoder.
        if (currentEncodedFrameReaders_.empty()) {
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

    // Stitch data for all tiles for this GOP.
    auto packet = stitchedDataForNextGOP();
    unsigned long flags = 0;
    auto data = std::make_shared<CPUEncodedFrameData>(*fullFrameConfig_, DecodeReaderPacket(*packet.data(), flags));
    data->setFirstFrameIndexAndNumberOfFrames(packet.firstFrameIndex(), packet.numberOfFrames());
    // Hardcode tile number 0 because we're simulating a 1x1 tile layout.
    data->setTileNumber(0);

    return {data};
}

std::unique_ptr<Configuration> ScanFullFramesFromTiledVideoOperator::fullFrameConfig() {
    auto firstTileConfig = tasm::video::GetConfiguration(tileLocationProvider_->locationOfTileForFrame(0, 0));
    auto layout = tileLocationProvider_->tileLayoutForFrame(0);
    auto fullFrameConfig = std::make_unique<Configuration>(
            layout->totalWidth(),
            layout->totalHeight(),
            layout->codedWidth(),
            layout->codedHeight(),
            layout->codedWidth(),
            layout->codedHeight(),
            firstTileConfig->frameRate,
            firstTileConfig->codec,
            0);
    return fullFrameConfig;
}

} // namespace tasm