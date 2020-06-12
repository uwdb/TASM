#ifndef TASM_DECODEREADER_H
#define TASM_DECODEREADER_H

#include "GPUContext.h"
#include "MP4Reader.h"
#include "spsc_queue.h"

#include "nvcuvid.h"
#include <experimental/filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

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
              packets_(std::make_unique<tasm::spsc_queue<DecodeReaderPacket>>(4096)), // Must be initialized before source
    source_(CreateVideoSource(filename)),
            format_(GetVideoSourceFormat(source_)),
            decoded_bytes_(0) {
        if(format().codec != cudaVideoCodec_H264 && format().codec != cudaVideoCodec_HEVC)
            throw std::runtime_error("FileDecodeReader only supports H264/HEVC input video");
        else if(format().chroma_format != cudaVideoChromaFormat_420)
            throw std::runtime_error("FileDecodeReader only supports 4:2:0 chroma");
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
            std::cerr << "Swallowed CompleteVideo failure" << std::endl;
        else if((status = cuvidSetVideoSourceState(source_, cudaVideoState_Stopped)) != CUDA_SUCCESS)
            std::cerr << "Swallowed cuvidSetVideoSourceState failure (" << status << ')' << std::endl;
        else if((status = cuvidDestroyVideoSource(source_)) != CUDA_SUCCESS)
            std::cerr << "Swallowed cuvidDestroyVideoSource failure (" << status << ')' << std::endl;
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
            std::cout << "Decoded " << decoded_bytes_ << " bytes from " << filename() << std::endl;
            return {};
        }
    }

    inline bool isComplete() const override {
        return !packets_->read_available() && cuvidGetVideoSourceState(source_) != cudaVideoState_Started;
    }

private:
    static int CUDAAPI HandleVideoData(void *userData, CUVIDSOURCEDATAPACKET *packet) {
        auto *packets = static_cast<tasm::spsc_queue<DecodeReaderPacket>*>(userData);

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
            throw std::runtime_error("File does not exist: " + std::string(filename));
        else if(GPUContext::device_count() == 0)
            throw std::runtime_error("No CUDA device was found");
        if((status = cuvidCreateVideoSource(&source, filename, &videoSourceParameters)) != CUDA_SUCCESS)
            throw std::runtime_error("Call to cuvidCreateVideoSource failed: " + std::to_string(status));
        else if((status = cuvidSetVideoSourceState(source, cudaVideoState_Started)) != CUDA_SUCCESS)
            throw std::runtime_error("Call to cuvidSetVideoSourceState failed: " + std::to_string(status));

        return source;
    }

    CUVIDEOFORMAT GetVideoSourceFormat(CUvideosource source) {
        CUresult status;
        CUVIDEOFORMAT format;

        if((status = cuvidGetSourceVideoFormat(source, &format, 0)) != CUDA_SUCCESS)
            throw std::runtime_error("Call to cuvidGetSourceVideoFormat failed: " + std::to_string(status));
        return format;
    }

    bool CompleteVideo() {
        packets_->reset();
        return true;
    }

    std::string filename_;
    std::unique_ptr<tasm::spsc_queue<DecodeReaderPacket>> packets_;
    CUvideosource source_;
    CUVIDEOFORMAT format_;
    size_t decoded_bytes_;
};

struct GOPReaderPacket {
public:
    explicit GOPReaderPacket(std::vector<char> data, unsigned int firstFrameIndex, unsigned int numberOfFrames)
            : data_(std::make_unique<std::vector<char>>(data)),
              firstFrameIndex_(firstFrameIndex),
              numberOfFrames_(numberOfFrames)
    {}

    explicit GOPReaderPacket(std::unique_ptr<std::vector<char>> data, unsigned int firstFrameIndex, unsigned int numberOfFrames)
            : data_(std::move(data)),
              firstFrameIndex_(firstFrameIndex),
              numberOfFrames_(numberOfFrames)
    { }

    std::unique_ptr<std::vector<char>> &data() { return data_; }
    unsigned int firstFrameIndex() const { return firstFrameIndex_; }
    unsigned int numberOfFrames() const { return numberOfFrames_; }

public:
    std::unique_ptr<std::vector<char>> data_;
    unsigned int firstFrameIndex_;
    unsigned int numberOfFrames_;
};

class EncodedFrameReader {
public:
    // Assume frames is sorted.
    // Frames is in global frame numbers (e.g. starting from frameOffsetInFile, not 0).
    explicit EncodedFrameReader(std::experimental::filesystem::path filename, std::vector<int> frames, int frameOffsetInFile = 0, bool shouldReadEntireGOPs=false)
            : filename_(std::move(filename)),
              mp4Reader_(filename_),
              frames_(std::move(frames)),
              numberOfSamplesRead_(0),
              shouldReadFramesExactly_(false),
              frameOffsetInFile_(frameOffsetInFile),
              shouldReadEntireGOPs_(shouldReadEntireGOPs)
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

    void setNewFileWithSameKeyframes(const std::experimental::filesystem::path &newFilename) {
        mp4Reader_.setNewFileWithSameKeyframes(newFilename);

        frameIterator_ = frames_.begin();
        keyframeIterator_ = mp4Reader_.keyframeNumbers().begin();
    }

    void setNewFileWithSameKeyframesButNewFrames(const std::experimental::filesystem::path &newFilename, std::vector<int> frames, int frameOffsetInFile) {
        filename_ = newFilename;
        mp4Reader_.setNewFileWithSameKeyframes(newFilename);
        frames_ = std::move(frames);
        frameOffsetInFile_ = frameOffsetInFile;

        if (frameOffsetInFile) {
            std::for_each(frames_.begin(), frames_.end(), [&](auto &frame) {
                frame -= frameOffsetInFile;
            });
        }

        frameIterator_ = frames_.begin();
        keyframeIterator_ = mp4Reader_.keyframeNumbers().begin();
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

            if (shouldReadEntireGOPs_) {
                // If there are more keyframes, then read to before the next one.
                if (haveMoreKeyframes()) {
                    lastSampleToRead = MP4Reader::frameNumberToSampleNumber(*keyframeIterator_ - 1);
                } else {
                    lastSampleToRead = mp4Reader_.numberOfSamples();
                }
            }

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

    std::experimental::filesystem::path filename_;
    MP4Reader mp4Reader_;
    std::vector<int> frames_;
    std::vector<int>::iterator frameIterator_;
    std::vector<int>::const_iterator keyframeIterator_;
    unsigned int numberOfSamplesRead_;
    std::vector<int> globalFrames_;
    std::vector<int>::const_iterator globalFramesIterator_;
    bool shouldReadFramesExactly_;
    int frameOffsetInFile_;
    bool shouldReadEntireGOPs_;
};


#endif //TASM_DECODEREADER_H
