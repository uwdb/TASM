#ifndef LIGHTDB_DECODESESSIONMANAGER_H
#define LIGHTDB_DECODESESSIONMANAGER_H

#include "ThreadPool.h"
#include "LightField.h"
#include "MaterializedLightField.h"

#include "cuviddec.h"
#include "nvcuvid.h"
#include <mutex>

namespace lightdb {

//namespace decoded {
//struct DecodedFrameInformation {
//    unsigned int frameNumber;
//    CUdeviceptr handle;
//    unsigned int pitch;
//    CUVIDEOFORMAT format;
//};
//} // namespace lightdb::decoded

class DecodeSession {
public:
    DecodeSession(unsigned int identifier, VideoLock &lock, DecodeConfiguration &configuration)
        : identifier_(identifier),
        lock_(lock),
        configuration_(configuration),
        queue_(lock),
        frameNumberQueue_(1000),
        decoder_(std::make_unique<CudaDecoder>(configuration, queue_, lock, frameNumberQueue_)),
        threadPool_(1),
        currentTileNumber_(-1)
    { }

    DecodeSession(DecodeSession &&other) = delete;

    ~DecodeSession() {
        decoder_->frame_queue().endDecode();
        if (parser_)
            cuvidDestroyVideoParser(*parser_);
    }

    void decodePacket(unsigned int tileNumber, physical::MaterializedLightFieldReference encodedData) {
        if (!parser_)
            parser_ = std::make_unique<CUvideoparser>(CreateParser(decoder_.get()));
        else
            assert(decoder_->streamIsComplete());

        decoder_->setIsStreamComplete(false);

        currentTileNumber_ = tileNumber;
        threadPool_.push(std::bind(&DecodeSession::DecodePacket, decoder_.get(), *parser_, encodedData));
    }

    bool isComplete() const { return decoder_->streamIsComplete(); }
    unsigned int tileNumber() const {
        assert(currentTileNumber_ >= 0);
        return static_cast<unsigned int>(currentTileNumber_);
    }

    int tileNumberIfExists() const {
        return currentTileNumber_;
    }

    std::list<decoded::DecodedFrameInformation> read(); // This leads to dequeing from the frame queue.

private:
    std::optional<decoded::DecodedFrameInformation> decode(std::chrono::milliseconds duration);

    static void DecodePacket(CudaDecoder *decoder, CUvideoparser parser, physical::MaterializedLightFieldReference encodedData) {
        auto &cpuEncodedData = encodedData.downcast<physical::CPUEncodedFrameData>();
        int firstFrameIndex = -1;
        int numberOfFrames = -1;
        bool gotFirstFrameIndex = cpuEncodedData.getFirstFrameIndexIfSet(firstFrameIndex);
        bool gotNumberOfFrames = cpuEncodedData.getNumberOfFramesIfSet(numberOfFrames);
        assert(gotFirstFrameIndex);
        assert(gotNumberOfFrames);

//        assert(decoder->frameNumberQueue().empty());

        for (int i = firstFrameIndex; i < firstFrameIndex + numberOfFrames; ++i)
            decoder->frameNumberQueue().push(i);

        auto packet = static_cast<DecodeReaderPacket>(cpuEncodedData);
        CUresult status;
        if ((status = cuvidParseVideoData(parser, &packet)) != CUDA_SUCCESS) {
            cuvidDestroyVideoParser(parser);
            throw GpuCudaRuntimeError("Call to cuvidParseVideoData failed", status);
        }

//        decoder->frame_queue().endDecode();
    }

    static CudaDecoder& ResetDecoder(CudaDecoder &decoder) {
        decoder.frame_queue().reset();
        return decoder;
    }

    static CUvideoparser CreateParser(CudaDecoder *decoder) {
        CUresult status;
        CUvideoparser parser = nullptr;
        CUVIDPARSERPARAMS parameters = {
                .CodecType = decoder->configuration().codec.cudaId().value(),
                .ulMaxNumDecodeSurfaces = decoder->configuration().decode_surfaces,
                .ulClockRate = 0,
                .ulErrorThreshold = 0,
                .ulMaxDisplayDelay = 1,
                .uReserved1 = {},
                .pUserData = decoder,
                .pfnSequenceCallback = HandleVideoSequence,
                .pfnDecodePicture = HandlePictureDecode,
                .pfnDisplayPicture = HandlePictureDisplay,
                {}, nullptr
        };

        if ((status = cuvidCreateVideoParser(&parser, &parameters)) != CUDA_SUCCESS)
            throw GpuCudaRuntimeError("Call to cuvidCreateVideoParser failed", status);

        return parser;
    }

    static int CUDAAPI HandleVideoSequence(void *userData, CUVIDEOFORMAT *format) {
        // TODO: Docs say that here is the best place to reconfigure the decoder.
        // To reconfigure, put the parameters into CUVIDRECONFIGUREDECODERINFO.
        // New width and height must be less than the max's.
        // Then call cuvidReconfigureDecoder.

        auto* decoder = static_cast<CudaDecoder*>(userData);

        assert(format->display_area.bottom - format->display_area.top >= 0);
        assert(format->display_area.right - format->display_area.left >= 0);

        if(decoder == nullptr)
            LOG(ERROR) << "Unexpected null decoder during video decode (HandleVideoSequence)";
        else if (decoder->reconfigureDecoderIfNecessary(format)) {
            // Pass to skip the check below. Do the check if the decoder was not reconfigured because those are cases
            // where it should have been reconfigured.
            return 1;
        } else if ((format->codec != decoder->configuration().codec.cudaId()) ||
                   (format->chroma_format != decoder->configuration().chroma_format))
            throw GpuRuntimeError("Video format changed but not currently supported");

        return 1;
    }

    static int CUDAAPI HandlePictureDecode(void *userData, CUVIDPICPARAMS *parameters) {
        CUresult status;
        auto* decoder = static_cast<CudaDecoder*>(userData);

        if(decoder == nullptr)
            LOG(ERROR) << "Unexpected null decoder during video decode (HandlePictureDecode)";
        else {
            decoder->frame_queue().waitUntilFrameAvailable(parameters->CurrPicIdx);
            if((status = cuvidDecodePicture(decoder->handle(), parameters)) != CUDA_SUCCESS)
                LOG(ERROR) << "cuvidDecodePicture failed (" << status << ")";
        }

        return 1;
    }

    static int CUDAAPI HandlePictureDisplay(void *userData, CUVIDPARSERDISPINFO *frame) {
        auto* decoder = static_cast<CudaDecoder*>(userData);

        if (!frame) {
            decoder->setIsStreamComplete(true);
            return 1;
        }

        if(decoder == nullptr)
            LOG(ERROR) << "Unexpected null decoder during video decode (HandlePictureDisplay)";
        else {
            // TODO: This should happen on a separate thread than cuvidDecodePicture() for performance.
            decoder->mapFrame(frame, decoder->currentFormat());
            decoder->frame_queue().enqueue(frame);
        }

        return 1;
    }

    unsigned int identifier_;
    VideoLock &lock_;
    const DecodeConfiguration configuration_;
    CUVIDFrameQueue queue_;
    spsc_queue<int> frameNumberQueue_;
    std::unique_ptr<CudaDecoder> decoder_;
    ThreadPool threadPool_;
    std::unique_ptr<CUvideoparser> parser_;
    int currentTileNumber_;
};

class DecodeSessionManager {
public:
    DecodeSessionManager(unsigned int numberOfSessions,
            DecodeConfiguration &configuration,
            VideoLock &lock)
        : numberOfSessions_(numberOfSessions)
    {
        setUpDecoders(lock, configuration);
    }

    void decodeData(unsigned int tileNumber, physical::MaterializedLightFieldReference data) {
        std::scoped_lock lock(lock_);
        if (!availableDecoders_.empty()) {
            auto sessionIndex = *availableDecoders_.begin();
            availableDecoders_.erase(sessionIndex);
            decodeSessions_[sessionIndex]->decodePacket(tileNumber, data);
        } else {
            tileAndEncodedDataQueue_.push(std::make_pair(tileNumber, data));
        }
    }

    std::unordered_map<unsigned int, std::list<decoded::DecodedFrameInformation>> read();

    bool allDecodersAreDone() const {
        std::scoped_lock lock(lock_);
        if (!tileAndEncodedDataQueue_.empty())
            return false;

        return std::all_of(decodeSessions_.begin(), decodeSessions_.end(), [](auto &session) {
            return session->isComplete();
        });
    }

    bool anyDecoderIsIncompleteForTile(unsigned int tileNumber) {
        std::any_of(decodeSessions_.begin(), decodeSessions_.end(), [&](auto &session) {
            return session->tileNumberIfExists() != -1 && session->tileNumber() == tileNumber && !session->isComplete();
        });

    }

private:
    void setUpDecoders(VideoLock &lock, DecodeConfiguration &decodeConfiguration);
    void markDecoderAsAvailable(unsigned int index); // Assumes lock is already held.

    unsigned int numberOfSessions_;
    std::vector<std::unique_ptr<DecodeSession>> decodeSessions_;
    std::unordered_set<unsigned int> availableDecoders_;
    std::queue<std::pair<unsigned int, physical::MaterializedLightFieldReference>> tileAndEncodedDataQueue_;
    mutable std::mutex lock_;
};

} // namespace lightdb


#endif //LIGHTDB_DECODESESSIONMANAGER_H
