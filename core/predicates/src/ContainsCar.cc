#include "ContainsCar.h"
#include "ipp.h"
#include "timer.h"

// PP imports
#include <msd_wrapper.h>
#include <linear_wrapper.h>
#include <image.h>
#include <box.h>

bool MSDPredicate::instantiatedModels_ = 0;
bool LinearPredicate::instantiatedModels_ = 0;

void MSDPredicate::loadModel() {
    if (!instantiatedModels_) {
        _init_msd_models();
        instantiatedModels_ = true;
    }

    _load_msd_model(modelName_.c_str());
    _print_msd_model_names();
}

void MSDPredicate::freeModel() {
    _free_msd_model(modelName_.c_str());
}

void LinearPredicate::loadModel() {
    if (!instantiatedModels_) {
        _init_linear_models();
        instantiatedModels_ = true;
    }

    _load_linear_model(modelName_.c_str());
    _print_linear_model_names();
}

void LinearPredicate::freeModel() {
    _free_linear_model(modelName_.c_str());
}

std::vector<box> ContainsCarPredicate::detectImage(const std::string &imagePath) {
    return _detect_msd_model(modelName_.c_str(), imagePath.c_str());
}

std::vector<box> ContainsCarPredicate::detectImage(image im) {
    return _detect_msd_model_from_image(modelName_.c_str(), im);
}

static int COUNT = 0;

std::unique_ptr<std::vector<float>> CarColorFeaturePredicate::getFeatures(image im) {
    auto resized = resize_image(im, modelWidth_, modelHeight_);
    std::string cropName = std::string("crop_") + std::to_string(COUNT) + std::string(".jpg");
    save_image(resized, cropName.c_str());
    ++COUNT;
    auto features = _forward_msd_model_ptr(modelName_.c_str(), resized.data, resized.w * resized.w * resized.c, 1, 0);
    free_image(resized);
    return features;
}

IntVectorPtr CarColorPredicate::classifyColors(float *features, unsigned int len_in) {
    return _classify_linear_model_ptr(modelName_.c_str(), features, len_in, 1, false);
}

std::vector<FloatVectorPtr> lightdb::physical::PredicateOperator::Runtime::getCarColorFeatures(image im, const std::vector<box> &crops) {
    std::vector<FloatVectorPtr> features;
    for (auto &crop : crops) {
        features.push_back(carColorFeaturePredicate_->getFeatures(crop_image(im, crop.x, crop.y, crop.w, crop.h)));
    }
    return features;
}

std::vector<IntVectorPtr> lightdb::physical::PredicateOperator::Runtime::getCarColors(const std::vector<FloatVectorPtr> &features) {
    std::vector<IntVectorPtr> colors;
    for (auto &feat : features)
        colors.push_back(carColorPredicate_->classifyColors(feat->data(), feat->size()));
    return colors;
}

void lightdb::physical::PredicateOperator::Runtime::convertFrames(CPUDecodedFrameData data) {
    const auto channels = 3u;
    std::vector<float> output;
    for (auto &frame : data.frames()) {
        Timer timer;
        timer.startSection("transform frame");
        Allocate(frame->height(), frame->width(), channels);
        auto y_data = reinterpret_cast<const unsigned char *>(frame->data().data());
        auto uv_data = y_data + frame_size_;
        IppiSize size{static_cast<int>(frame->width()), static_cast<int>(frame->height())};

        // NV12 -> RGB
        //assert(
        ippiYCbCr420ToRGB_8u_P2C3R(y_data, frame->width(), uv_data, frame->width(), rgb_.data(),
                                   channels * frame->width(), size);//== ippStsNoErr);

        // RGBRGBRGB -> RRRBBBGGG
        assert(ippiCopy_8u_C3P3R(rgb_.data(), 3 * frame->width(), std::initializer_list<unsigned char *>(
                {planes_.data(), planes_.data() + frame_size_, planes_.data() + 2 * frame_size_}).begin(),
                                 frame->width(), size) == ippStsNoErr);

        // uchar -> float
        assert(ippsConvert_8u32f(planes_.data(), scaled_.data(), total_size_) == ippStsNoErr);

        // float -> scaled float (x / 255)
        assert(ippsDivC_32f_I(255.f, scaled_.data(), total_size_) == ippStsNoErr);
        timer.endSection("transform frame");

        timer.startSection("predict");
        auto frameIm = float_to_image(frame->width(), frame->height(), channels, scaled_.data());
        auto boxes = carPredicate_->detectImage(frameIm);
        timer.endSection("predict");

        timer.startSection("Get features");
        auto features = getCarColorFeatures(frameIm, boxes);
        timer.endSection("Get features");

        timer.startSection("Get colors");
        auto colors = getCarColors(features);
        timer.endSection("Get colors");

        timer.printAllTimes();
    }
}

void lightdb::physical::PredicateOperator::Runtime::Allocate(unsigned int height, unsigned int width, unsigned int channels) {
    if (total_size_ != channels * width * height) {
        frame_size_ = height * width;
        total_size_ = channels * frame_size_;
        rgb_.resize(total_size_);
        scaled_.resize(total_size_);
        planes_.resize(total_size_);
//        resized_.resize(carPredicate_->modelWidth() * carPredicate_->modelHeight() * channels);
    }
}
