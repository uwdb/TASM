#ifndef LIGHTDB_ENCODEOPERATORS_H
#define LIGHTDB_ENCODEOPERATORS_H

#include "LightField.h"
#include "PhysicalOperators.h"
#include "Codec.h"
#include "Configuration.h"
#include "VideoEncoder.h"
#include "EncodeWriter.h"
#include "VideoEncoderSession.h"
#include "MaterializedLightField.h"
#include <cstdio>
#include <utility>

#include "timer.h"

namespace lightdb::physical {

class GPUEncodeToCPU : public PhysicalOperator, public GPUOperator {
public:
    static constexpr size_t kDefaultGopSize = 30;

    explicit GPUEncodeToCPU(const LightFieldReference &logical,
                            PhysicalOperatorReference &parent,
                            Codec codec)
            : PhysicalOperator(logical, {parent}, DeviceType::GPU, runtime::make<Runtime>(*this, "GPUEncodeToCPU-init")),
              GPUOperator(parent),
              codec_(std::move(codec)) {
        if(!codec.nvidiaId().has_value())
            throw GpuRuntimeError("Requested codec does not have an Nvidia encode id");
    }

    const Codec &codec() const { return codec_; }

private:
    class Runtime: public runtime::GPUUnaryRuntime<GPUEncodeToCPU, GPUDecodedFrameData> {
    public:
        explicit Runtime(GPUEncodeToCPU &physical)
            : runtime::GPUUnaryRuntime<GPUEncodeToCPU, GPUDecodedFrameData>(physical),
              encodeConfiguration_{configuration(), this->physical().codec().nvidiaId().value(), gop()},
              encoder_{this->context(), encodeConfiguration_, lock()},
              writer_{encoder_.api()},
              encodeSession_{encoder_, writer_},
              numberOfEncodedFrames_(0)
        {
//            timer_.endSection();
        }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            GLOBAL_TIMER.startSection("GPUEncodeToCPU");
            if (iterator() != iterator().eos()) {
                auto decoded = iterator()++;

                timer_.startSection("inside-GPUEncodeToCPU");
                for (const auto &frame: decoded.frames()) {
                    ++numberOfEncodedFrames_;
                    encodeSession_.Encode(*frame, decoded.configuration().offset.top,
                                          decoded.configuration().offset.left);
                }

                //TODO think this should move down just above nullopt
                // Did we just reach the end of the decode stream?
                if (iterator() == iterator().eos())
                    // If so, flush the encode queue and end this op too
                    encodeSession_.Flush();

                auto returnVal = CPUEncodedFrameData(physical().codec(), decoded.configuration(), decoded.geometry(), writer_.dequeue());
                GLOBAL_TIMER.endSection("GPUEncodeToCPU");
                timer_.endSection("inside-GPUEncodeToCPU");
                return {returnVal};
            } else {
                GLOBAL_TIMER.endSection("GPUEncodeToCPU");
                std::cout << "**Encoded " << numberOfEncodedFrames_ << " frames\n";
                timer_.printAllTimes();
                return std::nullopt;
            }
        }

    private:
        unsigned int gop() const {
            auto option = logical().is<OptionContainer<>>()
                          ? logical().downcast<OptionContainer<>>().get_option(EncodeOptions::GOPSize)
                          : std::nullopt;

            if(option.has_value() && option.value().type() != typeid(unsigned int))
                throw InvalidArgumentError("Invalid GOP option specified", EncodeOptions::GOPSize);
            else
                return std::any_cast<unsigned int>(option.value_or(
                        std::make_any<unsigned int>(kDefaultGopSize)));
        }

        EncodeConfiguration encodeConfiguration_;
        VideoEncoder encoder_;
        MemoryEncodeWriter writer_;
        VideoEncoderSession encodeSession_;
        Timer timer_;
        unsigned long numberOfEncodedFrames_;
    };

    const Codec codec_;
};

}; // namespace lightdb::physical

#endif //LIGHTDB_ENCODEOPERATORS_H
