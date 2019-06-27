#ifndef LIGHTDB_SAVEOPERATORS_H
#define LIGHTDB_SAVEOPERATORS_H

#include "PhysicalOperators.h"

namespace lightdb::physical {

class SaveBoxes: public PhysicalOperator {
public:
//        explicit SaveBoxes(const LightFieldReference &logical)
//            : PhysicalOperator(logical, {}, physical::DeviceType::CPU, runtime::make<Runtime>(*this))
//        { }

    explicit SaveBoxes(const LightFieldReference &logical, PhysicalOperatorReference &parent)
            : PhysicalOperator(logical, {parent}, physical::DeviceType::CPU, runtime::make<Runtime>(*this))
    { }

private:
    class Runtime: public runtime::Runtime<> {
    public:
        explicit Runtime(PhysicalOperator &physical)
                : runtime::Runtime<>(physical)
        { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            if (!all_parent_eos()) {
                assert(iterators().size() == 1);
                MaterializedLightFieldReference input = *(iterators().front());
                assert(input.is<MetadataLightField>());
                MetadataLightField metadataLightField = input.downcast<MetadataLightField>();

                Metadata lfMetadata = metadataLightField.metadata();
                std::for_each(lfMetadata.begin(), lfMetadata.end(), [&](auto labelAndRectangles) {
                    std::vector<Rectangle> &rectanglesForLabel = metadata_[labelAndRectangles.first];
                    rectanglesForLabel.insert(rectanglesForLabel.end(), labelAndRectangles.second.begin(), labelAndRectangles.second.end());
                });

//                std::vector<Rectangle> rectangles = metadataLightField.allRectangles();
//                rectangles_.insert(rectangles_.end(), rectangles.begin(), rectangles.end());
                return iterators().front()++;
            } else {
                // Write rectangles to file.
                std::filesystem::path outputFile = physical().logical().downcast<logical::SavedLightField>().filename();
                for (auto iter = metadata_.begin(); iter != metadata_.end(); iter++) {
                    std::string label = iter->first;
                    std::vector<Rectangle> boxes = iter->second;

                    std::filesystem::path pathForLabel = outputFile.parent_path().append(label + "_" + outputFile.filename().string());
                    std::ofstream ofs(pathForLabel, std::ios::binary);

                    std::string sizeAsString = std::to_string(boxes.size());
                    ofs.put((char)sizeAsString.length());
                    ofs.write(sizeAsString.data(), sizeAsString.length());

                    ofs.write(reinterpret_cast<char *>(boxes.data()), boxes.size() * sizeof(Rectangle));
                }

//                std::ofstream ofs(outputFile, std::ios::binary);
//                // FIXME: This doesn't work, e.g. when size is 234.
//                ofs.put((char)rectangles_.size());
//                ofs.write(reinterpret_cast<char*>(rectangles_.data()), rectangles_.size() * sizeof(Rectangle));
//                ofs.close();

                return std::nullopt;
            }
        }

    private:
        Metadata metadata_;
//        std::vector<Rectangle> rectangles_;
    };
};

class SaveToFile: public PhysicalOperator {
public:
    explicit SaveToFile(const LightFieldReference &logical,
                        PhysicalOperatorReference &parent)
            : PhysicalOperator(logical, {parent}, DeviceType::CPU,
                               runtime::make<Runtime>(*this, logical.downcast<logical::SavedLightField>())) {
        CHECK_EQ(parents().size(), 1);
    }

private:
    class Runtime: public runtime::UnaryRuntime<SaveToFile, FrameData> {
    public:
        Runtime(SaveToFile &physical, const logical::SavedLightField &logical)
                : runtime::UnaryRuntime<SaveToFile, FrameData>(physical),
                        outputs_{functional::transform<std::reference_wrapper<transactions::OutputStream>>(
                                 physical.parents().begin(), physical.parents().end(),
                                 [this, &logical](auto &parent) {
                                     return std::reference_wrapper(this->physical().context()->transaction().write(
                                             logical, this->geometry())); }) }
        { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            if(!all_parent_eos()) {
                auto &input = *iterator();
                auto &output = outputs_.front().get();

                std::copy(input.value().begin(), input.value().end(),
                          std::ostreambuf_iterator<char>(output.stream()));
                return iterator()++;
            } else
                return std::nullopt;
        }

    private:
        std::vector<std::reference_wrapper<transactions::OutputStream>> outputs_;
    };
};

class CopyFile: public PhysicalOperator {
public:
    explicit CopyFile(const LightFieldReference &logical,
                      const std::vector<std::filesystem::path> &destinations)
            : CopyFile(logical, destinations, {})
    { }

    explicit CopyFile(const LightFieldReference &logical,
                      const std::vector<std::filesystem::path> &destinations,
                      PhysicalOperatorReference &parent)
            : CopyFile(logical, destinations, std::vector<PhysicalOperatorReference>{parent})
    { }

    explicit CopyFile(const LightFieldReference &logical,
                      std::vector<std::filesystem::path> destinations,
                      const std::vector<PhysicalOperatorReference> &parents)
            : PhysicalOperator(logical, parents, DeviceType::CPU, runtime::make<Runtime>(*this)),
              sources_{functional::transform<std::filesystem::path>(
                          logical.expect_downcast<logical::StreamBackedLightField>().sources(),
                          [](auto &s) { return s.filename(); })},
              destinations_{std::move(destinations)}
    { CHECK_EQ(sources_.size(), destinations_.size()); }

    const std::vector<std::filesystem::path> &sources() const { return sources_; }
    const std::vector<std::filesystem::path> &destinations() const { return destinations_; }

private:
    class Runtime: public runtime::Runtime<CopyFile> {
    public:
        explicit Runtime(CopyFile &physical)
            : runtime::Runtime<CopyFile>(physical)
        { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            if(!copied_) {
                copied_ = true;
                for(auto i = 0u; i < physical().sources().size(); i++)
                    std::filesystem::copy(physical().sources()[i],
                                          physical().destinations()[i],
                                         std::filesystem::copy_options::overwrite_existing);
                return EmptyData{DeviceType::CPU};
            } else
                return std::nullopt;
        }

    private:
        bool copied_ = false;
    };

    const std::vector<std::filesystem::path> sources_;
    const std::vector<std::filesystem::path> destinations_;
};

} // namespace lightdb::physical

#endif //LIGHTDB_SAVEOPERATORS_H
