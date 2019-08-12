#ifndef LIGHTDB_SCANOPERATORS_H
#define LIGHTDB_SCANOPERATORS_H

#include "MetadataLightField.h"
#include "PhysicalOperators.h"
#include "Rectangle.h"

#include "timer.h"
#include <iostream>

namespace lightdb::physical {

class ScanSingleFileDecodeReader: public PhysicalOperator {
public:
    explicit ScanSingleFileDecodeReader(const LightFieldReference &logical, catalog::Source source)
            : PhysicalOperator(logical, DeviceType::CPU, runtime::make<Runtime>(*this, "ScanSingleFileDecodeReader-init")),
              source_(std::move(source))
    { }

    const catalog::Source &source() const { return source_; }
    const Codec &codec() const { return source_.codec(); }

private:
    class Runtime: public runtime::Runtime<ScanSingleFileDecodeReader> {
    public:
        explicit Runtime(ScanSingleFileDecodeReader &physical)
            : runtime::Runtime<ScanSingleFileDecodeReader>(physical),
              reader_(physical.source().filename())
        {
//            timer_.endSection();
        }

        std::optional<physical::MaterializedLightFieldReference> read() override {
//            timer_.startSection();

            auto packet = reader_.read();
            if (packet.has_value()) {
                 auto returnValue = std::optional<physical::MaterializedLightFieldReference>{
                        physical::MaterializedLightFieldReference::make<CPUEncodedFrameData>(
                                physical().source().codec(),
                                physical().source().configuration(),
                                physical().source().geometry(),
                                packet.value())};
//                 timer_.endSection();
                 return returnValue;
            } else {
//                timer_.endSection();
//                std::cout << "ANALYSIS ScanSingleFileDecodeReader took " << timer_.totalTimeInMillis() << " ms\n";
                return std::nullopt;
            }
        }

        FileDecodeReader reader_;
    private:
//        Timer timer_;
    };

    const catalog::Source source_;
};

class ScanFramesFromFileEncodedReader: public PhysicalOperator {
public:
    explicit ScanFramesFromFileEncodedReader(const LightFieldReference &logical, catalog::Source source)
        : PhysicalOperator(logical, DeviceType::CPU, runtime::make<Runtime>(*this, "ScanFramesFromFileEncodedReader-init")),
        source_(std::move(source))
    { }

    const catalog::Source &source() const { return source_; }

private:
    class Runtime: public runtime::Runtime<ScanFramesFromFileEncodedReader> {
    public:
        explicit Runtime(ScanFramesFromFileEncodedReader &physical)
            : runtime::Runtime<ScanFramesFromFileEncodedReader>(physical),
                    frameReader_(physical.source().filename(), {})
        {}

        std::optional<physical::MaterializedLightFieldReference> read() override {
            GLOBAL_TIMER.startSection("ScanFramesFromFileEncodedReader");
            std::optional<physical::MaterializedLightFieldReference> returnVal = {};
            auto gopPacket = frameReader_.read();
            if (gopPacket.has_value()) {
                auto cpuData = MaterializedLightFieldReference::make<CPUEncodedFrameData>(
                        physical().source().codec(),
                        physical().source().configuration(),
                        physical().source().geometry(),
                        gopPacket->data());

                cpuData.downcast<CPUEncodedFrameData>().setFirstFrameIndexAndNumberOfFrames(gopPacket->firstFrameIndex(), gopPacket->numberOfFrames());
                returnVal = {cpuData};
            }
            GLOBAL_TIMER.endSection("ScanFramesFromFileEncodedReader");
            return returnVal;
        }
    private:
        EncodedFrameReader frameReader_;
    };

    const catalog::Source source_;
};

class ScanFramesFromFileDecodeReader: public PhysicalOperator {
public:
    explicit ScanFramesFromFileDecodeReader(const LightFieldReference& logical, catalog::Source source)
        : PhysicalOperator(logical, DeviceType::CPU, runtime::make<Runtime>(*this, "ScanFramesFromFileDecodeReader-init")),
        source_(std::move(source))
    { }

    const catalog::Source &source() const { return source_; }

private:
    class Runtime: public runtime::Runtime<ScanFramesFromFileDecodeReader> {
    public:
        explicit Runtime(ScanFramesFromFileDecodeReader &physical)
            : runtime::Runtime<ScanFramesFromFileDecodeReader>(physical),
                    frameReader_(physical.source().filename(), {})
        { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            GLOBAL_TIMER.startSection("ScanFramesFromFileDecodeReader");
            auto packet = frameReader_.read();
            if (packet.has_value()) {
                // FIXME: implement this.
                auto returnVal = std::optional<physical::MaterializedLightFieldReference>{
                    physical::MaterializedLightFieldReference::make<CPUEncodedFrameData>(
                            physical().source().codec(),
                            physical().source().configuration(),
                            physical().source().geometry(),
                            packet.value())};
                GLOBAL_TIMER.endSection("ScanFramesFromFileDecodeReader");
                return returnVal;
            } else {
                return std::nullopt;
            }
        }

    private:
        FrameDecodeReader frameReader_;
    };

    const catalog::Source source_;

};

class ScanSingleBoxesFile: public PhysicalOperator {
public:
    explicit ScanSingleBoxesFile(const LightFieldReference &logical, catalog::Source source)
        : PhysicalOperator(logical, DeviceType::CPU, runtime::make<Runtime>(*this, "ScanSingleBoxesFile-init")),
        source_(std::move(source))
    { }

    const catalog::Source &source() const { return source_; }
    const Codec &codec() const { return source_.codec(); }

private:
    class Runtime: public runtime::Runtime<ScanSingleBoxesFile> {
    public:
        explicit Runtime(ScanSingleBoxesFile &physical)
            : runtime::Runtime<ScanSingleBoxesFile>(physical),
                    reader_(physical.source().filename(), std::ios::binary)
            { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            GLOBAL_TIMER.startSection("ScanSingleBoxesFile");
            if (!reader_.eof()) {
                char numDigits;
                reader_.get(numDigits);

                std::string sizeAsString;
                for (int i = 0; i < (int)numDigits; i++) {
                    char next;
                    reader_.get(next);
                    sizeAsString += next;
                }

                int size = std::stoi(sizeAsString);
//
//                auto realSize = 234; // Number of rectangles in dob_boxes.boxes.
                std::vector<Rectangle> rectangles(size);
                reader_.read(reinterpret_cast<char *>(rectangles.data()), size * sizeof(Rectangle));
                reader_.get(numDigits);
                assert(reader_.eof());

                auto returnValue = MetadataLightField({{"labels", rectangles}}, physical().source().configuration(), physical().source().geometry()).ref();
                GLOBAL_TIMER.endSection("ScanSingleBoxesFile");
                return {returnValue};
            } else {
                GLOBAL_TIMER.endSection("ScanSingleBoxesFile");
                return std::nullopt;
            }
        }
        std::ifstream reader_;
    };

    const catalog::Source source_;
};

template<size_t Size=131072>
class ScanSingleFile: public PhysicalOperator {
public:
    explicit ScanSingleFile(const LightFieldReference &logical, catalog::Source source)
            : PhysicalOperator(logical, DeviceType::CPU, runtime::make<Runtime>(*this, "ScanSingleFile-init")),
              source_(std::move(source))
    { }

    const catalog::Source &source() const { return source_; }
    const Codec &codec() const { return source_.codec(); }

private:
    class Runtime: public runtime::Runtime<ScanSingleFile<Size>> {
    public:
        explicit Runtime(ScanSingleFile &physical)
            : runtime::Runtime<ScanSingleFile<Size>>(physical),
              buffer_(Size, 0),
              reader_(physical.source().filename())
        { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            if(!reader_.eof()) {
                reader_.read(buffer_.data(), buffer_.size());
                return {CPUEncodedFrameData(
                        this->physical().source().codec(),
                        this->physical().source().configuration(),
                        this->physical().source().geometry(),
                        buffer_)};

            } else {
                reader_.close();
                return {};
            }
        }

    private:
        lightdb::bytestring buffer_;
        std::ifstream reader_;
    };

    const catalog::Source source_;
};

class ScanEntireFile: public PhysicalOperator {
public:
    explicit ScanEntireFile(const LightFieldReference &logical, catalog::Source source)
        : PhysicalOperator(logical, DeviceType::CPU, runtime::make<Runtime>(*this, "ScanEntireFile-init")),
        source_(std::move(source))
    {}

    const catalog::Source &source() const { return source_; }

private:
    class Runtime: public runtime::Runtime<ScanEntireFile> {
    public:
        explicit Runtime(ScanEntireFile &physical)
            : runtime::Runtime<ScanEntireFile>(physical),
            reader_(physical.source().filename(), std::ios::binary)
        {}

        std::optional<MaterializedLightFieldReference> read() override {
            if (reader_.eof()) {
                reader_.close();
                return {};
            } else {
                GLOBAL_TIMER.startSection("ScanEntireFile");
                reader_.seekg(0, std::ios::end);
                int size = reader_.tellg();
                reader_.seekg(0, std::ios::beg);

                bytestring fileData(size);
//                fileData.reserve(size);

                reader_.read(fileData.data(), size);
                assert(reader_.good());
                assert(!reader_.fail());

                if (!reader_.eof()) {
                    reader_.get();
                    assert(reader_.eof());
                }
                auto returnVal = CPUEncodedFrameData(
                        physical().source().codec(),
                        physical().source().configuration(),
                        physical().source().geometry(),
                        fileData);
                GLOBAL_TIMER.endSection("ScanEntireFile");
                return {returnVal};
            }
        }


    private:
        std::ifstream reader_;
    };

    const catalog::Source source_;
};

} // namespace lightdb::physical

#endif //LIGHTDB_SCANOPERATORS_H
