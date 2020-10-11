#ifndef LIGHTDB_CONTAINSCAR_H
#define LIGHTDB_CONTAINSCAR_H

#include "PhysicalOperators.h"
#include <string>

#include <image.h>
#include <box.h>
#include "ipp.h"

#include <ostream>

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

class BlazeItPredicate {
public:
    BlazeItPredicate()
        : modelWidth_(65), modelHeight_(65)
    {}

    unsigned int modelWidth() const { return modelWidth_; }
    unsigned int modelHeight() const { return modelHeight_; }

private:
    unsigned int modelWidth_;
    unsigned int modelHeight_;
};

namespace lightdb::physical {
class PredicateOperator : public PhysicalOperator {
public:
    PredicateOperator(const LightFieldReference &logical,
            PhysicalOperatorReference &parent,
            std::string outName)
            : PhysicalOperator(logical, {parent}, physical::DeviceType::CPU, runtime::make<Runtime>(*this, "PredicateOperator-init", outName))
        {}

private:
    class Runtime : public runtime::UnaryRuntime<PredicateOperator, CPUDecodedFrameData> {
    public:
        explicit Runtime(PredicateOperator &physical, const std::string fileName="")
                : runtime::UnaryRuntime<PredicateOperator, CPUDecodedFrameData>(physical),
                        blazeItPredicate_(std::make_unique<BlazeItPredicate>()),
                        frame_size_(0),
                        total_size_(0),
                        model_size_(0),
                        downsampled_frame_size_(0),
                        pSpec_(0),
                        pBuffer_(0),
                        shouldSave_(fileName.length()),
                        fout_(fileName + ".dat", std::ios::out | std::ios::binary)
            {}

        std::optional<physical::MaterializedLightFieldReference> read() override;

        ~Runtime() {
            if (pSpec_)
                ippsFree(pSpec_);
            if (pBuffer_)
                ippsFree(pBuffer_);

            fout_.close();
        }

    private:
        void convertFrames(CPUDecodedFrameData &data);
        void Allocate(unsigned int height, unsigned int width, unsigned int channels);
        IppStatus resize(const Ipp8u *src, Ipp32s srcStep, Ipp8u *pDst, Ipp32s dstStep);
        std::unique_ptr<BlazeItPredicate> blazeItPredicate_;

        unsigned int frame_size_;
        unsigned int total_size_;
        unsigned int model_size_;
        unsigned int downsampled_frame_size_;
        std::vector<unsigned char> rgb_;
        std::vector<float> planes_;
        std::vector<float> scaled_;
        std::vector<float> normalized_;
        std::vector<unsigned char> resized_;

        // Structures for resizing.
        IppiResizeSpec_32f *pSpec_;
        Ipp8u* pBuffer_;
        IppiSize srcSize_;
        IppiSize dstSize_;

        bool shouldSave_;
        std::ofstream fout_;
    };

};

} // namespace lightdb::physical


#endif //LIGHTDB_CONTAINSCAR_H
