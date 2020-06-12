#ifndef TASM_SEMANTICDATAMANAGER_H
#define TASM_SEMANTICDATAMANAGER_H

#include "Rectangle.h"
#include "SemanticIndex.h"
#include "SemanticSelection.h"
#include "TemporalSelection.h"

namespace tasm {

class SemanticDataManager {
public:
    SemanticDataManager(std::shared_ptr<SemanticIndex> index,
            const std::string &video,
            std::shared_ptr<MetadataSelection> metadataSelection,
            std::shared_ptr<TemporalSelection> temporalSelection)
            : index_(index),
            video_(video),
            metadataSelection_(metadataSelection),
            temporalSelection_(temporalSelection)
    {}

    const std::vector<int> &orderedFrames() {
        if (orderedFrames_)
            return *orderedFrames_;

        orderedFrames_ = index_->orderedFramesForSelection(video_, metadataSelection_, temporalSelection_);
        return *orderedFrames_;
    }

    const std::list<Rectangle> &rectanglesForFrame(int frame) {
        if (frameToRectangles_.count(frame))
            return *frameToRectangles_.at(frame);

        frameToRectangles_.emplace(frame, std::move(index_->rectanglesForFrame(video_, metadataSelection_, frame)));
        return *frameToRectangles_.at(frame);
    }

    std::unique_ptr<std::list<Rectangle>> rectanglesForFrames(int firstFrameInclusive, int lastFrameExclusive) {
        return index_->rectanglesForFrames(video_, metadataSelection_, firstFrameInclusive, lastFrameExclusive);
    }

private:
    std::shared_ptr<SemanticIndex> index_;
    std::string video_;
    std::shared_ptr<MetadataSelection> metadataSelection_;
    std::shared_ptr<TemporalSelection> temporalSelection_;

    std::unique_ptr<std::vector<int>> orderedFrames_;
    std::unordered_map<int, std::unique_ptr<std::list<Rectangle>>> frameToRectangles_;
};

} // namespace tasm

#endif //TASM_SEMANTICDATAMANAGER_H
