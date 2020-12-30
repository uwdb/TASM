#include "udf.h"

#include "cuda.h"
#include <nppi_color_conversion.h>
#include <nppi_geometry_transforms.h>
#include <nppi_data_exchange_and_initialization.h>
#include <nppi_arithmetic_and_logical_operations.h>
#include <fstream>
#include <ostream>

using namespace lightdb;

YOLOGPU::GPU::GPU()
    : GPU("/home/maureen/yolo_gpu_lightdb/darknet/cfg/yolov3.cfg",
            "/home/donghe/background-subtraction/yolo/yolov3.weights",
            "/home/maureen/yolo_gpu_lightdb/darknet/data/coco.names")
{ }

// From https://github.com/AlexeyAB/darknet/blob/master/src/yolo_console_dll.cpp
std::vector<std::string> YOLOGPU::GPU::objects_names_from_file(const std::string &filename) {
    std::ifstream file(filename);
    std::vector<std::string> file_lines;
    if (!file.is_open())
        return file_lines;
    for (std::string line; getline(file, line); ) {
        // Remove newline character that getline retrieves.
        file_lines.push_back(line.substr(0, line.length() - 1));
    }
    return file_lines;
}

void YOLOGPU::GPU::handlePostCreation(const std::shared_ptr<void> &arg) {
    tasm_ = std::static_pointer_cast<Tasm>(arg);
    numFramesDetected_ = 0;
}

void YOLOGPU::GPU::handleAllDataHasBeenProcessed() {
    std::cout << "Ran YOLO on " << numFramesDetected_ << " frames" << std::endl;
}

shared_reference<LightField> YOLOGPU::GPU::operator()(LightField &input) {
    auto &data = dynamic_cast<physical::GPUDecodedFrameData&>(input);
    physical::GPUDecodedFrameData output(data.configuration(), data.geometry());

    for (auto &frame : data.frames()) {
        long frameNumber = 0;
        assert(frame->getFrameNumber(frameNumber));

        // Check whether detection has been run on the frame yet.
        if (tasm_ && tasm_->detectionHasBeenRunOnFrame("", frameNumber))
            continue;

        ++numFramesDetected_;
        preprocessFrame(frame);
        detectFrame(frameNumber);
    }

    return shared_reference<physical::GPUDecodedFrameData>(data);
}

void YOLOGPU::GPU::preprocessFrame(GPUFrameReference &frame) {
    // Allocate arrays for conversions.
    allocate(frame->width(), frame->height());

    auto y_data = reinterpret_cast<unsigned char *>(frame->cuda()->handle());
    auto uv_data = y_data + frame->cuda()->pitch() * frame->codedHeight();

    // Convert to planar.
    NppStatus status;
    Npp8u* pSrc[2] = {y_data, uv_data};
    int nSrcStep = frame->cuda()->pitch();
    Npp8u* yuvSrc[3] = {reinterpret_cast<unsigned char *>(yuvHandle_),
                        reinterpret_cast<unsigned char *>(yuvHandle_ + height_ * yuvPitch_),
                        reinterpret_cast<unsigned char *>(yuvHandle_ + 2 * height_ * yuvPitch_)};
    int yuvStep[3] = {yuvPitch_, yuvPitch_, yuvPitch_};
    NppiSize oSizeROI{width_, height_};
    status = nppiNV12ToYUV420_8u_P2P3R(pSrc, nSrcStep, yuvSrc, yuvStep, oSizeROI);
    assert(status == NPP_SUCCESS);

    // YUV420 -> BGR
    Npp8u* bgrSrc = reinterpret_cast<unsigned char *>(bgrHandle_);
    status = nppiYUV420ToBGR_8u_P3C3R(yuvSrc, yuvStep, bgrSrc, bgrPitch_, oSizeROI);
    assert(status == NPP_SUCCESS);

    // Resize.
    NppiRect oSrcRectROI{0, 0, width_, height_};
    NppiSize resizedSize{inputWidth_, inputHeight_};
    NppiRect resizedRectROI{0, 0, inputWidth_, inputHeight_};
    Npp8u* resizedSrc = reinterpret_cast<unsigned char *>(resizedHandle_);
    status = nppiResize_8u_C3R(bgrSrc, bgrPitch_, oSizeROI, oSrcRectROI,
                               resizedSrc, resizedPitch_, resizedSize, resizedRectROI,
                               NPPI_INTER_LINEAR);
    assert(status == NPP_SUCCESS);

    // To float.
    Npp32f* packedFloatSrc = reinterpret_cast<float *>(packedFloatHandle_);
    status = nppiConvert_8u32f_C3R(
            reinterpret_cast<unsigned char *>(resizedHandle_), resizedPitch_,
            packedFloatSrc, packedFloatPitch_, resizedSize);
    assert(status == NPP_SUCCESS);

    // To scaled float (x / 255).
    status = nppiDivC_32f_C1IR(255.f, packedFloatSrc, packedFloatPitch_, {3 * inputWidth_, inputHeight_});
    assert(status == NPP_SUCCESS);

    // To planar.
    auto resizedFrameSize = inputWidth_ * inputHeight_ * sizeof(Npp32f);
    Npp32f* planarDst[3] = {
            reinterpret_cast<float *>(resizedPlanarHandle_),
            reinterpret_cast<float *>(resizedPlanarHandle_ + resizedFrameSize),
            reinterpret_cast<float *>(resizedPlanarHandle_ + 2 * resizedFrameSize)
    };
    status = nppiCopy_32f_C3P3R(packedFloatSrc, packedFloatPitch_, planarDst, sizeof(Npp32f) * inputWidth_, resizedSize);
    assert(status == NPP_SUCCESS);
}

static float scaled(int value, int downsampled, int upsampled) {
    return float(value) / downsampled * upsampled;
}

void YOLOGPU::GPU::detectFrame(long frameNumber) {
    // Detection runs on the resized, planar image.
    std::vector<bbox_t> predictions = detector_->detect_gpu(reinterpret_cast<float *>(resizedPlanarHandle_), inputWidth_, inputHeight_, threshold_);

    for (const auto &prediction : predictions) {
        if (prediction.prob <= minProb_)
            continue;
        auto &label = objectNames_[prediction.obj_id];
//        std::cout << "Detected " << label << " with prob " << prediction.prob << " on frame " << prediction.frames_counter << std::endl;

        if (tasm_)
            tasm_->addMetadata("", label, frameNumber,
                               static_cast<int>(scaled(prediction.x, inputWidth_, width_)),
                               static_cast<int>(scaled(prediction.y, inputHeight_, height_)),
                                static_cast<int>(scaled(prediction.w, inputWidth_, width_)),
                                static_cast<int>(scaled(prediction.h, inputHeight_, height_)));
    }
    if (tasm_)
        tasm_->markDetectionHasBeenRunOnFrame("", frameNumber);
}

void YOLOGPU::GPU::allocate(unsigned int width, unsigned int height) {
    if (width == width_ && height == height_)
        return;

    width_ = width;
    height_ = height;

    deallocate();
    CUresult status;

    // Allocate for YUV420.
    // Height should be 3 * frame height? 3/2 * height?
    status = cuMemAllocPitch(&yuvHandle_, &yuvPitch_, width, height * channels_, 8);
    assert(status == CUDA_SUCCESS);

    // Allocate for packed BGR.
    // Width should be 3 * frame width.
    // Height should be frame height.
    status = cuMemAllocPitch(&bgrHandle_, &bgrPitch_, channels_ * width, height, 8);
    assert(status == CUDA_SUCCESS);

    // Allocate for resized packed BGR.
    status = cuMemAllocPitch(&resizedHandle_, &resizedPitch_, channels_ * inputWidth_, inputHeight_, 8);
    assert(status == CUDA_SUCCESS);

    // Allocate for float packed BGR.
    status = cuMemAllocPitch(&packedFloatHandle_, &packedFloatPitch_, sizeof(float) * channels_ * inputWidth_, inputHeight_, 8);
    assert(status == CUDA_SUCCESS);

    // Allocate for flat resized, planar BGR.
    status = cuMemAlloc(&resizedPlanarHandle_, sizeof(float) * channels_ * inputWidth_ * inputHeight_);
    assert(status == CUDA_SUCCESS);
}

void YOLOGPU::GPU::deallocate() {
    CUresult status;
    if (yuvHandle_) {
        status = cuMemFree(yuvHandle_);
        assert(status == CUDA_SUCCESS);
        yuvHandle_ = 0;
    }

    if (bgrHandle_) {
        status = cuMemFree(bgrHandle_);
        assert(status == CUDA_SUCCESS);
        bgrHandle_ = 0;
    }

    if (resizedHandle_) {
        status = cuMemFree(resizedHandle_);
        assert(status == CUDA_SUCCESS);
        resizedHandle_ = 0;
    }

    if (packedFloatHandle_) {
        status = cuMemFree(packedFloatHandle_);
        assert(status == CUDA_SUCCESS);
        packedFloatHandle_ = 0;
    }

    if (resizedPlanarHandle_) {
        status = cuMemFree(resizedPlanarHandle_);
        assert(status == CUDA_SUCCESS);
        resizedPlanarHandle_ = 0;
    }
}

//void YOLOGPU::GPU::saveToNpy() {
//    if (count_ > 0)
//        return;
//
//    std::vector<unsigned char> outVec(inputWidth_ * inputHeight_ * channels_ * sizeof(float), 0);
////    auto params = CUDA_MEMCPY2D {
////        .srcXInBytes = 0,
////        .srcY = 0,
////        .srcMemoryType = CU_MEMORYTYPE_DEVICE,
////        .srcHost = nullptr,
////        .srcDevice = packedFloatHandle_,
////        .srcArray = nullptr,
////        .srcPitch = packedFloatPitch_,
////
////        .dstXInBytes = 0,
////        .dstY = 0,
////
////        .dstMemoryType = CU_MEMORYTYPE_HOST,
////        .dstHost = outVec.data(),
////        .dstDevice = 0,
////        .dstArray = nullptr,
////        .dstPitch = 0,
////
////        .WidthInBytes = inputWidth_ * 3 * sizeof(float),
////        .Height = inputHeight_
////    };
////    CUresult status = cuMemcpy2D(&params);
////    assert(status == CUDA_SUCCESS);
//
//    std::ofstream fout("resizedViaGPU.npy", std::ios::out | std::ios::binary); // | std::ios::app);
//
//    ++count_;
////    auto numPixels = inputWidth_ * inputHeight_ * channels_;
////    auto length = numPixels * sizeof(float);
////    std::vector<float> outVec(numPixels);
//    CUresult status = cuMemcpyDtoH(outVec.data(), resizedPlanarHandle_, outVec.size());
//    assert(status == CUDA_SUCCESS);
//
//    fout.write(reinterpret_cast<const char*>(outVec.data()), outVec.size());
//}

YOLOGPU yologpu;
