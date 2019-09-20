#ifndef LIGHTDB_VIDEODECODERSESSION_H
#define LIGHTDB_VIDEODECODERSESSION_H

#include "VideoDecoder.h"
#include "DecodeReader.h"
#include <thread>
#include <chrono>

template<typename Input=DecodeReader::iterator>
class VideoDecoderSession {
public:
    VideoDecoderSession(CudaDecoder& decoder, Input reader, const Input end)
            : decoder_(RestartDecoder(decoder)),
              worker_{std::make_unique<std::thread>(&VideoDecoderSession::DecodeAll, std::ref(decoder), reader, end)}
    { }

    VideoDecoderSession(VideoDecoderSession&& other) noexcept
            : decoder_(other.decoder_),
              worker_(std::move(other.worker_)) {
        other.moved_ = true;
    }

    ~VideoDecoderSession() {
        if(!moved_) {
            decoder_.frame_queue().endDecode();
            worker_->join();
        }
    }

    DecodedFrame decode() {
        return DecodedFrame(decoder_, decoder_.frame_queue().dequeue_wait<CUVIDPARSERDISPINFO>());
    }

    template<typename Rep, typename Period, size_t interval=4>
    std::optional<DecodedFrame> decode(std::chrono::duration<Rep, Period> duration) {
        std::shared_ptr<CUVIDPARSERDISPINFO> packet;

        for(auto begin = std::chrono::system_clock::now();
            std::chrono::system_clock::now() - begin < duration;
            std::this_thread::sleep_for(duration / interval)) {
                if((packet = decoder_.frame_queue().try_dequeue<CUVIDPARSERDISPINFO>()) != nullptr) {
                    auto frameNumber = -1;
                    if (decoder_.frameNumberQueue().pop(frameNumber)) {
                        return {DecodedFrame(decoder_, packet, frameNumber)};
                    } else
                        return {DecodedFrame(decoder_, packet)};
                }
            }

        return std::nullopt;
    }

    const CudaDecoder &decoder() const { return decoder_; }

protected:
    CudaDecoder &decoder_;
    std::unique_ptr<std::thread> worker_;
    bool moved_ = false;

    static void DecodeAll(CudaDecoder &decoder, Input reader, const Input end) {
        CUresult status;

        auto parser = CreateParser(decoder);

        do {
            if (reader != end) {
                int firstFrameIndex = -1;
                int numberOfFrames = -1;
                bool gotFirstFrameIndex = (*reader).getFirstFrameIndexIfSet(firstFrameIndex);
                bool gotNumberOfFrames = (*reader).getNumberOfFramesIfSet(numberOfFrames);
                if (gotFirstFrameIndex && gotNumberOfFrames) {
                    for (int i = firstFrameIndex; i < firstFrameIndex + numberOfFrames; i++) {
                        decoder.frameNumberQueue().push(i);
                    }
                }

                auto packet = static_cast<DecodeReaderPacket>(reader++);
                if ((status = cuvidParseVideoData(parser, &packet)) != CUDA_SUCCESS) {
                    cuvidDestroyVideoParser(parser);
                    throw GpuCudaRuntimeError("Call to cuvidParseVideoData failed", status);
                }
            }
        } while (!decoder.frame_queue().isEndOfDecode() && reader != end);

        cuvidDestroyVideoParser(parser);
        decoder.frame_queue().endDecode();
        LOG(INFO) << "Decode complete; thread terminating.";
    }

private:
    static CudaDecoder& RestartDecoder(CudaDecoder& decoder) {
        decoder.frame_queue().reset();
        return decoder;
    }

    static CUvideoparser CreateParser(CudaDecoder &decoder) {
        CUresult status;
        CUvideoparser parser = nullptr;
        CUVIDPARSERPARAMS parameters = {
            .CodecType = decoder.configuration().codec.cudaId().value(),
            .ulMaxNumDecodeSurfaces = decoder.configuration().decode_surfaces,
            .ulClockRate = 0,
            .ulErrorThreshold = 0,
            .ulMaxDisplayDelay = 1,
            .uReserved1 = {},
            .pUserData = &decoder,
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

        if(decoder == nullptr)
            LOG(ERROR) << "Unexpected null decoder during video decode (HandlePictureDisplay)";
        else {
            decoder->mapFrame(frame);
            decoder->frame_queue().enqueue(frame);
        }

        return 1;
    }
};

#endif //LIGHTDB_VIDEODECODERSESSION_H
