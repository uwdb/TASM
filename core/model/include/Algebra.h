#ifndef LIGHTDB_ALGEBRA_H
#define LIGHTDB_ALGEBRA_H

#include "LightField.h"
#include "Catalog.h"
#include "Geometry.h"
#include "Interpolation.h"
#include "Metadata.h"
#include "options.h"
#include <filesystem>

namespace lightdb {
    namespace functor {
        template<std::size_t n> class naryfunctor;
        template<std::size_t n> using FunctorReference = shared_reference<naryfunctor<n>>;
        using UnaryFunctorReference = FunctorReference<1>;
    }

    namespace logical {
        class RegretAccumulator;
        class TileAroundMoreObjectsManager;

        LightFieldReference ScanTiled(const std::string &name);
        LightFieldReference ScanMultiTiled(const std::string &name, bool usesOnlyOneTile = false);

        enum class RetileStrategy {
            RetileAlways,
            RetileIfDifferent,
            RetileBasedOnRegret,
            RetileAroundMoreObjects,
        };

        LightFieldReference ScanAndRetile(const std::string &name,
                                          const MetadataSpecification &metadataSpecification,
                                          unsigned int layoutDuration,
                                          CrackingStrategy crackingStrategy,
                                          RetileStrategy retileOnlyIfDifferent = RetileStrategy::RetileAlways,
                                          std::shared_ptr<RegretAccumulator> regretAccumulator = {},
                                          std::shared_ptr<TileAroundMoreObjectsManager> tileAroundMoreObjectsManager = {},
                                          const lightdb::options<>& = {});
    LightFieldReference ScanAndRetile(const std::string &name,
                                      const std::string &originalVideoName,
                                      const MetadataSpecification &metadataSpecification,
                                      unsigned int layoutDuration,
                                      CrackingStrategy crackingStrategy,
                                      RetileStrategy retileOnlyIfDifferent = RetileStrategy::RetileAlways,
                                      std::shared_ptr<RegretAccumulator> regretAccumulator = {},
                                      std::shared_ptr<TileAroundMoreObjectsManager> tileAroundMoreObjectsManager = {},
                                      const lightdb::options<>& = {});

        LightFieldReference ScanByGOP(const std::string &name);
        LightFieldReference Scan(const std::string &name);
        LightFieldReference Scan(const catalog::Catalog&, const std::string &name);
        LightFieldReference Load(const std::filesystem::path&, const lightdb::options<>& = {});
        LightFieldReference Load(const std::filesystem::path&, const Volume&, const GeometryReference&,
                                 const lightdb::options<>& = {});

        LightFieldReference CreateBlackTile(Codec codec, unsigned int width, unsigned int height, unsigned int numFrames);

        class Algebra: public DefaultMixin {
        public:
            LightFieldReference Select(const Volume&);
            LightFieldReference Select(SpatiotemporalDimension, const SpatiotemporalRange&);
            LightFieldReference Select(const ThetaRange&);
            LightFieldReference Select(const PhiRange&);
            LightFieldReference Select(const TemporalRange&);

            // Metadata select.
            // Specify table name, column name, and column value.
            // For now just support equality.
            // Can create a class, like the ranges, to enable specifying all this.
            LightFieldReference Select(const FrameMetadataSpecification&, const lightdb::options<>& = {});
            LightFieldReference Select(const PixelMetadataSpecification&, bool shouldCrack=false, bool shouldReadEntireGOPs=false, const lightdb::options<>& = {});
            LightFieldReference Select(const PixelsInFrameMetadataSpecification&, const lightdb::options<>& = {});
            LightFieldReference Select(const PixelsInFrameMetadataSpecification&, functor::UnaryFunctorReference, const lightdb::options<>& = {});
            LightFieldReference Select(const MetadataSpecification&, MetadataSubsetType subsetType = MetadataSubsetTypeFrame, bool shouldCrack = false, bool shouldReadEntireGOPs=false, const lightdb::options<>& = {});
            LightFieldReference Select(std::shared_ptr<const FrameSpecification> frameSpecification);


            LightFieldReference Union(std::vector<LightFieldReference>); //TODO needs merge function
            LightFieldReference Union(LightFieldReference); //TODO needs merge function
            LightFieldReference Rotate(angle theta, angle phi);
            LightFieldReference Partition(Dimension, const number&);
            LightFieldReference Interpolate(Dimension, interpolation::InterpolatorReference);
            LightFieldReference Discretize(const GeometryReference&);
            LightFieldReference Discretize(Dimension, const number&);
            LightFieldReference Map(functor::UnaryFunctorReference);
            LightFieldReference Subquery(const std::function<LightFieldReference(LightFieldReference)>&);

            LightFieldReference Encode(const Codec& = Codec::hevc(), const lightdb::options<>& = {});
            LightFieldReference Encode(const MetadataSpecification &metadataSpecification);

            LightFieldReference Store(const std::string &name, const Codec &codec=Codec::hevc(),
                                      const std::optional<GeometryReference> &geometry={});
            LightFieldReference Store(const std::string &name, const catalog::Catalog&,
                                      const Codec &codec=Codec::hevc(),
                                      const std::optional<GeometryReference> &geometry={});

            LightFieldReference StoreCracked(const std::string &name,
                                                const std::string &metadataIdentifier="",
                                                const MetadataSpecification * const metadataSpecification=nullptr,
                                                unsigned int layoutDuration=0,
                                                CrackingStrategy crackingStrategy=CrackingStrategy::None,
                                                bool encodeTiles=true);
            LightFieldReference StoreCrackedUniform(const std::string &name,
                                                unsigned int uniformDimensionsCols,
                                                unsigned int uniformDimensionsRows);
            LightFieldReference StoreCrackedROI(const std::string &name, ROI roi);
            LightFieldReference PrepareForCracking(const std::string &name, unsigned int layoutDuration = 0, const lightdb::options<>& = {});

            LightFieldReference Save(const std::filesystem::path&);
            LightFieldReference Sink();
            LightFieldReference Predicate(std::string outName="");

            Algebra& operator=(Algebra&& other) noexcept { return *this; }

        protected:
            explicit Algebra(const LightFieldReference &lightField)
                    : DefaultMixin(lightField), this_(lightField)
            { }

        private:
            const LightFieldReference &this_;
        };
    } // namespace logical
} // namespace lightdb

#endif //LIGHTDB_ALGEBRA_H
