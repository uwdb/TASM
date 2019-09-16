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
            unsigned int tileNumber,
            std::shared_ptr<const metadata::MetadataManager> metadataManager,
            std::shared_ptr<const tiles::TileLocationProvider> tileLocationProvider)
        : PhysicalOperator(logical, DeviceType::CPU, runtime::make<Runtime>(*this, "ScanMultiTileOperator-init", tileNumber, tileLocationProvider)),
        tileNumber_(tileNumber),
        metadataManager_(metadataManager),
        tileLocationProvider_(tileLocationProvider)
    {
        assert(metadataManager_);
        assert(tileLocationProvider_);
    }

    const std::shared_ptr<const metadata::MetadataManager> metadataManager() const { return metadataManager_; }

private:
    class Runtime: public runtime::Runtime<ScanMultiTileOperator> {
    public:
        explicit Runtime(ScanMultiTileOperator &physical, unsigned int tileNumber, std::shared_ptr<const tiles::TileLocationProvider> tileLocationProvider)
            : runtime::Runtime<ScanMultiTileOperator>(physical),
                    tileNumber_(tileNumber),
                    tileLocationProvider_(tileLocationProvider),
                    framesIterator_(physical.metadataManager()->orderedFramesForMetadata().begin()),
                    endOfFramesIterator_(physical.metadataManager()->orderedFramesForMetadata().end()),
                    totalVideoWidth_(0),
                    totalVideoHeight_(0)
        { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            // Find all of the frames that have to be read from the same file, and create an EncodedFrameReader for them.
            // Iterate over global frames.
            // For each frame, see if this tile overlaps with any objects on that frame, given the tile layout.
            // If any do, add the frame to the list of frames to read.
            // Once it comes across a new tile layout or new file, pass the read frames on to the decoder, & include the width/height.

            // We're done once we've read all of the frames, and the frames are actually done decoding.
            if (framesIterator_ == endOfFramesIterator_ && currentEncodedFrameReader_ && currentEncodedFrameReader_->isEos())
                return {};

            if (!currentEncodedFrameReader_ || currentEncodedFrameReader_->isEos()) {
                do {
                    setUpNextEncodedFrameReader();
                } while (framesIterator_ != endOfFramesIterator_ && !currentEncodedFrameReader_);

                // If we get out of the loop and the frame reader is still empty, then there are no more frames to read.
                if (!currentEncodedFrameReader_)
                    return {};
            }

            // Read frames from reader.
            auto gopPacket = currentEncodedFrameReader_->read();
            assert(gopPacket.has_value());
            assert(gopPacket->firstFrameIndex() >= tileLocationProvider_->frameOffsetInTileFile(*currentTilePath_));

            unsigned long flags = CUVID_PKT_DISCONTINUITY;
            if (currentEncodedFrameReader_->isEos())
                flags |= CUVID_PKT_ENDOFSTREAM;

            GeometryReference geometry = GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples());
            Configuration configuration = video::gpac::load_configuration(*currentTilePath_);

            assert(totalVideoWidth_);
            assert(totalVideoHeight_);
            configuration.max_width = totalVideoWidth_;
            configuration.max_height = totalVideoHeight_;

            auto cpuData = MaterializedLightFieldReference::make<CPUEncodedFrameData>(
                    Codec::hevc(),
                    configuration,
                    geometry,
                    DecodeReaderPacket(gopPacket->data(), flags));
            cpuData.downcast<CPUEncodedFrameData>().setFirstFrameIndexAndNumberOfFrames(gopPacket->firstFrameIndex(), gopPacket->numberOfFrames());

            return {cpuData};
        }

    private:
        void setUpNextEncodedFrameReader() {
            // Get all of next frames that are in the same file.
            auto possibleFramesToRead = nextGroupOfFramesWithTheSameLayoutAndFromTheSameFile();

            // Filter out the ones that don't contain any of the object.
            auto lastIntersectingFrameIt = removeFramesThatDoNotContainObject(possibleFramesToRead);
            possibleFramesToRead.resize(std::distance(possibleFramesToRead.begin(), lastIntersectingFrameIt));
            if (!possibleFramesToRead.size()) {
                currentEncodedFrameReader_ = nullptr;
                return;
            }

            // Create an frame reader with the frames.
            // TODO: Enable creating the encoded frame reader with a frame offset, so that it can tell the mp4 reader to read the correct samples.
            currentEncodedFrameReader_ = std::make_unique<EncodedFrameReader>(
                                                *currentTilePath_,
                                                possibleFramesToRead,
                                                tileLocationProvider_->frameOffsetInTileFile(*currentTilePath_));
        }

        // Updates current tile path & layout.
        std::vector<int> nextGroupOfFramesWithTheSameLayoutAndFromTheSameFile() {
            // Get the configuration and location for the next frame.
            // While the path is the same, it must have the same configuration.
            currentTilePath_ = std::make_unique<std::filesystem::path>(tileLocationProvider_->locationOfTileForFrame(tileNumber_, *framesIterator_));
            currentTileLayout_ = std::make_unique<tiles::TileLayout>(tileLocationProvider_->tileLayoutForFrame(*framesIterator_));

            if (!totalVideoWidth_) {
                assert(!totalVideoHeight_);
                totalVideoWidth_ = currentTileLayout_->totalWidth();
                totalVideoHeight_ = currentTileLayout_->totalHeight();
            }

            std::vector<int> framesWithSamePathAndConfiguration({ *framesIterator_++ });
            while (framesIterator_ != endOfFramesIterator_) {
                if (tileLocationProvider_->locationOfTileForFrame(tileNumber_, *framesIterator_) == *currentTilePath_)
                    framesWithSamePathAndConfiguration.push_back(*framesIterator_++);
                else
                    break;
            }

            return framesWithSamePathAndConfiguration;
        }

        // Returns an iterator past the last frame that should be read.
        std::vector<int>::iterator removeFramesThatDoNotContainObject(std::vector<int> &possibleFrames) {
            // Get rectangle for tile in current tile layout.
            if (tileNumber_ >= currentTileLayout_->numberOfTiles())
                return possibleFrames.begin();

            auto tileRect = currentTileLayout_->rectangleForTile(tileNumber_);

            return std::remove_if(possibleFrames.begin(), possibleFrames.end(), [&](auto &frameNumber) {
               // Return true if there is no intersection.
               auto &rectanglesForFrame = physical().metadataManager()->rectanglesForFrame(frameNumber);
               return std::none_of(rectanglesForFrame.begin(), rectanglesForFrame.end(), [&](auto &rectangle) {
                   return tileRect.intersects(rectangle);
               });
            });
        }

        unsigned int tileNumber_;
        const std::shared_ptr<const tiles::TileLocationProvider> tileLocationProvider_;
        std::vector<int>::const_iterator framesIterator_;
        std::vector<int>::const_iterator endOfFramesIterator_;

        unsigned int totalVideoWidth_;
        unsigned int totalVideoHeight_;

        std::unique_ptr<EncodedFrameReader> currentEncodedFrameReader_;
        std::unique_ptr<tiles::TileLayout> currentTileLayout_;
        std::unique_ptr<std::filesystem::path> currentTilePath_;
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
