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

        static std::unordered_set<int> idealKeyframesForFrames(const std::vector<int> &orderedFrames);


    private:
        const std::filesystem::path pathToVideo_;
        const MetadataSpecification metadataSpecification_;
        mutable bool didSetFramesForMetadata_;
        mutable std::unordered_set<int> framesForMetadata_;
        mutable bool didSetOrderedFramesForMetadata_;
        mutable std::vector<int> orderedFramesForMetadata_;
        mutable bool didSetIdealKeyframesForMetadata_;
        mutable std::unordered_set<int> idealKeyframesForMetadata_;
    };
} // namespace lightdb::metadata

namespace lightdb::logical {
    class MetadataSubsetLightField : public LightField {
    public:
        MetadataSubsetLightField(const LightFieldReference &lightField, const MetadataSpecification &metadataSpecification, const catalog::Source &source)
        : LightField(lightField),
        metadataSelection_(metadataSpecification),
        source_(source),
        metadataManager_(source_.filename(), metadataSelection_)
        {
//            setKeyframesInOptions();
        }

        void accept(LightFieldVisitor &visitor) override { LightField::accept<MetadataSubsetLightField>(visitor); }
        const MetadataSpecification &metadataSpecification() const { return metadataSelection_; }
        const catalog::Source &source() const { return source_; }
        const std::unordered_set<int> &framesForMetadata() const { return metadataManager_.framesForMetadata(); }
        const std::vector<int> &orderedFramesForMetadata() const { return metadataManager_.orderedFramesForMetadata(); }
        std::pair<std::vector<int>, std::vector<int>> sequentialFramesAndNonSequentialFrames() const {
            return source().mp4Reader().frameSequencesInSequentialGOPsAndNonSequentialGOPs(orderedFramesForMetadata());
        }

//        const lightdb::options<>& options() const override { return options_; }

    private:
//        void setKeyframesInOptions() {
//            options_[EncodeOptions::Keyframes] = metadataManager_.idealKeyframesForMetadata();
//        }

        MetadataSpecification metadataSelection_;
        catalog::Source source_;
        metadata::MetadataManager metadataManager_;
//        lightdb::options<> options_;
    };
} // namespace lightdb::logical



#endif //LIGHTDB_METADATALIGHTFIELD_H
