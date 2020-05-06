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
#include <mutex>

namespace lightdb::tiles {
class TileConfigurationProvider;
}

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
        explicit MetadataManager(const std::string &pathToVideo, const MetadataSpecification &metadataSpecification)
                : videoIdentifier_(pathToVideo),
                metadataSpecification_(metadataSpecification),
                didSetFramesForMetadata_(false),
                framesForMetadata_(0),
                didSetOrderedFramesForMetadata_(false),
                orderedFramesForMetadata_(0),
                didSetIdealKeyframesForMetadata_(false),
                idealKeyframesForMetadata_(0),
                didSetFramesForMetadataOrderedByNumObjects_(false),
                framesForMetadataOrderedByNumObjects_(0)
        {
            openDatabase();
        }

        ~MetadataManager() {
            closeDatabase();
        }

        const std::string &metadataIdentifier() const { return videoIdentifier_; }
        const MetadataSpecification &metadataSpecification() const { return metadataSpecification_; }

        const std::unordered_set<int> &framesForMetadata() const;
        const std::vector<int> &orderedFramesForMetadata() const;
        const std::unordered_set<int> &idealKeyframesForMetadata() const;
        const std::vector<int> &framesForMetadataOrderedByNumObjects() const;

        const std::vector<Rectangle> &rectanglesForFrame(int frame) const;
        std::unique_ptr<std::list<Rectangle>> rectanglesForFrames(int firstFrameInclusive, int lastFrameExclusive) const;
        std::unique_ptr<std::list<Rectangle>> rectanglesForAllObjectsForFrames(int firstFrameInclusive, int lastFrameExclusive) const;

        static std::unordered_set<int> idealKeyframesForFrames(const std::vector<int> &orderedFrames);

        std::unordered_set<int> idealKeyframesForMetadataAndTiles(const tiles::TileLayout &tileLayout) const;
        std::vector<int> framesForTileAndMetadata(unsigned int tile, const tiles::TileLayout &tileLayout) const;

    private:
        void selectFromMetadataAndApplyFunction(const char* query, std::function<void(sqlite3_stmt*)> resultFn, std::function<void(sqlite3*)> afterOpeningFn = nullptr) const;
        void selectFromMetadataWithoutQuerySubstitutionsAndApplyFunction(const char* query, unsigned int querySize, std::function<void(sqlite3_stmt*)> resultFn, std::function<void(sqlite3*)> afterOpeningFn = nullptr) const;
        void selectFromMetadataAndApplyFunctionWithFrameLimits(const char* query, std::function<void(sqlite3_stmt*)> resultFn, std::function<void(sqlite3*)> afterOpeningFn = nullptr) const;
        void openDatabase();
        void closeDatabase();

        const std::string videoIdentifier_;
        const MetadataSpecification metadataSpecification_;
        mutable bool didSetFramesForMetadata_;
        mutable std::unordered_set<int> framesForMetadata_;
        mutable bool didSetOrderedFramesForMetadata_;
        mutable std::vector<int> orderedFramesForMetadata_;
        mutable bool didSetIdealKeyframesForMetadata_;
        mutable std::unordered_set<int> idealKeyframesForMetadata_;
        mutable std::unordered_map<int, std::vector<lightdb::Rectangle>> frameToRectangles_;
        sqlite3 *db_;
        mutable std::mutex mutex_;

        mutable bool didSetFramesForMetadataOrderedByNumObjects_;
        mutable std::vector<int> framesForMetadataOrderedByNumObjects_;
    };
} // namespace lightdb::metadata

namespace lightdb::logical {
    class MetadataSubsetLightFieldWithoutSources : public LightField {
    public:
        MetadataSubsetLightFieldWithoutSources(const LightFieldReference &lightField,
                                                const MetadataSpecification &metadataSpecification,
                                                MetadataSubsetType subsetType,
                                                std::string metadataIdentifier,
                                                bool shouldCrack = false,
                                                bool shouldReadEntireGOPs = false)
                : LightField(lightField),
                metadataSpecification_(metadataSpecification),
                subsetType_(subsetType),
//                metadataManager_(std::make_shared<metadata::MetadataManager>(metadataIdentifier, metadataSpecification_)),
                shouldCrack_(shouldCrack),
                shouldReadEntireGOPs_(shouldReadEntireGOPs)
        {
            // Transform metadataIdentifier.
            auto crackedPos = metadataIdentifier.find("-cracked");
            if (crackedPos != std::string::npos) {
                metadataIdentifier = metadataIdentifier.substr(0, crackedPos);
            } else {
                crackedPos = metadataIdentifier.find_last_of("-");
                metadataIdentifier = metadataIdentifier.substr(0, crackedPos);
            }
            metadataManager_ = std::make_shared<metadata::MetadataManager>(metadataIdentifier, metadataSpecification_);

        }

        void accept(LightFieldVisitor &visitor) override { LightField::accept<MetadataSubsetLightFieldWithoutSources>(visitor); }

        std::shared_ptr<const metadata::MetadataManager> metadataManager() const { return metadataManager_; }
        bool shouldCrack() const { return shouldCrack_; }
        bool shouldReadEntireGOPs() const { return shouldReadEntireGOPs_; }

    private:
        const MetadataSpecification metadataSpecification_;
        const MetadataSubsetType subsetType_;
        std::shared_ptr<const metadata::MetadataManager> metadataManager_;
        bool shouldCrack_;
        bool shouldReadEntireGOPs_;
    };

    class MetadataSubsetLightField : public LightField {
    public:
        explicit MetadataSubsetLightField(const LightFieldReference &lightField, const MetadataSpecification &metadataSpecification, MetadataSubsetType subsetType, const catalog::Source &source)
            : MetadataSubsetLightField(lightField, metadataSpecification, subsetType, std::vector<catalog::Source>({ source }))
        { }

        explicit MetadataSubsetLightField(const LightFieldReference &lightField,
                const MetadataSpecification &metadataSpecification,
                MetadataSubsetType subsetType,
                const std::vector<catalog::Source> &sources,
                std::optional<std::string> metadataIdentifier = {})
        : LightField(lightField),
        metadataSelection_(metadataSpecification, sources[0].mp4Reader().numberOfSamples()),
        subsetType_(subsetType),
        sources_(sources),
        metadataManager_(metadataIdentifier ? *metadataIdentifier : source().filename().string(), metadataSelection_)
        {
            // TODO: Open database here.
            // Close on destruction.
        }

        void accept(LightFieldVisitor &visitor) override { LightField::accept<MetadataSubsetLightField>(visitor); }
        const MetadataSpecification &metadataSpecification() const { return metadataSelection_; }
        MetadataSubsetType subsetType() const { return subsetType_; }
        const catalog::Source &source() const {
            assert(sources_.size() == 1);
            return sources_[0];
        }

        const metadata::MetadataManager &metadataManager() const { return metadataManager_; }

        // Should there be different classes of metadata selection? e.g. frames, pixels?
        const std::unordered_set<int> &framesForMetadata() const { return metadataManager_.framesForMetadata(); }
        const std::vector<int> &orderedFramesForMetadata() const { return metadataManager_.orderedFramesForMetadata(); }
        std::pair<std::vector<int>, std::vector<int>> sequentialFramesAndNonSequentialFrames() const {
            return source().mp4Reader().frameSequencesInSequentialGOPsAndNonSequentialGOPs(orderedFramesForMetadata());
        }

        const std::vector<Rectangle> &rectanglesForFrame(int frame) const { return metadataManager_.rectanglesForFrame(frame); }

        std::vector<int> framesForTileAndMetadata(unsigned int tileNumber, const tiles::TileLayout &tileLayout) const {
            return metadataManager_.framesForTileAndMetadata(tileNumber, tileLayout);
        }
    private:
        MetadataSpecification metadataSelection_;
        MetadataSubsetType subsetType_;
        std::vector<catalog::Source> sources_;
        metadata::MetadataManager metadataManager_;
    };

    class FrameSubsetLightField : public LightField {
    public:
        explicit FrameSubsetLightField(const LightFieldReference &lightField, std::shared_ptr<const FrameSpecification> frameSpecification)
            : LightField(lightField), frameSpecification_(frameSpecification)
        {}

        void accept(LightFieldVisitor &visitor) override { LightField::accept<FrameSubsetLightField>(visitor); }
        std::shared_ptr<const FrameSpecification> frameSpecification() const { return frameSpecification_; }

    private:
        std::shared_ptr<const FrameSpecification> frameSpecification_;
    };

class RegretAccumulator {
public:
    virtual void addRegretToGOP(unsigned int gop, double regret, const std::string &layoutIdentifier) = 0;
    virtual bool shouldRetileGOP(unsigned int gop, std::string &layoutIdentifier) = 0;
    virtual void resetRegretForGOP(unsigned int gop) = 0;
    virtual std::shared_ptr<tiles::TileConfigurationProvider> configurationProviderForIdentifier(const std::string &identifier) = 0;
    virtual const std::vector<std::string> layoutIdentifiers() = 0;
    virtual ~RegretAccumulator() {}
};

} // namespace lightdb::logical



#endif //LIGHTDB_METADATALIGHTFIELD_H
