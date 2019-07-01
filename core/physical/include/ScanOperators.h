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
            : PhysicalOperator(logical, DeviceType::CPU, runtime::make<Runtime>(*this)),
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
        { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            timer_.startSection();

            auto packet = reader_.read();
            if (packet.has_value()) {
                 auto returnValue = std::optional<physical::MaterializedLightFieldReference>{
                        physical::MaterializedLightFieldReference::make<CPUEncodedFrameData>(
                                physical().source().codec(),
                                physical().source().configuration(),
                                physical().source().geometry(),
                                packet.value())};
                 timer_.endSection();
                 return returnValue;
            } else {
                std::cout << "ANALYSIS ScanSingleFileDecodeReader took " << timer_.totalTimeInMillis() << " ms" << std::endl;
                return std::nullopt;
            }
        }

        FileDecodeReader reader_;
    private:
        Timer timer_;
    };

    const catalog::Source source_;
};

class ScanSingleBoxesFile: public PhysicalOperator {
public:
    explicit ScanSingleBoxesFile(const LightFieldReference &logical, catalog::Source source)
        : PhysicalOperator(logical, DeviceType::CPU, runtime::make<Runtime>(*this)),
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

                return {MetadataLightField({{"labels", rectangles}}, physical().source().configuration(), physical().source().geometry()).ref()};
            } else
                return std::nullopt;
        }
        std::ifstream reader_;
    };

    const catalog::Source source_;
};

template<size_t Size=131072>
class ScanSingleFile: public PhysicalOperator {
public:
    explicit ScanSingleFile(const LightFieldReference &logical, catalog::Source source)
            : PhysicalOperator(logical, DeviceType::CPU, runtime::make<Runtime>(*this)),
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

} // namespace lightdb::physical

#endif //LIGHTDB_SCANOPERATORS_H
