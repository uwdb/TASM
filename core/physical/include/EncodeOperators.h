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
              codec_(std::move(codec)),
              didSetFramesToKeep_(false) {
        if(!codec.nvidiaId().has_value())
            throw GpuRuntimeError("Requested codec does not have an Nvidia encode id");
    }

    const Codec &codec() const { return codec_; }

    const bool didSetFramesToKeep() const { return didSetFramesToKeep_; }
    const std::unordered_set<int> &framesToKeep() const { return framesToKeep_; }
    void setFramesToKeep(std::unordered_set<int> framesToKeep) {
        didSetFramesToKeep_ = true;
        framesToKeep_ = std::move(framesToKeep);
    }

private:
    class Runtime: public runtime::GPUUnaryRuntime<GPUEncodeToCPU, GPUDecodedFrameData> {
    public:
        explicit Runtime(GPUEncodeToCPU &physical)
            : runtime::GPUUnaryRuntime<GPUEncodeToCPU, GPUDecodedFrameData>(physical),
              encodeConfiguration_{configuration(), this->physical().codec().nvidiaId().value(), gop()},
              encoder_{this->context(), encodeConfiguration_, lock()},
              writer_{encoder_.api()},
              encodeSession_{encoder_, writer_},
              numberOfEncodedFrames_(0),
              frameNumber_(0)
        {
            keyframes_ = getKeyframes();
        }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            GLOBAL_TIMER.startSection("GPUEncodeToCPU");
            if (iterator() != iterator().eos()) {
                auto decoded = iterator()++;

                timer_.startSection("inside-GPUEncodeToCPU");
                for (const auto &frame: decoded.frames()) {
                    auto frameNumber = frameNumber_++;
                    if (physical().didSetFramesToKeep() && !physical().framesToKeep().count(frameNumber))
                        continue;

                    ++numberOfEncodedFrames_;
                    encodeSession_.Encode(*frame, decoded.configuration().offset.top,
                                          decoded.configuration().offset.left,
                                          keyframes_.count(frameNumber));
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
                std::cout << "**Encoded " << numberOfEncodedFrames_ << " frames, considered " << frameNumber_ << "frames\n";
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

        std::unordered_set<int> getKeyframes() const {
            auto option = logical().is<OptionContainer<>>()
                    ? logical().downcast<OptionContainer<>>().get_option(EncodeOptions::Keyframes)
                    : std::nullopt;

            if (option.has_value() && option.value().type() != typeid(std::unordered_set<int>))
                throw InvalidArgumentError("Invalid keyframes option specified", EncodeOptions::Keyframes);
            else if (option.has_value())
                return std::any_cast<std::unordered_set<int>>(option.value());
            else
                return {};
        }

        EncodeConfiguration encodeConfiguration_;
        VideoEncoder encoder_;
        MemoryEncodeWriter writer_;
        VideoEncoderSession encodeSession_;
        Timer timer_;
        unsigned long numberOfEncodedFrames_;
        unsigned long frameNumber_;
        std::unordered_set<int> keyframes_;
    };

    const Codec codec_;

    // Add optional list of frames to keep.
    bool didSetFramesToKeep_;
    std::unordered_set<int> framesToKeep_;
};

}; // namespace lightdb::physical

#endif //LIGHTDB_ENCODEOPERATORS_H
