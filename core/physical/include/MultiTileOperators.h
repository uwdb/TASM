#ifndef LIGHTDB_MULTITILEROPERATORS_H
#define LIGHTDB_MULTITILEROPERATORS_H

#include "PhysicalOperators.h"

#include "Gpac.h"


namespace lightdb::physical {

class ScanMultiTilePlaceholderOperator : public PhysicalOperator {
public:
    explicit ScanMultiTilePlaceholderOperator(const LightFieldReference &logical)
            : PhysicalOperator(logical, DeviceType::CPU, runtime::make<Runtime>(*this, "ScanMultiTilePlaceholderOperator-init"))
    { }

    const logical::ScannedMultiTiledLightField &multiTiledLightField() const { return logical().downcast<logical::ScannedMultiTiledLightField>(); }

private:
    class Runtime: public runtime::Runtime<ScanMultiTilePlaceholderOperator> {
    public:
        explicit Runtime(ScanMultiTilePlaceholderOperator &physical)
            : runtime::Runtime<ScanMultiTilePlaceholderOperator>(physical)
        {
            // This runtime should never be created.
            assert(false);
        }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            // This operator should never end up in an actual plan.
            assert(false);
            return {};
        }
    };
};

class ScanMultiTileOperator : public PhysicalOperator {
public:
    explicit ScanMultiTileOperator(const LightFieldReference &logical,
            std::shared_ptr<const metadata::MetadataManager> metadataManager,
            std::shared_ptr<const tiles::TileLocationProvider> tileLocationProvider,
            bool shouldReadEntireGOPs = false)
        : PhysicalOperator(logical, DeviceType::CPU, runtime::make<Runtime>(*this, "ScanMultiTileOperator-init", tileLocationProvider, shouldReadEntireGOPs)),
        tileNumber_(0),
        metadataManager_(metadataManager),
        tileLocationProvider_(tileLocationProvider)
    {
        assert(metadataManager_);
        assert(tileLocationProvider_);
    }

    const std::shared_ptr<const metadata::MetadataManager> metadataManager() const { return metadataManager_; }

private:
    class Runtime: public runtime::Runtime<ScanMultiTileOperator> {
    private:
        struct TileInformation {
            std::filesystem::path filename;
            int tileNumber;
            unsigned int width;
            unsigned int height;
            std::vector<int> framesToRead;
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

    public:
        explicit Runtime(ScanMultiTileOperator &physical, std::shared_ptr<const tiles::TileLocationProvider> tileLocationProvider, bool shouldReadEntireGOPs)
            : runtime::Runtime<ScanMultiTileOperator>(physical),
                    tileNumberForCurrentLayout_(0),
                    tileLocationProvider_(tileLocationProvider),
                    framesIterator_(physical.metadataManager()->orderedFramesForMetadata().begin()),
                    endOfFramesIterator_(physical.metadataManager()->orderedFramesForMetadata().end()),
                    shouldReadEntireGOPs_(shouldReadEntireGOPs),
                    totalVideoWidth_(0),
                    totalVideoHeight_(0),
                    totalNumberOfPixels_(0),
                    totalNumberOfIFrames_(0),
                    totalNumberOfFrames_(0),
                    totalNumberOfBytes_(0),
                    numberOfTilesRead_(0),
                    didSignalEOS_(false),
                    currentTileNumber_(0)
        {
            preprocess();
        }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            // Find all of the frames that have to be read from the same file, and create an EncodedFrameReader for them.
            // Iterate over global frames.
            // For each frame, see if this tile overlaps with any objects on that frame, given the tile layout.
            // If any do, add the frame to the list of frames to read.
            // Once it comes across a new tile layout or new file, pass the read frames on to the decoder, & include the width/height.

            // We're done once we've read all of the frames, and the frames are actually done decoding.
            // TODO: and this is the frame reader for the last tile.
//            if (framesIterator_ == endOfFramesIterator_
//                    && currentEncodedFrameReader_
//                    && currentEncodedFrameReader_->isEos()
//                    && tileNumberIt_ == tileNumbersForCurrentLayout_.end()) {
//                std::cout << "ANALYSIS: Total number of pixels decoded " << totalNumberOfPixels_ << std::endl;
//                std::cout << "ANALYSIS: Total number of keyframes decoded " << totalNumberOfIFrames_ << std::endl;
//                std::cout << "ANALYSIS: num-frames-decoded " << totalNumberOfFrames_ << std::endl;
//                std::cout << "ANALYSIS: num-bytes-decoded " << totalNumberOfBytes_ << std::endl;
//                std::cout << "ANALYSIS: num-tiles-read " << numberOfTilesRead_ << std::endl;
//                return {};
//            }

            if (didSignalEOS_) {
                std::cout << "ANALYSIS: num-pixels-decoded " << totalNumberOfPixels_ << std::endl;
                std::cout << "ANALYSIS: num-keyframes-decoded " << totalNumberOfIFrames_ << std::endl;
                std::cout << "ANALYSIS: num-frames-decoded " << totalNumberOfFrames_ << std::endl;
                std::cout << "ANALYSIS: num-bytes-decoded " << totalNumberOfBytes_ << std::endl;
                std::cout << "ANALYSIS: num-tiles-read " << numberOfTilesRead_ << std::endl;
                return {};
            }

            if (!currentEncodedFrameReader_ || currentEncodedFrameReader_->isEos()) {
                do {
                    setUpNextEncodedFrameReader();
                } while (framesIterator_ != endOfFramesIterator_ && !currentEncodedFrameReader_);

                // If we get out of the loop and the frame reader is still empty, then there are no more frames to read.
                if (!currentEncodedFrameReader_) {
//                    std::cout << "ANALYSIS: num-pixels-decoded " << totalNumberOfPixels_ << std::endl;
//                    std::cout << "ANALYSIS: num-keyframes-decoded " << totalNumberOfIFrames_ << std::endl;
//                    std::cout << "ANALYSIS: num-frames-decoded " << totalNumberOfFrames_ << std::endl;
//                    std::cout << "ANALYSIS: num-bytes-decoded " << totalNumberOfBytes_ << std::endl;
//                    return {};
//
//                }
                    didSignalEOS_ = true;
                    CUVIDSOURCEDATAPACKET packet;
                    memset(&packet, 0, sizeof(packet));
                    packet.flags = CUVID_PKT_ENDOFSTREAM;
                    Configuration configuration;
                    GeometryReference geometry = GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples());
                    return MaterializedLightFieldReference::make<CPUEncodedFrameData>(
                            Codec::hevc(),
                            configuration,
                            geometry,
                            DecodeReaderPacket(packet));
                }
            }

            // Read frames from reader.
            auto gopPacket = currentEncodedFrameReader_->read();
            assert(gopPacket.has_value());

            unsigned long flags = 0; //CUVID_PKT_DISCONTINUITY | CUVID_PKT_ENDOFSTREAM;
//            if (currentEncodedFrameReader_->isEos())
//                flags |= CUVID_PKT_ENDOFSTREAM;

            GeometryReference geometry = GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples());

            Configuration configuration;
            if (tilePathToConfiguration_.count(*currentTilePath_))
                configuration = tilePathToConfiguration_.at(*currentTilePath_);
            else {
                configuration = video::gpac::load_configuration(*currentTilePath_);
                tilePathToConfiguration_[*currentTilePath_] = configuration;
            }

            totalNumberOfPixels_ += gopPacket->numberOfFrames() * currentTileArea_; //currentTileLayout_->rectangleForTile(*tileNumberIt_).area();
            ++totalNumberOfIFrames_;
            totalNumberOfFrames_ += gopPacket->numberOfFrames();
            totalNumberOfBytes_ += gopPacket->data()->size();

            assert(totalVideoWidth_);
            assert(totalVideoHeight_);
            // TODO: Figure out how to get maximum coded width/height.
            configuration.max_width = std::max(totalVideoWidth_, configuration.max_width);
            configuration.max_height = std::max(totalVideoHeight_, configuration.max_height);

            auto cpuData = MaterializedLightFieldReference::make<CPUEncodedFrameData>(
                    Codec::hevc(),
                    configuration,
                    geometry,
                    DecodeReaderPacket(*gopPacket->data(), flags));
            cpuData.downcast<CPUEncodedFrameData>().setFirstFrameIndexAndNumberOfFrames(gopPacket->firstFrameIndex(), gopPacket->numberOfFrames());
            cpuData.downcast<CPUEncodedFrameData>().setTileNumber(currentTileNumber_);

            return {cpuData};
        }

    private:
        void preprocess();

        void setUpNextEncodedFrameReader() {
            if (orderedTileInformationIt_ == orderedTileInformation_.end()) {
                currentEncodedFrameReader_ = nullptr;
            } else {
                ++numberOfTilesRead_;
                READ_FROM_NEW_FILE_TIMER.startSection("setUpNewReader");

                currentEncodedFrameReader_ = std::make_unique<EncodedFrameReader>(
                        orderedTileInformationIt_->filename,
                        orderedTileInformationIt_->framesToRead,
                        orderedTileInformationIt_->frameOffsetInFile,
                        shouldReadEntireGOPs_);

                currentTileArea_ = orderedTileInformationIt_->width * orderedTileInformationIt_->height;
                currentTilePath_ = std::make_unique<std::filesystem::path>(orderedTileInformationIt_->filename);
                currentTileNumber_ = orderedTileInformationIt_->tileNumber;

                ++orderedTileInformationIt_;
                READ_FROM_NEW_FILE_TIMER.endSection("setUpNewReader");
            }


            // TODO: If there is another tile with the same layout that contains objects, set up the encoded reader for that.
//            if (!tileNumbersForCurrentLayout_.empty() && std::next(tileNumberIt_, 1) != tileNumbersForCurrentLayout_.end()) {
//                ++tileNumberIt_;
//                ++numberOfTilesRead_;
//                READ_FROM_NEW_FILE_TIMER.startSection("setUpNewReader");
//                currentEncodedFrameReader_->setNewFileWithSameKeyframes(catalog::TileFiles::tileFilename(currentTilePath_->parent_path(), *tileNumberIt_));
//                READ_FROM_NEW_FILE_TIMER.endSection("setUpNewReader");
//            } else if (framesIterator_ != endOfFramesIterator_) {
//                // Get all of next frames that are in the same file.
//                auto possibleFramesToRead = nextGroupOfFramesWithTheSameLayoutAndFromTheSameFile();
//
//                // Filter out the ones that don't contain any of the object.
//                auto lastIntersectingFrameIt = removeFramesThatDoNotContainObject(possibleFramesToRead);
//                possibleFramesToRead.resize(std::distance(possibleFramesToRead.begin(), lastIntersectingFrameIt));
//                if (!possibleFramesToRead.size()) {
//                    currentEncodedFrameReader_ = nullptr;
//                    return;
//                }
//
//                // Create an frame reader with the frames.
//                // TODO: Enable creating the encoded frame reader with a frame offset, so that it can tell the mp4 reader to read the correct samples.
//                ++numberOfTilesRead_;
//                READ_FROM_NEW_FILE_TIMER.startSection("setUpNewReader");
////                if (currentEncodedFrameReader_) {
////                    currentEncodedFrameReader_->setNewFileWithSameKeyframesButNewFrames(
////                            catalog::TileFiles::tileFilename(currentTilePath_->parent_path(), *tileNumberIt_),
////                            possibleFramesToRead,
////                            tileLocationProvider_->frameOffsetInTileFile(*currentTilePath_));
////                } else {
//                    currentEncodedFrameReader_ = std::make_unique<EncodedFrameReader>(
//                        catalog::TileFiles::tileFilename(currentTilePath_->parent_path(), *tileNumberIt_),
//                        possibleFramesToRead,
//                        tileLocationProvider_->frameOffsetInTileFile(*currentTilePath_),
//                        shouldReadEntireGOPs_);
////                }
//                READ_FROM_NEW_FILE_TIMER.endSection("setUpNewReader");
//            } else
//                currentEncodedFrameReader_ = nullptr;
        }

        // Updates current tile path & layout.
        std::vector<int> nextGroupOfFramesWithTheSameLayoutAndFromTheSameFile() {
            assert(framesIterator_ != endOfFramesIterator_);

            auto fakeTileNumber = 0;
            // Get the configuration and location for the next frame.
            // While the path is the same, it must have the same configuration.
            currentTilePath_ = std::make_unique<std::filesystem::path>(tileLocationProvider_->locationOfTileForFrame(fakeTileNumber, *framesIterator_));
            currentTileLayout_ = std::make_unique<tiles::TileLayout>(tileLocationProvider_->tileLayoutForFrame(*framesIterator_));

            if (!totalVideoWidth_) {
                assert(!totalVideoHeight_);
                totalVideoWidth_ = currentTileLayout_->totalWidth();
                totalVideoHeight_ = currentTileLayout_->totalHeight();
            }

            std::vector<int> framesWithSamePathAndConfiguration({ *framesIterator_++ });
            while (framesIterator_ != endOfFramesIterator_) {
                if (tileLocationProvider_->locationOfTileForFrame(fakeTileNumber, *framesIterator_) == *currentTilePath_)
                    framesWithSamePathAndConfiguration.push_back(*framesIterator_++);
                else
                    break;
            }

            return framesWithSamePathAndConfiguration;
        }

        // Returns an iterator past the last frame that should be read.
        // Also set up which tiles contain the object.
        std::vector<int>::iterator removeFramesThatDoNotContainObject(std::vector<int> &possibleFrames) {
            tileNumberForCurrentLayoutToFrames_.clear();
            // For each tile, find the maximum frame where any object intersects.
            // This could be more efficient by passing that maximum frame to the encoded file reader.
            int maximumFrame = 0;

            for (auto i = 0u; i < currentTileLayout_->numberOfTiles(); ++i) {
                auto tileRect = currentTileLayout_->rectangleForTile(i);
                tileNumberForCurrentLayoutToFrames_[i].reserve(possibleFrames.size());
                for (auto frame = possibleFrames.begin(); frame != possibleFrames.end(); ++frame) {
                    auto &rectanglesForFrame = physical().metadataManager()->rectanglesForFrame(*frame);
                    bool anyIntersect = std::any_of(rectanglesForFrame.begin(), rectanglesForFrame.end(), [&](auto &rectangle) {
                        return tileRect.intersects(rectangle);
                    });
                    if (anyIntersect) {
                        tileNumberForCurrentLayoutToFrames_[i].push_back(*frame);
                    }
                }
            }
            return possibleFrames.begin();
        }

        unsigned int tileNumberForCurrentLayout_;
        const std::shared_ptr<const tiles::TileLocationProvider> tileLocationProvider_;
        std::vector<int>::const_iterator framesIterator_;
        std::vector<int>::const_iterator endOfFramesIterator_;
        bool shouldReadEntireGOPs_;

        unsigned int totalVideoWidth_;
        unsigned int totalVideoHeight_;

        unsigned long long int totalNumberOfPixels_;
        unsigned long long int totalNumberOfIFrames_;
        unsigned long long int totalNumberOfFrames_;
        unsigned long long int totalNumberOfBytes_;
        unsigned int numberOfTilesRead_;
        bool didSignalEOS_;

        std::vector<TileInformation> orderedTileInformation_;
        std::vector<TileInformation>::const_iterator orderedTileInformationIt_;
        unsigned int currentTileArea_;

        std::unique_ptr<EncodedFrameReader> currentEncodedFrameReader_;
        std::unique_ptr<tiles::TileLayout> currentTileLayout_;
        std::unique_ptr<std::filesystem::path> currentTilePath_;
        unsigned int currentTileNumber_;
        std::unordered_map<std::string, Configuration> tilePathToConfiguration_;

        std::unordered_map<unsigned int, std::vector<int>> tileNumberForCurrentLayoutToFrames_;
        std::unordered_map<unsigned int, std::vector<int>>::const_iterator tileNumberToFramesIt_;

        std::vector<int> tileNumbersForCurrentLayout_;
        std::vector<int>::const_iterator tileNumberIt_;
    };

    unsigned int tileNumber_;
    const std::shared_ptr<const metadata::MetadataManager> metadataManager_;
    const std::shared_ptr<const tiles::TileLocationProvider> tileLocationProvider_;

};

class DecodeMultipleStreams : public PhysicalOperator {
    // TODO: How will merge operator know what tile it is reading data for?
public:
    explicit DecodeMultipleStreams(const LightFieldReference &logical,
            std::vector<PhysicalOperatorReference> parents)
        : PhysicalOperator(logical, parents, DeviceType::CPU, runtime::make<Runtime>(*this, "DecodeMultipleStreams-init"))
    { }

private:
    class Runtime: public runtime::Runtime<DecodeMultipleStreams> {
    public:
        explicit Runtime(DecodeMultipleStreams &physical)
            : runtime::Runtime<DecodeMultipleStreams>(physical)
        { }

        std::optional<MaterializedLightFieldReference> read() override {
            // Will read CPUEncodedData from each scanner.
            // Pass on to decoder instance.
            // Read data from each decoder and pass on group of decoded data to next operator.
            return {};
        }
    };
};

} // namespace lightdb::physical

#endif //LIGHTDB_MULTITILEROPERATORS_H
