#include "TasmOperators.h"

#include "Stitcher.h"
#include "cuda.h"
#include <nppi_color_conversion.h>
#include <nppi_geometry_transforms.h>
#include <nppi_data_exchange_and_initialization.h>
#include <nppi_arithmetic_and_logical_operations.h>

namespace lightdb::physical {

static const unsigned int MAX_PPS_ID = 64;
static const unsigned int ALIGNMENT = 32;

static std::unique_ptr<bytestring> ReadFile(const std::filesystem::path &path) {
    std::ifstream istrm(path);
    return std::make_unique<bytestring>(std::istreambuf_iterator<char>(istrm), std::istreambuf_iterator<char>());
}

static std::vector<unsigned int> ToCtbs(const std::vector<unsigned int> &pixelVals) {
    std::vector<unsigned int> ctbs(pixelVals.size() - 1);
    std::transform(pixelVals.begin(), std::prev(pixelVals.end(), 1), ctbs.begin(), [](auto &v) { return std::ceil(v / ALIGNMENT); });
    return ctbs;
}

std::shared_ptr<bytestring> StitchOperator::Runtime::getSEI() {
    if (sei_)
        return sei_;

    sei_ = std::shared_ptr(ReadFile(seiPath_));
    return sei_;
}

std::shared_ptr<bytestring> StitchOperator::Runtime::stitchGOP(int gop) {
    // First, get tile layout for the current GOP.
    auto firstFrameOfGOP = gop * gopLength_;
    auto tileLayout = tileLocationProvider_->tileLayoutForFrame(firstFrameOfGOP);

    // Special case when there is one tile:
    // Send special SEI followed by the single tile.
    if (tileLayout.numberOfTiles() == 1) {
        if (!sentSEIForOneTile_) {
            sentSEIForOneTile_ = true;
            return getSEI();
        } else {
            sentSEIForOneTile_ = false;
            return ReadFile(tileLocationProvider_->locationOfTileForFrame(0, firstFrameOfGOP));
        }
    }

    // First: create context.
    std::pair<unsigned int, unsigned int> tileDimensions{tileLayout.numberOfRows(), tileLayout.numberOfColumns()};
    std::pair<unsigned int, unsigned int> videoCodedDimensions{tileLayout.codedHeight(), tileLayout.codedWidth()};
    std::pair<unsigned int, unsigned int> videoDisplayDimensions{tileLayout.totalHeight(), tileLayout.totalWidth()};
    bool shouldUseUniformTiles = false;
    hevc::StitchContext context(tileDimensions,
            videoCodedDimensions,
            videoDisplayDimensions,
            shouldUseUniformTiles,
            ToCtbs(tileLayout.heightsOfRows()),
            ToCtbs(tileLayout.widthsOfColumns()),
            ppsId_++);

    // Respect limits on PPS_ID in HEVC specification.
    if (ppsId_ >= MAX_PPS_ID)
        ppsId_ = 1;

    // Second: read tiles into memory
    std::vector<bytestring> tiles;
    for (auto i = 0u; i < tileLayout.numberOfTiles(); ++i) {
        auto tilePath = tileLocationProvider_->locationOfTileForFrame(i, firstFrameOfGOP);
        tiles.push_back(*ReadFile(tilePath));
    }

    hevc::Stitcher stitcher(context, tiles);
    auto stitchedSegments = stitcher.GetStitchedSegments();
    return std::move(stitchedSegments);
}

std::optional<MaterializedLightFieldReference> GPUCreateBlackTile::Runtime::makeTiles() {
    Configuration configuration{width_, height_,
                                width_, height_,
                                0,
                                Configuration::FrameRate(numFrames_),
                                {0, 0}};
    GeometryReference geometry = GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples());
    GPUDecodedFrameData blackFrames(configuration, geometry);

    for (auto i = 0u; i < numFrames_; ++i) {
        auto cudaInfo = makeFrame();
        blackFrames.frames().push_back(GPUFrameReference::make<CudaFrame>(Frame(configuration, NV_ENC_PIC_STRUCT_FRAME, i), cudaInfo.first, cudaInfo.second, true));
    }
    return {blackFrames};
}

std::pair<CUdeviceptr, size_t> GPUCreateBlackTile::Runtime::makeFrame() {
    CUdeviceptr handle;
    size_t pitch;
    CUresult result = cuMemAllocPitch(&handle, &pitch, width_, height_ * 3 / 2, 16);
    assert(result == CUDA_SUCCESS);

    // Set luma to 0.
    result = cuMemsetD2D8(handle,
                        pitch,
                        0,
                        width_,
                        height_);
    assert(result == CUDA_SUCCESS);

    // Set chroma to 128.
    auto uvOffset = pitch * height_;
    auto uvHeight = height_ / 2;
    result = cuMemsetD2D8(handle + uvOffset,
                          pitch,
                          128,
                          width_,
                          uvHeight);
    assert(result == CUDA_SUCCESS);

    return std::make_pair(handle, pitch);
}

void GPUSaveToArray::Runtime::allocate(unsigned int width, unsigned int height) {
    if (width == width_ && height == height_)
        return;

    width_ = width;
    height_ = height;

    deallocate();

    CUresult status = cuMemAllocPitch(&yuvHandle_, &yuvPitch_, width, height * channels_, 8);
    assert(status == CUDA_SUCCESS);

    status = cuMemAllocPitch(&bgrHandle_, &bgrPitch_, channels_ * width, height, 8);
    assert(status == CUDA_SUCCESS);

    status = cuMemAllocPitch(&packedFloatHandle_, &packedFloatPitch_, sizeof(float) * channels_ * width_, height_, 8);
    assert(status == CUDA_SUCCESS);

    // Allocate for flat planar BGR.
    status = cuMemAlloc(&planarHandle_, sizeof(float) * channels_ * width_ * height_);
    assert(status == CUDA_SUCCESS);
}

void GPUSaveToArray::Runtime::deallocate() {
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

    if (packedFloatHandle_) {
        status = cuMemFree(packedFloatHandle_);
        assert(status == CUDA_SUCCESS);
        packedFloatHandle_ = 0;
    }

    if (planarHandle_) {
        status = cuMemFree(planarHandle_);
        assert(status == CUDA_SUCCESS);
        planarHandle_ = 0;
    }
}

void GPUSaveToArray::Runtime::processFrame(GPUFrameReference &frame) {
    allocate(frame->width(), frame->height());

    auto y_data = reinterpret_cast<unsigned char*>(frame->cuda()->handle());
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

    // To float.
    Npp32f* packedFloatSrc = reinterpret_cast<float *>(packedFloatHandle_);
    status = nppiConvert_8u32f_C3R(
            reinterpret_cast<unsigned char *>(bgrHandle_), bgrPitch_,
            packedFloatSrc, packedFloatPitch_, oSizeROI);
    assert(status == NPP_SUCCESS);

    // To scaled float (x / 255).
    status = nppiDivC_32f_C1IR(255.f, packedFloatSrc, packedFloatPitch_, {3 * width_, height_});
    assert(status == NPP_SUCCESS);

    // To planar.
    auto frameSize = width_ * height_ * sizeof(Npp32f);
    Npp32f* planarDst[3] = {
            reinterpret_cast<float *>(planarHandle_),
            reinterpret_cast<float *>(planarHandle_ + frameSize),
            reinterpret_cast<float *>(planarHandle_ + 2 * frameSize)
    };
    status = nppiCopy_32f_C3P3R(packedFloatSrc, packedFloatPitch_, planarDst, sizeof(Npp32f) * width_, oSizeROI);
    assert(status == NPP_SUCCESS);
}

std::unique_ptr<std::vector<unsigned char>> GPUSaveToArray::Runtime::pixelData() {
    auto data = std::make_unique<std::vector<unsigned char>>(width_ * height_ * channels_ * sizeof(float), 0);
    CUresult status = cuMemcpyDtoH(data->data(), planarHandle_, data->size());
    assert(status == CUDA_SUCCESS);

    return std::move(data);
}

} // namespace lightdb::physical
