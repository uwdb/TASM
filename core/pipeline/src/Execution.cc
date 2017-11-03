#include "Execution.h"
#include "Physical.h"
#include "Display.h"

namespace visualcloud {
    namespace pipeline {
        template<typename ColorSpace>
        static std::optional<EncodedLightField> applyTiling(LightFieldReference <ColorSpace> lightfield, const std::string &format) {
            try {
                return visualcloud::physical::EquirectangularTiledLightField<ColorSpace>(lightfield).apply(format);
            } catch(std::invalid_argument) {
                return {};
            }
        }

        template<typename ColorSpace>
        static std::optional<EncodedLightField> applyStitching(LightFieldReference<ColorSpace> lightfield, visualcloud::rational temporalInterval=0) {
            try {
                return visualcloud::physical::StitchedLightField<ColorSpace>(lightfield).apply(temporalInterval);
            } catch(std::invalid_argument) {
                //TODO just name these things, rather than relying on dynamic casting
                auto *partitioning = dynamic_cast<const PartitionedLightField<ColorSpace>*>(&*lightfield);
                auto *discrete = dynamic_cast<const DiscretizedLightField<ColorSpace>*>(&*lightfield);
                auto *interpolated = dynamic_cast<const InterpolatedLightField<ColorSpace>*>(&*lightfield);

                if(partitioning != nullptr && partitioning->dimension() == Dimension::Time && lightfield->provenance().size() == 1)
                    return applyStitching(lightfield->provenance().at(0), partitioning->interval());
                //TODO this is mega broken; find the composite
                else if(discrete != nullptr || interpolated != nullptr)
                    return applyStitching(lightfield->provenance().at(0), temporalInterval);
                else
                    return {};
            }
        }

        template<typename ColorSpace>
        static std::optional<EncodedLightField> applyIdentityTranscode(LightFieldReference<ColorSpace> lightfield, const std::string &format) {
            auto *video = dynamic_cast<PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>*>(&*lightfield);
            if(video != nullptr && video->metadata().codec == format) {
                auto sp = static_cast<const std::shared_ptr<LightField<ColorSpace>>>(lightfield);
                auto vlf = std::static_pointer_cast<PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>>(sp);
                auto elf = std::static_pointer_cast<EncodedLightFieldData>(vlf);

                return elf;
            }
            else
                return {};
        }

        template<typename ColorSpace>
        static std::optional<EncodedLightField> applyTransformTranscode(LightFieldReference<ColorSpace> lightfield, const std::string &format) {
            if(lightfield->provenance().size() != 1)
                return {};

            auto *transform = dynamic_cast<TransformedLightField<ColorSpace>*>(&*lightfield);
            auto *video = dynamic_cast<PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>*>(&*lightfield->provenance()[0]);
            if(transform != nullptr && video != nullptr &&
               transform->functor().hasFrameTransform())
                return visualcloud::physical::EquirectangularTranscodedLightField<ColorSpace>(*video, transform->functor()).apply(format);
            else
                return {};
        }

        template<typename ColorSpace>
        static std::optional<EncodedLightField> applySelection(LightFieldReference<ColorSpace> lightfield, const std::string &format) {
            if(lightfield->provenance().size() != 1)
                return {};

            auto *subset = dynamic_cast<SubsetLightField<ColorSpace>*>(&*lightfield);
            auto *video = dynamic_cast<PanoramicVideoLightField<EquirectangularGeometry, ColorSpace>*>(&*lightfield->provenance()[0]);
            if(subset != nullptr && video != nullptr &&
                    subset->volumes().size() == 1 &&
                    subset->volumes()[0].x.Empty() &&
                    subset->volumes()[0].y.Empty() &&
                    subset->volumes()[0].z.Empty()) {
                auto elf = visualcloud::physical::EquirectangularCroppedLightField<ColorSpace>(*video, subset->volumes()[0].theta, subset->volumes()[0].phi).apply(format);
                return elf;
            }
            else
                return {};
        }

        template<typename ColorSpace>
        EncodedLightField execute(LightFieldReference <ColorSpace> lightfield, const std::string &format) {
            //print_plan(lightfield);

            //auto volumes = lightfield->volumes();
            //auto vl = volumes.size();

            std::optional<EncodedLightField> result;
            if((result = applyIdentityTranscode(lightfield, format)).has_value())
                return result.value();
            else if((result = applyTiling(lightfield, format)).has_value())
                return result.value();
            else if((result = applyStitching(lightfield)).has_value())
                return result.value();
            else if((result = applySelection(lightfield, format)).has_value())
                return result.value();
            else if((result = applyTransformTranscode(lightfield, format)).has_value())
                return result.value();
            else
                throw std::runtime_error("Unable to execute query");

            //visualcloud::physical::EquirectangularTiledLightField<ColorSpace> foo(lightfield);
            //return foo.apply(format);
        }

        template EncodedLightField execute<YUVColorSpace>(const LightFieldReference <YUVColorSpace>, const std::string&);
    } // namespace pipeline
} // namespace visualcloud
