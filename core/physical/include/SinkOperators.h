#ifndef LIGHTDB_SINKOPERATORS_H
#define LIGHTDB_SINKOPERATORS_H

#include "PhysicalOperators.h"

namespace lightdb::physical {

class Sink: public PhysicalOperator {
public:
    explicit Sink(const LightFieldReference &logical,
                   PhysicalOperatorReference &parent)
        : PhysicalOperator(logical, {parent}, DeviceType::CPU, runtime::make<Runtime>(*this, "Sink-init"))
    { }

private:
    class Runtime: public runtime::Runtime<> {
    public:
        explicit Runtime(PhysicalOperator &physical) : runtime::Runtime<>(physical) { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            if(!all_parent_eos()) {
                auto outputFile = "/home/maureen/noscope_videos/jackson_car_pixels.hevc";
                std::ofstream ofs(outputFile);
                std::for_each(iterators().begin(), iterators().end(), [&](auto &i) {
//                    i++;
                    MaterializedLightFieldReference data = i++;
                    auto &value = data.downcast<SerializableData>().value();
                    ofs.write(value.data(), value.size());
                });
                return EmptyData{physical().device()};
            } else
                return std::nullopt;
        }
    };
};

} // namespace lightdb::physical

#endif //LIGHTDB_SINKOPERATORS_H
