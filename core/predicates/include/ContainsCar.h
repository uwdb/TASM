#ifndef LIGHTDB_CONTAINSCAR_H
#define LIGHTDB_CONTAINSCAR_H

#include "PhysicalOperators.h"
#include <string>

#include <image.h>
#include <box.h>

// Try re-defining missing function?
//#include <cudnn.h>
//cudnnHandle_t cudnn_handle();

typedef std::unique_ptr<std::vector<int>> IntVectorPtr;
typedef std::unique_ptr<std::vector<float>> FloatVectorPtr;

class Predicate {
public:
    Predicate(const std::string &name,
              unsigned int width,
              unsigned int height)
            : modelName_(name),
              modelWidth_(width),
              modelHeight_(height)
    {}

    virtual void loadModel() = 0;
    virtual unsigned int modelWidth() const { return modelWidth_; }
    virtual unsigned int modelHeight() const { return modelHeight_; }
    virtual ~Predicate() {
        freeModel();
    }

protected:
    virtual void freeModel() {}
    std::string modelName_;
    unsigned int modelWidth_;
    unsigned int modelHeight_;
};

class MSDPredicate : public Predicate {
public:
    MSDPredicate(const std::string &name,
              unsigned int width,
              unsigned int height)
      : Predicate(name, width, height) {}

    void loadModel() override;

protected:
    void freeModel() override;
    static bool instantiatedModels_;
};

class LinearPredicate : public Predicate {
public:
    LinearPredicate(const std::string &name,
                 unsigned int width,
                 unsigned int height)
        : Predicate(name, width, height) {}
    void loadModel() override;

protected:
    void freeModel() override;
    static bool instantiatedModels_;
};

class ContainsCarPredicate : public MSDPredicate {
public:
    ContainsCarPredicate()
        : MSDPredicate("data/detrac.model", 736, 736)
    {}

    std::vector<box> detectImage(const std::string &imagePath);
    std::vector<box> detectImage(image im);
};

class CarColorFeaturePredicate : public MSDPredicate {
public:
    CarColorFeaturePredicate()
        : MSDPredicate("data/darknet_conv_color.model", 64, 64)
    {}

    FloatVectorPtr getFeatures(image im);
};

class CarColorPredicate : public LinearPredicate {
public:
    CarColorPredicate()
        : LinearPredicate("data/detrac_color.model", 0, 0)
    {}

    IntVectorPtr classifyColors(float *features, unsigned int len_in);
};

class DetracPPFeaturePredicate : public MSDPredicate {
public:
    DetracPPFeaturePredicate()
        : MSDPredicate("data/detrac_pp_conv.model", 384, 384)
    {}

    FloatVectorPtr getFeatures(image im);
    FloatVectorPtr getFeaturesForResizedImages(std::vector<float> &resizedImages, unsigned int batchSize);
};

class DetracBusPredicate : public LinearPredicate {
public:
    DetracBusPredicate()
        : LinearPredicate("data/pps/detrac-type-bus.model", 0, 0)
    {}

    bool matches(FloatVectorPtr features);
};

namespace lightdb::physical {
class PredicateOperator : public PhysicalOperator {
public:
    PredicateOperator(const LightFieldReference &logical,
            PhysicalOperatorReference &parent)
            : PhysicalOperator(logical, {parent}, physical::DeviceType::CPU, runtime::make<Runtime>(*this, "PredicateOperator-init"))
        {}

private:
    class Runtime : public runtime::UnaryRuntime<PredicateOperator, CPUDecodedFrameData> {
    public:
        explicit Runtime(PredicateOperator &physical)
                : runtime::UnaryRuntime<PredicateOperator, CPUDecodedFrameData>(physical),
                        frame_size_(0),
                        total_size_(0),
                        carPredicate_(std::unique_ptr<ContainsCarPredicate>(new ContainsCarPredicate())),
                        carColorFeaturePredicate_(std::unique_ptr<CarColorFeaturePredicate>(new CarColorFeaturePredicate())),
                        carColorPredicate_(std::make_unique<CarColorPredicate>()),
                        ppFeaturePredicate_(std::make_unique<DetracPPFeaturePredicate>()),
                        busPredicate_(std::make_unique<DetracBusPredicate>())
            {
                carPredicate_->loadModel();
                carColorFeaturePredicate_->loadModel();
                carColorPredicate_->loadModel();
                ppFeaturePredicate_->loadModel();
                busPredicate_->loadModel();
            }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            while (iterator() != iterator().eos()) {
                auto data = iterator()++;
                convertFrames(data);
            }
            return {};
        }

    private:
        void convertFrames(CPUDecodedFrameData data);
        void Allocate(unsigned int height, unsigned int width, unsigned int channels);
        std::vector<FloatVectorPtr> getCarColorFeatures(image im, const std::vector<box> &crops);
        std::vector<IntVectorPtr> getCarColors(const std::vector<std::unique_ptr<std::vector<float>>> &features);

        unsigned int frame_size_;
        unsigned int total_size_;
        std::vector<unsigned char> rgb_;
        std::vector<unsigned char> planes_;
        std::vector<float> scaled_;
        std::vector<float> resized_;
        std::unique_ptr<ContainsCarPredicate> carPredicate_;
        std::unique_ptr<CarColorFeaturePredicate> carColorFeaturePredicate_;
        std::unique_ptr<CarColorPredicate> carColorPredicate_;
        std::unique_ptr<DetracPPFeaturePredicate> ppFeaturePredicate_;
        std::unique_ptr<DetracBusPredicate> busPredicate_;
    };

};

} // namespace lightdb::physical


#endif //LIGHTDB_CONTAINSCAR_H
