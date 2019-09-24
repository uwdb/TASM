#ifndef LIGHTDB_SHAREDDECODEOPERATORS_H
#define LIGHTDB_SHAREDDECODEOPERATORS_H

#include "DecodeSessionManager.h"
#include "PhysicalOperators.h"

namespace lightdb::physical {

class SharedDecode : public PhysicalOperator, public GPUOperator {
public:
    explicit SharedDecode(const LightFieldReference &logical,
                            std::vector<PhysicalOperatorReference> &parents,
                            const execution::GPU &gpu)
            : PhysicalOperator(logical, parents, DeviceType::GPU, runtime::make<Runtime>(*this, "SharedDecode-init")),
            GPUOperator(gpu)
    { }

private:
    class Runtime : public runtime::GPURuntime<SharedDecode> {
    public:
        explicit Runtime(SharedDecode &physical)
            : runtime::GPURuntime<SharedDecode>(physical),
              configuration_(getConfiguration()),
              decodeSessionManager_(4, configuration_, lock())
        { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            // If all parents are done, and all data is decoded, then return nullopt.
            if (all_parent_eos() && decodeSessionManager_.allDecodersAreDone())
                return {};

            std::vector<int> completeTiles;

            // Else read all of the decoded data from the decode manager.
            auto decoded = decodeSessionManager_.read();

            // For every iterator that isn't done, send it's data to be decoded.
            // TODO: This should be done on a separate thread, not in the main pipeline.
            for (auto index = 0u; index < iterators().size(); ++index) {
                if (iterators()[index] != iterators()[index].eos()) {
                    decodeSessionManager_.decodeData(index, iterators()[index]++);
                } else if (!decodeSessionManager_.anyDecoderIsIncompleteForTile(index))
                    completeTiles.push_back(index);
            }

            // Transform decoded into GPUDecodedFrameData that includes tile number.
            // Each frame will have to be responsible for freeing itself.
            // Return a map from tile -> GPUDecodedFrameData.
            std::unique_ptr<std::unordered_map<unsigned int, GPUDecodedFrameData>> tileToData(new std::unordered_map<unsigned int, GPUDecodedFrameData>());
            for (auto it = decoded.begin(); it != decoded.end(); ++it) {
                tileToData->emplace(std::pair<unsigned int, GPUDecodedFrameData>(
                        std::piecewise_construct,
                        std::forward_as_tuple(it->first),
                        std::forward_as_tuple(static_cast<Configuration>(configuration_), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()))));

                auto &decodedFrameData = tileToData->at(it->first);

                // Create a decoded frame for each decoded info and put it into frames.
                for (auto &decodedInfo : it->second) {
                    decodedFrameData.frames().push_back(GPUFrameReference::make<DecodedFrame>(decodedInfo));
                }
            }

            return MultiGPUDecodedFrameData(std::move(tileToData), completeTiles);
        }

    private:
        DecodeConfiguration getConfiguration() {
            for (auto &iterator : iterators()) {
                if (iterator != iterator.eos()) {
                    return DecodeConfiguration((*iterator).downcast<CPUEncodedFrameData>().configuration(),
                                                (*iterator).downcast<CPUEncodedFrameData>().codec());
                }
            }
            // One of the sources should not be empty.
            assert(false);
        }

        DecodeConfiguration configuration_;
        DecodeSessionManager decodeSessionManager_;
    };

};

} // namespace lightdb::physical

#endif //LIGHTDB_SHAREDDECODEOPERATORS_H
