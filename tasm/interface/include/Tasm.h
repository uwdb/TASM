#ifndef TASM_TASM_H
#define TASM_TASM_H

#include "SemanticIndex.h"
#include "SemanticSelection.h"
#include "TemporalSelection.h"
#include "VideoManager.h"

#include <memory>
#include <string>

namespace tasm {

class TASM {
public:
    TASM(const std::experimental::filesystem::path &dbPath = EnvironmentConfiguration::instance().defaultLabelsDatabasePath())
        : TASM(SemanticIndex::IndexType::XY, dbPath)
    {}

    TASM(SemanticIndex::IndexType indexType, const std::experimental::filesystem::path &dbPath = EnvironmentConfiguration::instance().defaultLabelsDatabasePath())
        : semanticIndex_(SemanticIndexFactory::create(indexType, dbPath))
    {}

    TASM(const TASM&) = delete;

    virtual void addMetadata(const std::string &video,
            const std::string &label,
            unsigned int frame,
            unsigned int x1,
            unsigned int y1,
            unsigned int x2,
            unsigned int y2);

    virtual void addBulkMetadata(const std::vector<MetadataInfo>&);

    virtual void store(const std::string &videoPath, const std::string &savedName) {
        videoManager_.store(videoPath, savedName);
    }

    virtual void storeWithUniformLayout(const std::string &videoPath, const std::string &savedName, unsigned int rows, unsigned int columns) {
        videoManager_.storeWithUniformLayout(videoPath, savedName, rows, columns);
    }

    virtual void storeWithNonUniformLayout(const std::string &videoPath, const std::string &savedName, const std::string &metadataIdentifier, const std::string &labelToTileAround, bool force = true) {
        videoManager_.storeWithNonUniformLayout(videoPath, savedName, metadataIdentifier, std::make_shared<SingleMetadataSelection>(labelToTileAround), semanticIndex_, force);
    }

    virtual std::unique_ptr<ImageIterator> select(const std::string &video, const std::string &label, const std::string &metadataIdentifier = "") {
        return select(video, label, std::shared_ptr<TemporalSelection>(), metadataIdentifier);
    }

    virtual std::unique_ptr<ImageIterator> select(const std::string &video, const std::string &label, unsigned int frame, const std::string &metadataIdentifier = "") {
        return select(video, label, std::make_shared<EqualTemporalSelection>(frame), metadataIdentifier);
    }

    virtual std::unique_ptr<ImageIterator> select(const std::string &video,
                         const std::string &label,
                         unsigned int firstFrameInclusive,
                         unsigned int lastFrameExclusive,
                         const std::string &metadataIdentifier = "") {
        return select(video, label, std::make_shared<RangeTemporalSelection>(firstFrameInclusive, lastFrameExclusive), metadataIdentifier);
    }

    virtual std::unique_ptr<ImageIterator> selectTiles(const std::string &video,
                const std::string &label,
                const std::string &metadataIdentifier = "") {
        return select(video, label, std::shared_ptr<TemporalSelection>(), metadataIdentifier, SelectStrategy::Tiles);
    }

    virtual std::unique_ptr<ImageIterator> selectTiles(const std::string &video,
                                                       const std::string &label,
                                                       unsigned int firstFrameInclusive,
                                                       unsigned int lastFrameExclusive,
                                                       const std::string &metadataIdentifier = "") {
        return select(video, label, std::make_shared<RangeTemporalSelection>(firstFrameInclusive, lastFrameExclusive), metadataIdentifier, SelectStrategy::Tiles);
    }

    virtual std::unique_ptr<ImageIterator> selectFrames(const std::string &video,
            const std::string &label,
            const std::string &metadataIdentifier = "") {
        return select(video, label, std::shared_ptr<TemporalSelection>(), metadataIdentifier, SelectStrategy::Frames);
    }

    virtual std::unique_ptr<ImageIterator> selectFrames(const std::string &video,
                                                        const std::string &label,
                                                        unsigned int firstFrameInclusive,
                                                        unsigned int lastFrameExclusive,
                                                        const std::string &metadataIdentifier = "") {
        return select(video, label, std::make_shared<RangeTemporalSelection>(firstFrameInclusive, lastFrameExclusive), metadataIdentifier, SelectStrategy::Frames);
    }

    void retileVideoBasedOnRegret(const std::string &video) {
        videoManager_.retileVideoBasedOnRegret(video);
    }

    void activateRegretBasedTilingForVideo(const std::string &video, const std::string &metadataIdentifier = "", double threshold = 0) {
        videoManager_.activateRegretBasedRetilingForVideo(video, metadataIdentifier.length() ? metadataIdentifier : video, semanticIndex_, threshold);
    }

    void deactivateRegretBasedTilingForVideo(const std::string &video) {
        videoManager_.deactivateRegretBasedRetilingForVideo(video);
    }

    virtual ~TASM() = default;

    std::shared_ptr<SemanticIndex> semanticIndex() const {
        return semanticIndex_;
    }

private:
    std::unique_ptr<ImageIterator> select(const std::string &video, const std::string &label, std::shared_ptr<TemporalSelection> temporalSelection, const std::string &metadataIdentifier, SelectStrategy strategy=SelectStrategy::Objects) {
        return videoManager_.select(
                video,
                metadataIdentifier.length() ? metadataIdentifier : video,
                std::make_shared<SingleMetadataSelection>(label),
                temporalSelection,
                semanticIndex_,
                strategy);
    }

    std::shared_ptr<SemanticIndex> semanticIndex_;
    VideoManager videoManager_;
};

} // namespace tasm

#endif //TASM_TASM_H
