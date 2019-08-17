#ifndef LIGHTDB_STOREOPERATORS_H
#define LIGHTDB_STOREOPERATORS_H

#include "PhysicalOperators.h"
#include "Transaction.h"

namespace lightdb::physical {

class StoreOutOfOrderData: public PhysicalOperator {
public:
    // TODO: INITIALIZE WITH FRAMES THAT IT IS EXPECTING.
    explicit StoreOutOfOrderData(const LightFieldReference &logical,
            const std::vector<PhysicalOperatorReference> &parents,
            const std::vector<int> &framesToStore)
            : StoreOutOfOrderData(logical,
                    logical->downcast<logical::StoredLightField>(),
                    parents,
                    framesToStore)
    { }

    const Codec &codec() const { return codec_; }
    const std::vector<int> &framesToStore() const { return framesToStore_; }

private:
    explicit StoreOutOfOrderData(const LightFieldReference &logical,
            const logical::StoredLightField &store,
            const std::vector<PhysicalOperatorReference> &parents,
            const std::vector<int> &framesToStore)
            : PhysicalOperator(logical, parents, DeviceType::CPU, runtime::make<Runtime>(*this, "StoreOutOfOrderData-init", store)),
            codec_(store.codec()),
            framesToStore_(framesToStore) {
        CHECK_GT(parents.size(), 0);
    }


    class Runtime: public runtime::Runtime<StoreOutOfOrderData, FrameData> {
    public:
        Runtime(StoreOutOfOrderData &physical, const logical::StoredLightField &logical)
                : runtime::Runtime<StoreOutOfOrderData, FrameData>(physical),
                  outputs_{functional::transform<std::reference_wrapper<transactions::OutputStream>>(
                          physical.parents().begin(), physical.parents().begin()+1,
                          [this, &logical](auto &parent) {
                              return std::ref(
                                      this->physical().context()->transaction().write(
                                              logical,
                                              this->geometry(parent))); })},
                  nextFrameToSave_(physical.framesToStore().begin())
        { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            if(!all_parent_eos()) {
                GLOBAL_TIMER.startSection("StoreOutOfOrderData");

                auto &output = outputs_.front().get();
                for (auto i = 0u; i < iterators().size(); i++) {
                    auto &iterator = iterators().at(i);
                    if (iterator == iterator.eos())
                        continue;

                    auto data = (*iterator++).downcast<CPUEncodedFrameData>();
                    int firstFrameIndex = -1;
                    int numberOfFrames = -1;
                    assert(data.getFirstFrameIndexIfSet(firstFrameIndex));
                    assert(data.getNumberOfFramesIfSet(numberOfFrames));

                    // See if the first frame is the next frame we are looking for.
                    if (firstFrameIndex == *nextFrameToSave_) {
                        std::move(data.value().begin(), data.value().end(), std::ostreambuf_iterator<char>(output.stream()));
                        std::advance(nextFrameToSave_, numberOfFrames);
                    } else if (numberOfFrames) {
                        startingFrameToData_.emplace(std::piecewise_construct,
                                std::forward_as_tuple(firstFrameIndex),
                                std::forward_as_tuple(numberOfFrames, data.value()));
                    }
                }

                // See if we have the next set of frames saved.
                auto nextFrameSequence = startingFrameToData_.end();
                while (haveMoreFramesToSave() && (nextFrameSequence = startingFrameToData_.find(*nextFrameToSave_)) != startingFrameToData_.end()) {
                    std::move(nextFrameSequence->second.value.begin(), nextFrameSequence->second.value.end(), std::ostreambuf_iterator<char>(output.stream()));
                    std::advance(nextFrameToSave_, nextFrameSequence->second.numberOfFrames);

                    startingFrameToData_.erase(nextFrameSequence->first);
                }

                GLOBAL_TIMER.endSection("StoreOutOfOrderData");
                return EmptyData(DeviceType::CPU);
            } else {
                assert(!haveMoreFramesToSave());
                return std::nullopt;
            }
        }
    private:
        struct FrameDataInfo {
            FrameDataInfo(int numberOfFrames, lightdb::bytestring value)
                : numberOfFrames(numberOfFrames),
                value(std::move(value))
            { }

            int numberOfFrames;
            lightdb::bytestring value;
        };

        inline bool haveMoreFramesToSave() const {
            return nextFrameToSave_ != physical().framesToStore().end();
        }

        std::vector<std::reference_wrapper<transactions::OutputStream>> outputs_;
        std::vector<int>::const_iterator nextFrameToSave_;
        std::unordered_map<int, FrameDataInfo> startingFrameToData_;
    };
    const Codec codec_;
    const std::vector<int> framesToStore_;
};

class Store: public PhysicalOperator {
public:
    explicit Store(const LightFieldReference &logical,
                   PhysicalOperatorReference &parent)
            : Store(logical,
                    std::vector<PhysicalOperatorReference>{parent})
    { }

    explicit Store(const LightFieldReference &logical,
                   const std::vector<PhysicalOperatorReference> &parents)
            : Store(logical,
                    logical->downcast<logical::StoredLightField>(),
                    parents)
    { }

    const Codec &codec() const { return codec_; }

private:
    explicit Store(const LightFieldReference &logical,
                   const logical::StoredLightField &store,
                   const std::vector<PhysicalOperatorReference> &parents)
            : PhysicalOperator(logical, parents, DeviceType::CPU, runtime::make<Runtime>(*this, "Store-init", store)),
              codec_(store.codec()) {
        CHECK_GT(parents.size(), 0);
    }

    class Runtime: public runtime::Runtime<Store, FrameData> {
    public:
        Runtime(Store &physical, const logical::StoredLightField &logical)
                : runtime::Runtime<Store, FrameData>(physical),
                  outputs_{functional::transform<std::reference_wrapper<transactions::OutputStream>>(
                          physical.parents().begin(), physical.parents().end(),
                          [this, &logical](auto &parent) {
                              return std::ref(
                                  this->physical().context()->transaction().write(
                                      logical,
                                      this->geometry(parent))); })}
        { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            if(!all_parent_eos()) {
                GLOBAL_TIMER.startSection("Store");
                for(auto i = 0u; i < iterators().size(); i++) {
                    auto input = iterators().at(i)++;
                    auto &data = input.downcast<SerializableData>();
                    auto &output = outputs_.at(i).get();

                    std::copy(data.value().begin(), data.value().end(), std::ostreambuf_iterator<char>(output.stream()));
                }
                GLOBAL_TIMER.endSection("Store");
                return EmptyData(DeviceType::CPU);
            } else
                return std::nullopt;
        }

    private:
        std::vector<std::reference_wrapper<transactions::OutputStream>> outputs_;
    };

    const Codec codec_;
};

} // namespace lightdb::physical

#endif //LIGHTDB_STOREOPERATORS_H
