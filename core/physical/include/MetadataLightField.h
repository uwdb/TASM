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

namespace lightdb::logical {
    class MetadataSubsetLightField : public LightField {
    public:
        MetadataSubsetLightField(const LightFieldReference &lightField, const MetadataSpecification &metadataSpecification, const catalog::Source &source)
        : LightField(lightField),
        metadataSelection_(metadataSpecification),
        source_(source)
        { }

        void accept(LightFieldVisitor &visitor) override { LightField::accept<MetadataSubsetLightField>(visitor); }
        const MetadataSpecification &metadataSpecification() const { return metadataSelection_; }
        const catalog::Source &source() const { return source_; }

    private:
        MetadataSpecification metadataSelection_;
        catalog::Source source_;
    };
} // namespace lightdb::logical

namespace lightdb::metadata {
    class MetadataManager {
    public:
        explicit MetadataManager(const std::filesystem::path &pathToMetadata)
                : pathToMetadata_(pathToMetadata)
        {}

        std::unordered_set<int> framesForMetadata(const MetadataSpecification&) const;
    private:
        const std::filesystem::path pathToMetadata_;
    };
} // namespace lightdb::metadata


#endif //LIGHTDB_METADATALIGHTFIELD_H
