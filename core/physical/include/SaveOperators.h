#ifndef LIGHTDB_SAVEOPERATORS_H
#define LIGHTDB_SAVEOPERATORS_H

#include "PhysicalOperators.h"

#include "timer.h"
#include <iostream>

#include "sqlite3.h"

namespace lightdb::physical {

class SaveBoxes: public PhysicalOperator {
public:
    explicit SaveBoxes(std::shared_ptr<metadata::MetadataManager> metadataManager, const LightFieldReference &logical, PhysicalOperatorReference &parent)
            : PhysicalOperator(logical, {parent}, physical::DeviceType::CPU, runtime::make<Runtime>(*this, "SaveBoxes-init", metadataManager))
    { }

private:
    class Runtime: public runtime::Runtime<> {
    public:
        explicit Runtime(PhysicalOperator &physical, std::shared_ptr<metadata::MetadataManager> metadataManager)
                : runtime::Runtime<>(physical),
                        metadataManager_(metadataManager)
        { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            if (!all_parent_eos()) {
                assert(iterators().size() == 1);
                MaterializedLightFieldReference input = *(iterators().front());
                assert(input.is<MetadataLightField>());
                MetadataLightField metadataLightField = input.downcast<MetadataLightField>();

                Metadata lfMetadata = metadataLightField.metadata();
                std::for_each(lfMetadata.begin(), lfMetadata.end(), [&](auto labelAndRectangles) {
                    std::for_each(labelAndRectangles.second.begin(), labelAndRectangles.second.end(), [&](const Rectangle &rectangle) {
                        metadataManager_->addMetadata(labelAndRectangles.first, rectangle.id, rectangle.x, rectangle.y, rectangle.width, rectangle.height);
                    });
//                    std::vector<Rectangle> &rectanglesForLabel = metadata_[labelAndRectangles.first];
//                    rectanglesForLabel.insert(rectanglesForLabel.end(), labelAndRectangles.second.begin(), labelAndRectangles.second.end());
                });

                return iterators().front()++;
            } else {
                return std::nullopt;
            }
        }

    private:
        std::shared_ptr<metadata::MetadataManager> metadataManager_;
        Metadata metadata_;
//        std::vector<Rectangle> rectangles_;
    };
private:
};

class SaveToFile: public PhysicalOperator {
public:
    explicit SaveToFile(const LightFieldReference &logical,
                        PhysicalOperatorReference &parent)
            : PhysicalOperator(logical, {parent}, DeviceType::CPU,
                               runtime::make<Runtime>(*this, "SaveToFile-init", logical.downcast<logical::SavedLightField>())) {
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
        {
//            timer_.endSection();
        }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            GLOBAL_TIMER.startSection("SaveToFile");
            if(!all_parent_eos()) {
                auto &input = *iterator();
                timer_.startSection("inside-SaveToFile");
                auto &output = outputs_.front().get();

                std::copy(input.value().begin(), input.value().end(),
                          std::ostreambuf_iterator<char>(output.stream()));
                auto returnVal = *iterator();
                timer_.endSection("inside-SaveToFile");
                iterator()++;
                GLOBAL_TIMER.endSection("SaveToFile");
                return returnVal;
            } else {
                GLOBAL_TIMER.endSection("SaveToFile");
                timer_.printAllTimes();
                return std::nullopt;
            }
        }

    private:
        std::vector<std::reference_wrapper<transactions::OutputStream>> outputs_;
        Timer timer_;
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
            : PhysicalOperator(logical, parents, DeviceType::CPU, runtime::make<Runtime>(*this, "CopyFile-init")),
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
