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

    LightFieldReference ScanAndRetile(const std::string &name,
                                  const MetadataSpecification &metadataSpecification,
                                  unsigned int layoutDuration,
                                  CrackingStrategy crackingStrategy,
                                  RetileStrategy retileOnlyIfDifferent,
                                  std::shared_ptr<RegretAccumulator> regretAccumulator,
                                  std::shared_ptr<TileAroundMoreObjectsManager> tileAroundMoreObjectsManager) {

        // Transform metadataIdentifier.
        std::string metadataIdentifier;
        auto crackedPos = name.find("-cracked");
        if (crackedPos != std::string::npos) {
            metadataIdentifier = name.substr(0, crackedPos);
        } else {
            crackedPos = metadataIdentifier.find_last_of("-");
            metadataIdentifier = name.substr(0, crackedPos);
        }
        auto entry = std::make_shared<catalog::Entry>(Catalog::instance().entry(metadataIdentifier));

        auto metadataManager = std::make_shared<metadata::MetadataManager>(metadataIdentifier, MetadataSpecification(metadataSpecification, entry->sources()[0].mp4Reader().numberOfSamples()));

        auto lightField = Catalog::instance().getMultiTiledForRetiling(name);
        lightField.downcast<logical::MultiTiledLightFieldForRetiling>().setProperties(
                metadataManager, crackingStrategy, layoutDuration, entry, retileOnlyIfDifferent);

        if (regretAccumulator)
            lightField.downcast<logical::MultiTiledLightFieldForRetiling>().setRegretAccumulator(regretAccumulator);

        if (tileAroundMoreObjectsManager)
            lightField.downcast<logical::MultiTiledLightFieldForRetiling>().setTileAroundMoreObjectsManager(tileAroundMoreObjectsManager);

        return lightField;
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

    LightFieldReference Algebra::Select(const PixelsInFrameMetadataSpecification &pixelsInFrameMetadataSpecification, functor::UnaryFunctorReference functor) {
        auto lightField = Select(pixelsInFrameMetadataSpecification, MetadataSubsetTypePixelsInFrame);
        lightField.downcast<MetadataSubsetLightFieldWithoutSources>().setDetectionFunctor(functor);
        return lightField;
    }

    LightFieldReference Algebra::Select(const MetadataSpecification &metadataSpecification, MetadataSubsetType subsetType, bool shouldCrack, bool shouldReadEntireGOPs) {
        if (this_.is<ExternalLightField>())
            return LightFieldReference::make<MetadataSubsetLightField>(this_, metadataSpecification, subsetType, std::vector<catalog::Source>({ this_.downcast<ExternalLightField>().source() }), std::optional(this_.downcast<ExternalLightField>().source().filename().parent_path()));
        else if (this_.is<ScannedLightField>()) {
            auto &scan = this_.downcast<ScannedLightField>();
            auto metadataIdentifier = scan.entry().name();
            auto gopPos = metadataIdentifier.find("-gop");
            if (gopPos != std::string::npos)
                metadataIdentifier = metadataIdentifier.substr(0, gopPos);
            else {
                gopPos = metadataIdentifier.find("-customGOP");
                if (gopPos != std::string::npos)
                    metadataIdentifier = metadataIdentifier.substr(0, gopPos);
            }

            return LightFieldReference::make<MetadataSubsetLightField>(this_, metadataSpecification, subsetType,
                                                                       scan.sources(),
                                                                       std::optional(metadataIdentifier));
        } else if (this_.is<ScannedTiledLightField>()) {
            auto &scan = this_.downcast<ScannedTiledLightField>();
            return LightFieldReference::make<MetadataSubsetLightField>(this_, metadataSpecification, subsetType, scan.sources(), std::optional(scan.entry().name()));
        } else if (this_.is<ScannedMultiTiledLightField>()) {
            auto &scan = this_.downcast<ScannedMultiTiledLightField>();
            return LightFieldReference::make<MetadataSubsetLightFieldWithoutSources>(this_, metadataSpecification, subsetType, scan.tileLayoutsManager()->entry().name(), shouldCrack, shouldReadEntireGOPs);
        } else
            assert(false);
    }

    LightFieldReference Algebra::Select(std::shared_ptr<const FrameSpecification> frameSpecification) {
        return LightFieldReference::make<FrameSubsetLightField>(this_, frameSpecification);
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
                                              unsigned int layoutDuration,
                                              CrackingStrategy crackingStrategy,
                                              bool encodeTiles) {
        std::shared_ptr<metadata::MetadataManager> metadataManager_;
        if (metadataIdentifier.length()) {
            assert(metadataSpecification);
            metadataManager_ = std::make_shared<metadata::MetadataManager>(metadataIdentifier, *metadataSpecification);
        }

        return LightFieldReference::make<CrackedLightField>(this_, name, catalog::Catalog::instance(),
                std::nullopt,
                Codec::hevc(),
                metadataManager_,
                layoutDuration,
                crackingStrategy,
                encodeTiles);
    }

    LightFieldReference Algebra::StoreCrackedUniform(const std::string &name,
                                                  unsigned int uniformDimensionsCols,
                                                    unsigned int uniformDimensionsRows) {
        return LightFieldReference::make<CrackedLightField>(this_, name, catalog::Catalog::instance(),
                CrackingStrategy::Uniform,
                uniformDimensionsCols,
                uniformDimensionsRows);
    }

    LightFieldReference Algebra::StoreCrackedROI(const std::string &name, ROI roi) {
        return LightFieldReference::make<CrackedLightField>(this_, name, catalog::Catalog::instance(),
                CrackingStrategy::ROI,
                roi);
    }

    LightFieldReference Algebra::PrepareForCracking(const std::string &name, unsigned int layoutDuration) {
        return LightFieldReference::make<CrackedLightField>(this_, name, catalog::Catalog::instance(),
                std::nullopt,
                Codec::hevc(),
                nullptr,
                layoutDuration,
                CrackingStrategy::OneTile);
    }

    LightFieldReference Algebra::Sink() {
        return LightFieldReference::make<SunkLightField>(this_);
    }

    LightFieldReference Algebra::Predicate(std::string outName) {
        return LightFieldReference::make<PredicateLightField>(this_, outName);
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