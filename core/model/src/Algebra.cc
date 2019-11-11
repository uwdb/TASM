#include "Algebra.h"
#include "MetadataLightField.h"
#include "Model.h"

#include <iostream>

using namespace lightdb::catalog;

namespace lightdb::logical {
    LightFieldReference ScanTiled(const std::string &name) {
        return Catalog::instance().getTiled(name);
    }

    LightFieldReference ScanMultiTiled(const std::string &name, bool usesOnlyOneTile) {
        return Catalog::instance().getMultiTiled(name, usesOnlyOneTile);
    }

    LightFieldReference Scan(const std::string &name) {
        return Scan(Catalog::instance(), name);
    }

    LightFieldReference ScanByGOP(const std::string &name) {
        return Catalog::instance().getByGOP(name);
    }

    LightFieldReference Scan(const catalog::Catalog &catalog, const std::string &name) {
        return catalog.get(name);
    }

    LightFieldReference Load(const std::filesystem::path &filename,
                             const lightdb::options<>& options) {
        auto entry = catalog::ExternalEntry(filename, options.get<Volume>(GeometryOptions::Volume),
                                                      options.get<GeometryReference>(GeometryOptions::Projection));
        return LightFieldReference::make<ExternalLightField>(filename, entry, YUVColorSpace::instance(), options);
    }

    LightFieldReference Load(const std::filesystem::path &filename, const Volume &volume,
                             const GeometryReference &geometry, const lightdb::options<>& options) {
        auto entry = catalog::ExternalEntry(filename, volume, geometry);
        return LightFieldReference::make<ExternalLightField>(filename, entry, YUVColorSpace::instance(), options);
    }

    LightFieldReference Algebra::Save(const std::filesystem::path &filename) {
        return LightFieldReference::make<SavedLightField>(this_, filename);
    }

    LightFieldReference Algebra::Select(const Volume &volume) {
        return LightFieldReference::make<SubsetLightField>(this_, volume);
    }

    LightFieldReference Algebra::Select(const SpatiotemporalDimension dimension, const SpatiotemporalRange &range) {
        Volume volume(this_->volume().bounding());
        volume.set(dimension, range);
        return LightFieldReference::make<SubsetLightField>(this_, volume);
    }

    LightFieldReference Algebra::Select(const ThetaRange &range) {
        Volume volume(this_->volume().bounding());
        volume.theta(range);
        return LightFieldReference::make<SubsetLightField>(this_, volume);
    }

    LightFieldReference Algebra::Select(const PhiRange &range) {
        Volume volume(this_->volume().bounding());
        volume.phi(range);
        return LightFieldReference::make<SubsetLightField>(this_, volume);
    }

    LightFieldReference Algebra::Select(const TemporalRange &range) {
        Volume volume(this_->volume().bounding());
        volume.t(range);
        return LightFieldReference::make<SubsetLightField>(this_, volume);
    }

    LightFieldReference Algebra::Select(const FrameMetadataSpecification &frameMetadataSpecification) {
        return Select(frameMetadataSpecification, MetadataSubsetTypeFrame);
    }

    LightFieldReference Algebra::Select(const PixelMetadataSpecification &pixelMetadataSpecification, bool shouldCrack, bool shouldReadEntireGOPs) {
        return Select(pixelMetadataSpecification, MetadataSubsetTypePixel, shouldCrack, shouldReadEntireGOPs);
    }

    LightFieldReference Algebra::Select(const PixelsInFrameMetadataSpecification &pixelsInFrameMetadataSpecification) {
        return Select(pixelsInFrameMetadataSpecification, MetadataSubsetTypePixelsInFrame);
    }

    LightFieldReference Algebra::Select(const MetadataSpecification &metadataSpecification, MetadataSubsetType subsetType, bool shouldCrack, bool shouldReadEntireGOPs) {
        if (this_.is<ExternalLightField>())
            return LightFieldReference::make<MetadataSubsetLightField>(this_, metadataSpecification, subsetType, std::vector<catalog::Source>({ this_.downcast<ExternalLightField>().source() }), std::optional(this_.downcast<ExternalLightField>().source().filename().parent_path()));
        else if (this_.is<ScannedLightField>()) {
            auto &scan = this_.downcast<ScannedLightField>();
            return LightFieldReference::make<MetadataSubsetLightField>(this_, metadataSpecification, subsetType,
                                                                       scan.sources(),
                                                                       std::optional(scan.entry().name()));
        } else if (this_.is<ScannedTiledLightField>()) {
            auto &scan = this_.downcast<ScannedTiledLightField>();
            return LightFieldReference::make<MetadataSubsetLightField>(this_, metadataSpecification, subsetType, scan.sources(), std::optional(scan.entry().name()));
        } else if (this_.is<ScannedMultiTiledLightField>()) {
            auto &scan = this_.downcast<ScannedMultiTiledLightField>();
            return LightFieldReference::make<MetadataSubsetLightFieldWithoutSources>(this_, metadataSpecification, subsetType, scan.tileLayoutsManager()->entry().name(), shouldCrack, shouldReadEntireGOPs);
        } else
            assert(false);
    }

    LightFieldReference Algebra::Store(const std::string &name, const Codec &codec,
                                       const std::optional<GeometryReference> &geometry) {
        return LightFieldReference::make<StoredLightField>(this_, name, catalog::Catalog::instance(), geometry, codec);
    }

    LightFieldReference Algebra::Store(const std::string &name, const catalog::Catalog &catalog,
                                       const Codec &codec, const std::optional<GeometryReference> &geometry) {
        return LightFieldReference::make<StoredLightField>(this_, name, catalog, geometry, codec);
    }

    LightFieldReference Algebra::StoreCracked(const std::string &name,
                                              const std::string &metadataIdentifier,
                                              const MetadataSpecification * const metadataSpecification,
                                              unsigned int layoutDuration) {
        std::shared_ptr<metadata::MetadataManager> metadataManager_;
        if (metadataIdentifier.length()) {
            assert(metadataSpecification);
            metadataManager_ = std::make_shared<metadata::MetadataManager>(metadataIdentifier, *metadataSpecification);
        }

        return LightFieldReference::make<CrackedLightField>(this_, name, catalog::Catalog::instance(), std::nullopt, Codec::hevc(), metadataManager_, layoutDuration);
    }

    LightFieldReference Algebra::Sink() {
        return LightFieldReference::make<SunkLightField>(this_);
    }

    LightFieldReference Algebra::Encode(const Codec &codec, const options<> &options) {
        return LightFieldReference::make<logical::EncodedLightField>(
                this_, codec, this_->volume().bounding(), this_->colorSpace(), options);
    }

    LightFieldReference Algebra::Encode(const MetadataSpecification &metadataSpecification) {
//        assert(this_.is<ScannedLightField>());
        if (this_.is<ScannedLightField>()) {
            return LightFieldReference::make<logical::MetadataEncodedLightField>(
                    this_, Codec::hevc(), this_->volume().bounding(), this_->colorSpace(), this_.downcast<ScannedLightField>().sources().front(), metadataSpecification);
        } else if (this_.is<ExternalLightField>()) {
            return LightFieldReference::make<logical::MetadataEncodedLightField>(
                    this_, Codec::hevc(), this_->volume().bounding(), this_->colorSpace(), this_.downcast<ExternalLightField>().source(), metadataSpecification);
        } else
            assert(false);

    }

    LightFieldReference Algebra::Union(const LightFieldReference other) {
        return LightFieldReference::make<logical::CompositeLightField>(std::vector<LightFieldReference>{this_, other});
    }

    LightFieldReference Algebra::Union(std::vector<LightFieldReference> fields) {
        fields.push_back(this_);
        return LightFieldReference::make<logical::CompositeLightField>(fields);
    }

    LightFieldReference Algebra::Rotate(const angle theta, const angle phi) {
        return LightFieldReference::make<logical::RotatedLightField>(this_, theta, phi);
    }

    LightFieldReference Algebra::Partition(const Dimension dimension, const number& interval) {
        return LightFieldReference::make<logical::PartitionedLightField>(this_, dimension, interval);

    }

    LightFieldReference Algebra::Interpolate(const Dimension dimension,
                                             interpolation::InterpolatorReference interpolator) {
        return LightFieldReference::make<logical::InterpolatedLightField>(this_, dimension, interpolator);
    }

    LightFieldReference Algebra::Discretize(const GeometryReference& geometry) {
        return LightFieldReference::make<logical::DiscretizedLightField>(this_, geometry);
    }

    LightFieldReference Algebra::Discretize(const Dimension dimension, const number& interval) {
        return Discretize(GeometryReference::make<IntervalGeometry>(dimension, interval));
    }

    LightFieldReference Algebra::Map(functor::UnaryFunctorReference functor) {
        return LightFieldReference::make<logical::TransformedLightField>(this_, functor);
    }

    LightFieldReference Algebra::Subquery(const std::function<LightFieldReference(LightFieldReference)> &subquery) {
        return LightFieldReference::make<logical::SubqueriedLightField>(this_, subquery);
    }

} // namespace lightdb::logical