#ifndef LIGHTDB_DECODEREADER_H
#define LIGHTDB_DECODEREADER_H

#include "spsc_queue.h"
#include "Stitcher.h"
#include "nvcuvid.h"
#include <thread>
#include <experimental/filesystem>

//#include "gpac/isomedia.h"
//#include "gpac/internal/isomedia_dev.h"
//#include "gpac/list.h"

#include "MP4Reader.h"
#include <iostream>


struct DecodeReaderPacket: public CUVIDSOURCEDATAPACKET {
public:
    DecodeReaderPacket() : CUVIDSOURCEDATAPACKET{} { }

    DecodeReaderPacket(const DecodeReaderPacket &packet) = default;

    explicit DecodeReaderPacket(const CUVIDSOURCEDATAPACKET &packet)
        : CUVIDSOURCEDATAPACKET{packet.flags, packet.payload_size, nullptr, packet.timestamp},
          buffer_(std::make_shared<std::vector<unsigned char>>()) {
        buffer_->reserve(payload_size);
        buffer_->insert(buffer_->begin(), packet.payload, packet.payload + payload_size);
        payload = buffer_->data();
    }

    explicit DecodeReaderPacket(const std::vector<char> &data, const unsigned long flags=0,
                                const CUvideotimestamp timestamp=0)
            : DecodeReaderPacket(CUVIDSOURCEDATAPACKET{flags, data.size(),
                                                       reinterpret_cast<const unsigned char*>(data.data()), timestamp})
    { }

    DecodeReaderPacket& operator=(const DecodeReaderPacket &packet) = default;
    bool operator==(const DecodeReaderPacket &packet) const noexcept {
        return this->payload_size == packet.payload_size &&
               this->flags == packet.flags &&
               this->timestamp == packet.timestamp &&
               this->buffer_ == packet.buffer_;
    }

private:
    std::shared_ptr<std::vector<unsigned char>> buffer_;
};

class DecodeReader {
public:
    virtual ~DecodeReader() { }

    class iterator {
        friend class DecodeReader;

    public:
        bool operator==(const iterator& other) const { return (eos_ && other.eos_) ||
                                                              (reader_ == other.reader_ &&
                                                               *current_ == *other.current_); }
        bool operator!=(const iterator& other) const { return !(*this == other); }
        void operator++()
        {
            if (!(current_ = reader_->read()).has_value())
                eos_ = true;
        }
        const DecodeReaderPacket operator++(int)
        {
            auto value = **this;
            ++*this;
            return value;
        }
        DecodeReaderPacket operator*() { return current_.value(); }

    protected:
        explicit iterator(DecodeReader &reader)
                : reader_(&reader), current_({reader.read()}), eos_(false)
        { }
        constexpr explicit iterator()
                : reader_(nullptr), current_(), eos_(true)
        { }

    private:
        DecodeReader *reader_;
        std::optional<DecodeReaderPacket> current_;
        bool eos_;
    };

    virtual iterator begin() { return iterator(*this); }
    virtual iterator end() { return iterator(); }

    virtual std::optional<DecodeReaderPacket> read() = 0;
    virtual CUVIDEOFORMAT format() const = 0;
    virtual bool isComplete() const = 0;
};

class FileDecodeReader: public DecodeReader {
public:
    explicit FileDecodeReader(const std::string &filename)
        : FileDecodeReader(filename.c_str()) { }

    explicit FileDecodeReader(const char *filename)
            : filename_(filename),
              packets_(std::make_unique<lightdb::spsc_queue<DecodeReaderPacket>>(4096)), // Must be initialized before source
              source_(CreateVideoSource(filename)),
              format_(GetVideoSourceFormat(source_)),
              decoded_bytes_(0) {
        if(format().codec != cudaVideoCodec_H264 && format().codec != cudaVideoCodec_HEVC)
            throw GpuRuntimeError("FileDecodeReader only supports H264/HEVC input video");
        else if(format().chroma_format != cudaVideoChromaFormat_420)
            throw GpuRuntimeError("FileDecodeReader only supports 4:2:0 chroma");
    }

    FileDecodeReader(const FileDecodeReader&) = delete;
    FileDecodeReader(FileDecodeReader&& other) noexcept
        : filename_(std::move(other.filename_)),
          packets_(std::move(other.packets_)),
          source_(other.source_),
          format_(other.format_),
          decoded_bytes_(other.decoded_bytes_) {
          other.source_ = nullptr;
    }

    ~FileDecodeReader() {
        CUresult status;

        if(source_ == nullptr)
            ;
        else if(!CompleteVideo())
            LOG(ERROR) << "Swallowed CompleteVideo failure";
        else if((status = cuvidSetVideoSourceState(source_, cudaVideoState_Stopped)) != CUDA_SUCCESS)
            LOG(ERROR) << "Swallowed cuvidSetVideoSourceState failure (" << status << ')';
        else if((status = cuvidDestroyVideoSource(source_)) != CUDA_SUCCESS)
            LOG(ERROR) << "Swallowed cuvidDestroyVideoSource failure (" << status << ')';
        else
            source_ = nullptr;
    }

    inline CUVIDEOFORMAT format() const override { return format_; }
    inline const std::string &filename() const { return filename_; }

    std::optional<DecodeReaderPacket> read() override {
        DecodeReaderPacket packet;

        while (cuvidGetVideoSourceState(source_) == cudaVideoState_Started &&
                !packets_->read_available())
            std::this_thread::yield();

        if(packets_->pop(packet)) {
            decoded_bytes_ += packet.payload_size;
            return {packet};
        } else {
            LOG(INFO) << "Decoded " << decoded_bytes_ << " bytes from " << filename();
            return {};
        }
    }

    inline bool isComplete() const override {
        return !packets_->read_available() && cuvidGetVideoSourceState(source_) != cudaVideoState_Started;
    }

private:
    static int CUDAAPI HandleVideoData(void *userData, CUVIDSOURCEDATAPACKET *packet) {
        auto *packets = static_cast<lightdb::spsc_queue<DecodeReaderPacket>*>(userData);

        while(!packets->push(DecodeReaderPacket(*packet)))
            std::this_thread::yield();

        return 1;
    }

    CUvideosource CreateVideoSource(const char *filename) {
        CUresult status;
        CUvideosource source;
        CUVIDSOURCEPARAMS videoSourceParameters = {
                .ulClockRate = 0,
                .uReserved1 = {},
                .pUserData = packets_.get(),
                .pfnVideoDataHandler = HandleVideoData,
                .pfnAudioDataHandler = nullptr,
                {nullptr}
        };

        if(!std::experimental::filesystem::exists(filename))
            throw InvalidArgumentError("File does not exist", "filename");
        else if(GPUContext::device_count() == 0)
            throw GpuCudaRuntimeError("No CUDA device was found", CUDA_ERROR_NOT_INITIALIZED);
        if((status = cuvidCreateVideoSource(&source, filename, &videoSourceParameters)) != CUDA_SUCCESS)
            throw GpuCudaRuntimeError("Call to cuvidCreateVideoSource failed", status);
        else if((status = cuvidSetVideoSourceState(source, cudaVideoState_Started)) != CUDA_SUCCESS)
            throw GpuCudaRuntimeError("Call to cuvidSetVideoSourceState failed", status);

        return source;
    }

    CUVIDEOFORMAT GetVideoSourceFormat(CUvideosource source) {
        CUresult status;
        CUVIDEOFORMAT format;

        if((status = cuvidGetSourceVideoFormat(source, &format, 0)) != CUDA_SUCCESS)
            throw GpuCudaRuntimeError("Call to cuvidGetSourceVideoFormat failed", status);
        return format;
    }

    bool CompleteVideo() {
        packets_->reset();
        return true;
    }

    std::string filename_;
    std::unique_ptr<lightdb::spsc_queue<DecodeReaderPacket>> packets_;
    CUvideosource source_;
    CUVIDEOFORMAT format_;
    size_t decoded_bytes_;
};

struct GOPReaderPacket {
public:
    explicit GOPReaderPacket(lightdb::bytestring data, unsigned int firstFrameIndex, unsigned int numberOfFrames)
            : data_(std::make_unique<lightdb::bytestring>(data)),
              firstFrameIndex_(firstFrameIndex),
              numberOfFrames_(numberOfFrames)
    {}

    explicit GOPReaderPacket(std::unique_ptr<lightdb::bytestring> data, unsigned int firstFrameIndex, unsigned int numberOfFrames)
        : data_(std::move(data)),
        firstFrameIndex_(firstFrameIndex),
        numberOfFrames_(numberOfFrames)
    { }

    std::unique_ptr<lightdb::bytestring> &data() { return data_; }
    unsigned int firstFrameIndex() const { return firstFrameIndex_; }
    unsigned int numberOfFrames() const { return numberOfFrames_; }

private:
    std::unique_ptr<lightdb::bytestring> data_;
    unsigned int firstFrameIndex_;
    unsigned int numberOfFrames_;
};

class BlackFrameReader {
public:
    explicit BlackFrameReader(std::filesystem::path filename, std::vector<int> frames, int frameOffsetInFile = 0)
        : filename_(std::move(filename)),
        mp4Reader_(filename_),
        frameRetriever_(mp4Reader_.dataForSamples(1, 1), mp4Reader_.dataForSamples(2, 2)),
        frames_(std::move(frames)),
        frameOffsetInFile_(frameOffsetInFile)
    {
        if (frameOffsetInFile) {
            std::for_each(frames_.begin(), frames_.end(), [&](auto &frame) {
                frame -= frameOffsetInFile;
            });
        }

        frameIterator_ = frames_.begin();
        keyframeIterator_ = mp4Reader_.keyframeNumbers().begin();
    }

    void setNewFrames(std::vector<int> frames) {
        frames_ = std::move(frames);
        frameIterator_ = frames_.begin();
    }

    bool isEos() const { return frameIterator_ == frames_.end(); }

    std::optional<GOPReaderPacket> read() {
        if (frameIterator_ == frames_.end())
            return {};

        // Get number of frames to read.
        unsigned int firstFrameToRead = 0;
        unsigned int lastFrameToRead = 0;
        while (haveMoreKeyframes() && *keyframeIterator_ <= *frameIterator_)
            ++keyframeIterator_;

        if (!haveMoreKeyframes())
            frameIterator_ = frames_.end();
        else {
            while (haveMoreFrames() && *frameIterator_ < *keyframeIterator_)
                ++frameIterator_;
        }

        firstFrameToRead = *std::prev(keyframeIterator_);
        lastFrameToRead = *std::prev(frameIterator_);
        auto numberOfFramesToRead = lastFrameToRead - firstFrameToRead + 1;
        return std::make_optional<GOPReaderPacket>(dataForFrames(numberOfFramesToRead), firstFrameToRead + frameOffsetInFile_, numberOfFramesToRead);
    }

private:
    bool haveMoreFrames() const { return frameIterator_ != frames_.end(); }
    bool haveMoreKeyframes() const { return keyframeIterator_ != mp4Reader_.keyframeNumbers().end(); }

    std::unique_ptr<lightdb::bytestring> dataForFrames(unsigned int numberOfFramesToRead) {
        if (!numberOfFramesToRead)
            return {};

        // Compute size of output.
        auto size = frameRetriever_.iFrameData().size()
                        + (numberOfFramesToRead - 1) * frameRetriever_.pFrameData().size()
                        + (numberOfFramesToRead - 1) * frameRetriever_.pFrameHeaderForPicOrder(1).size();

        std::unique_ptr<lightdb::bytestring> output(new lightdb::bytestring);
        output->reserve(size);

        output->insert(output->end(), frameRetriever_.iFrameData().begin(), frameRetriever_.iFrameData().end());
        for (auto i = 1u; i < numberOfFramesToRead; ++i) {
            auto &updatedHeader = frameRetriever_.pFrameHeaderForPicOrder(i);
            output->insert(output->end(), updatedHeader.begin(), updatedHeader.end());
            output->insert(output->end(), frameRetriever_.pFrameData().begin(), frameRetriever_.pFrameData().end());
        }

        return output;
    }

    std::filesystem::path filename_;
    MP4Reader mp4Reader_;
    lightdb::hevc::IdenticalFrameRetriever frameRetriever_;

    std::vector<int> frames_;
    std::vector<int>::iterator frameIterator_;
    std::vector<int>::const_iterator keyframeIterator_;
    int frameOffsetInFile_;
};

class EncodedFrameReader {
public:
    // Assume frames is sorted.
    // Frames is in global frame numbers (e.g. starting from frameOffsetInFile, not 0).
    explicit EncodedFrameReader(std::filesystem::path filename, std::vector<int> frames, int frameOffsetInFile = 0)
        : filename_(std::move(filename)),
        mp4Reader_(filename_),
        frames_(std::move(frames)),
        numberOfSamplesRead_(0),
        shouldReadFramesExactly_(false),
        frameOffsetInFile_(frameOffsetInFile)
    {
        // Update frames to be 0-indexed.
        if (frameOffsetInFile) {
            std::for_each(frames_.begin(), frames_.end(), [&](auto &frame) {
                frame -= frameOffsetInFile;
            });
        }

        frameIterator_ = frames_.begin(); // In global frame numbers.
        keyframeIterator_ = mp4Reader_.keyframeNumbers().begin(); // 0-indexed.
    }

    void setGlobalFrames(const std::vector<int> &globalFrames) {
        globalFrames_ = globalFrames;
        globalFramesIterator_ = globalFrames_.begin();
    }

    void setNewFrames(std::vector<int> frames) {
        frames_ = std::move(frames);
        frameIterator_ = frames_.begin();
    }

    void setShouldReadFramesExactly(bool shouldReadFramesExactly) {
        shouldReadFramesExactly_ = shouldReadFramesExactly;
    }

    bool isEos() const { return frameIterator_ == frames_.end(); }

    std::optional<GOPReaderPacket> read() {
        // If we are reading all of the frames, return the frames for the next GOP.
        if (frameIterator_ == frames_.end())
            return {};

        // Unideal, but frames_ is 0-indexed, but keyframes are 1-indexed because they are sample numbers.
        unsigned int firstSampleToRead = 0;
        unsigned int lastSampleToRead = 0;
        if (mp4Reader_.allFramesAreKeyframes()) {
            // Read sequential portion of frames.
            firstSampleToRead = MP4Reader::frameNumberToSampleNumber(*frameIterator_);

            auto lastFrameIndex = *frameIterator_++;
            while (frameIterator_ != frames_.end() && *frameIterator_ == lastFrameIndex + 1)
                lastFrameIndex = *frameIterator_++;

            lastSampleToRead = MP4Reader::frameNumberToSampleNumber(*std::prev(frameIterator_));
        } else {
            // Find GOP that the current frame is in.
            while (haveMoreKeyframes() &&
                   *keyframeIterator_ <= *frameIterator_)
                keyframeIterator_++;

            // Now keyframeIterator is pointing to the keyframe of the next GOP.
            // Find the rest of the frames that we want and are in the same GOP.
            int firstFrame = -1;
            if (shouldReadFramesExactly_)
                firstFrame = *frameIterator_;

            if (!haveMoreKeyframes())
                frameIterator_ = frames_.end();
            else {
                while (haveMoreFrames() && *frameIterator_ < *keyframeIterator_)
                    frameIterator_++;
            }

            // Now frameIterator_ is point to one past the last frame we are interested in.
            // Read frames from the previous GOP to the last frame we are interested in.
            firstSampleToRead = MP4Reader::frameNumberToSampleNumber(shouldReadFramesExactly_ ? firstFrame : *std::prev(keyframeIterator_));
            lastSampleToRead = MP4Reader::frameNumberToSampleNumber(*std::prev(frameIterator_));

            if (globalFrames_.size()) {
                // Finish reading frames in this GOP, even if they aren't in the frames list.
                // Make the last sample the last global frame in this GOP.
                // If there are no more keyframes, we're in the last GOP, so the last sample to read is the last frame in the global frames.
                if (!haveMoreKeyframes())
                    lastSampleToRead = MP4Reader::frameNumberToSampleNumber(globalFrames_.back());
                else {
                    // Move the global frames iterator forward until it's no longer in this GOP.
                    while (haveMoreGlobalFrames() && *globalFramesIterator_ < *keyframeIterator_)
                        globalFramesIterator_++;

                    lastSampleToRead = MP4Reader::frameNumberToSampleNumber(*std::prev(globalFramesIterator_));
                }
            }

        }

        numberOfSamplesRead_ += lastSampleToRead - firstSampleToRead + 1;
        return dataForSamples(firstSampleToRead, lastSampleToRead);
    }

//    ~EncodedFrameReader() {
//        std::cout << "Number of samples read: " << numberOfSamplesRead_ << std::endl;
//    }

private:
    bool haveMoreFrames() const { return frameIterator_ != frames_.end(); }
    bool haveMoreKeyframes() const { return keyframeIterator_ != mp4Reader_.keyframeNumbers().end(); }
    bool haveMoreGlobalFrames() const { return globalFramesIterator_ != globalFrames_.end(); }

    std::optional<GOPReaderPacket> dataForSamples(unsigned int firstSampleToRead, unsigned int lastSampleToRead) {
        // -1 from firstSampleToRead to go from sample number -> index.
        return { GOPReaderPacket(mp4Reader_.dataForSamples(firstSampleToRead, lastSampleToRead), MP4Reader::sampleNumberToFrameNumber(firstSampleToRead + frameOffsetInFile_), lastSampleToRead - firstSampleToRead + 1) };
    }

    std::filesystem::path filename_;
    MP4Reader mp4Reader_;
    std::vector<int> frames_;
    std::vector<int>::iterator frameIterator_;
    std::vector<int>::const_iterator keyframeIterator_;
    unsigned int numberOfSamplesRead_;
    std::vector<int> globalFrames_;
    std::vector<int>::const_iterator globalFramesIterator_;
    bool shouldReadFramesExactly_;
    int frameOffsetInFile_;
};

class FrameDecodeReader: public DecodeReader {
public:
    explicit FrameDecodeReader(std::filesystem::path filename, std::vector<unsigned int> frames)
        : filename_(std::move(filename)),
        frames_(std::move(frames)),
        frameIterator_(frames_.begin()),
        currentKeyframeSampleNumber_(-1),
        decodedBytes_(0)
    {
        // In order to get the headers, mode cannot be READ_DUMP, and the extract mode must be set.
        {
            std::scoped_lock lock(lightdb::GPAC_MUTEX);
            file_ = gf_isom_open(filename_.c_str(), GF_ISOM_OPEN_READ, nullptr);
            assert(gf_isom_set_nalu_extract_mode(file_, 1, GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG |
                                                           GF_ISOM_NALU_EXTRACT_ANNEXB_FLAG) == GF_OK);
        }

        frames_ = {};
        frameIterator_ = frames_.begin();
    }

    ~FrameDecodeReader() {
        std::scoped_lock lock(lightdb::GPAC_MUTEX);
        gf_isom_close(file_);
    }

    std::optional<DecodeReaderPacket> read() override {
        auto dataFromNextGOP = framesForNextGOP_();
        if (dataFromNextGOP.has_value()) {
            unsigned long flags = CUVID_PKT_DISCONTINUITY;
            if (frameIterator_ == frames_.end())
                flags |= CUVID_PKT_ENDOFSTREAM;

            return DecodeReaderPacket(dataFromNextGOP.value(), flags);
        } else
            return {};
    }

    // FIXME: Is this necessary for the frame reader?
    CUVIDEOFORMAT format() const override {
        CUVIDEOFORMAT format;
        return format;
    }

    // FIXME: implement this.
    bool isComplete() const override {
        // FIXME: This isn't right.
        return frameIterator_ == frames_.end();
    }

private:
    std::optional<lightdb::bytestring> framesForNextGOP_() {
        return {};
    }

    std::filesystem::path filename_;
    std::vector<unsigned int> frames_;
    std::vector<unsigned int>::const_iterator frameIterator_;
    long currentKeyframeSampleNumber_;
    unsigned long decodedBytes_;
    GF_ISOFile *file_;
};

#endif //LIGHTDB_DECODEREADER_H
