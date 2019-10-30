#ifndef LIGHTDB_VIDEODECODERSESSION_H
#define LIGHTDB_VIDEODECODERSESSION_H

#include "VideoDecoder.h"
#include "DecodeReader.h"
#include <thread>
#include <chrono>

#include "cudaProfiler.h"
#include "timer.h"
#include "ThreadPool.h"
#include "nvToolsExtCuda.h"
#include <mutex>

template<typename Input=DecodeReader::iterator>
class VideoDecoderSession {
public:
    VideoDecoderSession(CudaDecoder& decoder, Input reader, const Input end)
            : decoder_(RestartDecoder(decoder)),
//              worker_{std::make_unique<std::thread>(&VideoDecoderSession::DecodeAll, std::ref(decoder), reader, end)},
              numberOfWaits_(0),
              nextDataQueue_(100),
              flagsQueue_(100),
              threadPool_(1),
              isDoneReading_(false)
    {
        threadPool_.push(std::bind(&VideoDecoderSession::ReadNext, std::ref(decoder), reader, end, std::ref(nextDataQueue_), std::ref(flagsQueue_), std::ref(queuesMutex_), &isDoneReading_));
        worker_ = std::make_unique<std::thread>(&VideoDecoderSession::DecodeAll, std::ref(decoder), reader, end, std::ref(nextDataQueue_), std::ref(flagsQueue_), std::ref(queuesMutex_), &isDoneReading_);
    }

    VideoDecoderSession(VideoDecoderSession&& other) noexcept
            : decoder_(other.decoder_),
              worker_(std::move(other.worker_)) {
        other.moved_ = true;
    }

    ~VideoDecoderSession() {
        if(!moved_) {
            decoder_.frame_queue().endDecode();
            worker_->join();

            threadPool_.waitAll();

            std::scoped_lock lock(queuesMutex_);
            assert(nextDataQueue_.empty());
            assert(flagsQueue_.empty());
            assert(isDoneReading_);
        }

//        std::cout << "ANALYSIS: number-of-decoder-waits " << numberOfWaits_ << std::endl;
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
                bool gotFrame = false;
                if (decoder_.isDecodingDifferentSizes()) {
                    if (decoder_.decodedPictureQueue().read_available()) {
                        packet = decoder_.decodedPictureQueue().front();
                        decoder_.decodedPictureQueue().pop();
                        gotFrame = true;
                    }
                } else {
                    packet = decoder_.frame_queue().try_dequeue<CUVIDPARSERDISPINFO>();
                    gotFrame = packet != nullptr;
                }
                if (gotFrame) {
                    auto frameNumber = -1;
                    auto tileNumber = -1;
                    if (decoder_.frameNumberQueue().pop(frameNumber)) {
                        decoder_.tileNumberQueue().pop(tileNumber);
                        return {DecodedFrame(decoder_, packet, frameNumber, tileNumber)};
                    } else
                        return {DecodedFrame(decoder_, packet)};
                }
//                ++numberOfWaits_;
            }

        return std::nullopt;
    }

    const CudaDecoder &decoder() const { return decoder_; }

protected:
    CudaDecoder &decoder_;
    std::unique_ptr<std::thread> worker_;
    bool moved_ = false;
    unsigned int numberOfWaits_;

    std::mutex queuesMutex_;
    lightdb::spsc_queue<std::shared_ptr<std::vector<unsigned char>>> nextDataQueue_;
    lightdb::spsc_queue<unsigned long> flagsQueue_;

    static void ReadNext(CudaDecoder &decoder, Input reader, Input end, lightdb::spsc_queue<std::shared_ptr<std::vector<unsigned char>>> &nextDataQueue, lightdb::spsc_queue<unsigned long> &flagsQueue, std::mutex &queuesMutex, std::atomic_bool *isDoneReading) {
        do {
            std::shared_ptr<std::vector<unsigned char>> combinedData(new std::vector<unsigned char>());
            unsigned long flags = 0;
            if (reader != end) {
                for (auto i = 0u; i < 20; ++i) {
                    if (reader != end) {
                        int firstFrameIndex = -1;
                        int numberOfFrames = -1;
                        int tileNumber = -1;
                        bool gotFirstFrameIndex = (*reader).getFirstFrameIndexIfSet(firstFrameIndex);
                        bool gotNumberOfFrames = (*reader).getNumberOfFramesIfSet(numberOfFrames);
                        bool gotTileNumber = (*reader).getTileNumberIfSet(tileNumber);
                        if (gotFirstFrameIndex && gotNumberOfFrames) {
                            for (int i = firstFrameIndex; i < firstFrameIndex + numberOfFrames; i++) {
                                decoder.frameNumberQueue().push(i);

                                if (gotTileNumber)
                                    decoder.tileNumberQueue().push(tileNumber);
                            }
                        }

                        auto packet = static_cast<DecodeReaderPacket>(reader++);
                        combinedData->insert(combinedData->end(), packet.payload, packet.payload + packet.payload_size);
                        flags |= packet.flags;
                    }
                }
                {
                    std::scoped_lock lock(queuesMutex);
                    nextDataQueue.push(combinedData);
                    flagsQueue.push(flags); // It's possible that there will be data but not flags.
                }
            }
        } while (reader != end);
        *isDoneReading = true;
    }

    static void DecodeAll(CudaDecoder &decoder, Input reader, const Input end, lightdb::spsc_queue<std::shared_ptr<std::vector<unsigned char>>> &nextDataQueue, lightdb::spsc_queue<unsigned long> &flagsQueue, std::mutex &queuesMutex, std::atomic_bool *isDoneReading) {
        cuProfilerStart();
        lightdb::Timer timer;
        timer.startSection("DecodeAll");

        CUresult status;

        auto parser = CreateParser(decoder);

        do {
//            std::vector<unsigned char> combinedData;
//            unsigned long flags = 0;
//            if (reader != end) {
//                for (auto i = 0u; i < 80; ++i) {
//                    if (reader != end) {
//                        int firstFrameIndex = -1;
//                        int numberOfFrames = -1;
//                        int tileNumber = -1;
//                        bool gotFirstFrameIndex = (*reader).getFirstFrameIndexIfSet(firstFrameIndex);
//                        bool gotNumberOfFrames = (*reader).getNumberOfFramesIfSet(numberOfFrames);
//                        bool gotTileNumber = (*reader).getTileNumberIfSet(tileNumber);
//                        if (gotFirstFrameIndex && gotNumberOfFrames) {
//                            for (int i = firstFrameIndex; i < firstFrameIndex + numberOfFrames; i++) {
//                                decoder.frameNumberQueue().push(i);
//
//                                if (gotTileNumber)
//                                    decoder.tileNumberQueue().push(tileNumber);
//                            }
//                        }
//
//                        auto packet = static_cast<DecodeReaderPacket>(reader++);
//                        combinedData.insert(combinedData.end(), packet.payload, packet.payload + packet.payload_size);
//                        flags |= packet.flags;
//                    }
//                }

                while (true) {
                    bool readAvailable = false;
                    {
                        std::scoped_lock lock(queuesMutex);
                        readAvailable = flagsQueue.read_available() && nextDataQueue.read_available();
                    }
                    if (readAvailable)
                        break;
                    else
                        std::this_thread::yield();
                }

                auto flags = flagsQueue.front();
                flagsQueue.pop();

                auto combinedData = nextDataQueue.front();
                nextDataQueue.pop();

                nvtxNameOsThread(std::hash<std::thread::id>()(std::this_thread::get_id()), "DECODE");
                nvtxEventAttributes_t eventAttributes;
                memset(&eventAttributes, 0, sizeof(eventAttributes));
                eventAttributes.version = NVTX_VERSION;
                eventAttributes.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
                eventAttributes.colorType = NVTX_COLOR_ARGB;
                eventAttributes.color = 0xFF0000FF;
                eventAttributes.messageType = NVTX_MESSAGE_TYPE_ASCII;
                eventAttributes.message.ascii = "parse packet";
                nvtxRangePushEx(&eventAttributes);

                CUVIDSOURCEDATAPACKET packet;
                memset(&packet, 0, sizeof(packet));
                packet.flags = flags;
                packet.payload_size = combinedData->size();
                packet.payload = combinedData->data();
//            auto packet = static_cast<DecodeReaderPacket>(reader++);
                if ((status = cuvidParseVideoData(parser, &packet)) != CUDA_SUCCESS) {
                    cuvidDestroyVideoParser(parser);
                    throw GpuCudaRuntimeError("Call to cuvidParseVideoData failed", status);
                }
                nvtxRangePop();

            }  while (!decoder.frame_queue().isEndOfDecode() && (!(*isDoneReading) || nextDataQueue.read_available()));

        cuProfilerStop();
        cuvidDestroyVideoParser(parser);
        decoder.frame_queue().endDecode();
        timer.endSection("DecodeAll");
        timer.printAllTimes();
        LOG(INFO) << "Decode complete; thread terminating.";
    }

private:
    ThreadPool threadPool_;
    std::atomic_bool isDoneReading_;

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
            if (!decoder->isDecodingDifferentSizes())
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
            // TODO: This should happen on a separate thread than cuvidDecodePicture() for performance.
            if (decoder->isDecodingDifferentSizes())
                decoder->mapFrame(frame, decoder->currentFormat());
            else
                decoder->frame_queue().enqueue(frame);
        }

        return 1;
    }
};

#endif //LIGHTDB_VIDEODECODERSESSION_H
