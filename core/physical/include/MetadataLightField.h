//
// Created by Maureen Daum on 2019-06-21.
//

#ifndef LIGHTDB_METADATALIGHTFIELD_H
#define LIGHTDB_METADATALIGHTFIELD_H

#include "Configuration.h"
#include "Geometry.h"
#include "LightField.h"
#include "MaterializedLightField.h"
#include "Metadata.h"
#include "Pool.h"
#include "Rectangle.h"
#include "reference.h"
#include "sqlite3.h"
#include <unordered_set>

#include <map>

namespace lightdb::physical {
    class MetadataLightField;
    using MetadataLightFieldReference = shared_reference<MetadataLightField, pool::BufferPoolEntry<MetadataLightField>>;

    using Metadata = std::unordered_map<std::string, std::vector<Rectangle>>;


    class MetadataLightField: public MaterializedLightField {
    public:
        // Maybe specify default constructors/destructors/operators.
        // Similar to FrameData, except without DeviceType.
        MetadataLightField(const Metadata &metadata, Configuration configuration, GeometryReference geometry)
        : MaterializedLightField(DeviceType::GPU),
            metadata_(metadata),
            configuration_(std::move(configuration)),
            geometry_(geometry)
        { }

//        inline void accept(LightFieldVisitor& visitor) override { visitor.visit(*this); }

        inline MaterializedLightFieldReference ref() const override { return MaterializedLightFieldReference::make<MetadataLightField>(*this); }

        inline Metadata metadata() { return metadata_; }

        std::vector<Rectangle> allRectangles();

    private:
        // Store metadata.
        // For now, use a dictionary to store it.
        const Metadata metadata_;
        const Configuration configuration_;
        const GeometryReference geometry_;
    };
} // namespace lightdb::physical

namespace lightdb::metadata {
    class MetadataManager {
    public:
        explicit MetadataManager(const std::filesystem::path &pathToVideo, const MetadataSpecification &metadataSpecification)
                : pathToVideo_(pathToVideo),
                metadataSpecification_(metadataSpecification),
                didSetFramesForMetadata_(false),
                framesForMetadata_(0),
                didSetOrderedFramesForMetadata_(false),
                orderedFramesForMetadata_(0),
                didSetIdealKeyframesForMetadata_(false),
                idealKeyframesForMetadata_(0)
        {}

        const std::unordered_set<int> &framesForMetadata() const;
        const std::vector<int> &orderedFramesForMetadata() const;
        const std::unordered_set<int> &idealKeyframesForMetadata() const;

        const std::vector<Rectangle> &rectanglesForFrame(int frame) const;

        static std::unordered_set<int> idealKeyframesForFrames(const std::vector<int> &orderedFrames);

    private:
        void selectFromMetadataAndApplyFunction(const char* query, std::function<void(sqlite3_stmt*)> resultFn) const;

        const std::filesystem::path pathToVideo_;
        const MetadataSpecification metadataSpecification_;
        mutable bool didSetFramesForMetadata_;
        mutable std::unordered_set<int> framesForMetadata_;
        mutable bool didSetOrderedFramesForMetadata_;
        mutable std::vector<int> orderedFramesForMetadata_;
        mutable bool didSetIdealKeyframesForMetadata_;
        mutable std::unordered_set<int> idealKeyframesForMetadata_;
        mutable std::unordered_map<int, std::vector<lightdb::Rectangle>> frameToRectangles_;
    };
} // namespace lightdb::metadata

namespace lightdb::logical {
    class MetadataSubsetLightField : public LightField {
    public:
        MetadataSubsetLightField(const LightFieldReference &lightField, const MetadataSpecification &metadataSpecification, MetadataSubsetType subsetType, const catalog::Source &source)
        : LightField(lightField),
        metadataSelection_(metadataSpecification),
        subsetType_(subsetType),
        source_(source),
        metadataManager_(source_.filename(), metadataSelection_)
        {
//            setKeyframesInOptions();
        }

        void accept(LightFieldVisitor &visitor) override { LightField::accept<MetadataSubsetLightField>(visitor); }
        const MetadataSpecification &metadataSpecification() const { return metadataSelection_; }
        MetadataSubsetType subsetType() const { return subsetType_; }
        const catalog::Source &source() const { return source_; }

        // Should there be different classes of metadata selection? e.g. frames, pixels?
        const std::unordered_set<int> &framesForMetadata() const { return metadataManager_.framesForMetadata(); }
        const std::vector<int> &orderedFramesForMetadata() const { return metadataManager_.orderedFramesForMetadata(); }
        std::pair<std::vector<int>, std::vector<int>> sequentialFramesAndNonSequentialFrames() const {
            return source().mp4Reader().frameSequencesInSequentialGOPsAndNonSequentialGOPs(orderedFramesForMetadata());
        }

        const std::vector<Rectangle> &rectanglesForFrame(int frame) const { return metadataManager_.rectanglesForFrame(frame); }
    private:
        MetadataSpecification metadataSelection_;
        MetadataSubsetType subsetType_;
        catalog::Source source_;
        metadata::MetadataManager metadataManager_;
    };
} // namespace lightdb::logical



#endif //LIGHTDB_METADATALIGHTFIELD_H
