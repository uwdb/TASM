#ifndef TASM_SEMANTICSELECTION_H
#define TASM_SEMANTICSELECTION_H

#include <string>
#include <vector>

namespace tasm {

class MetadataSelection {
public:
    virtual std::string labelConstraints() const = 0;
    virtual const std::vector<std::string> &objects() const { static std::vector<std::string> empty; return empty; }
};

class SingleMetadataSelection : public MetadataSelection {
public:
    SingleMetadataSelection(std::string label)
        : label_(std::move(label)),
        objects_{label_}
    {}

    std::string labelConstraints() const override {
        // It's not critical, but it would be nice for this to be robust against sql-injection.
        return "label='" + label_ + "'";
    }

    const std::vector<std::string> &objects() const override { return objects_; }

private:
    const std::string label_;
    const std::vector<std::string> objects_;
};

class OrMetadataSelection : public MetadataSelection {
public:
    OrMetadataSelection(const std::vector<std::shared_ptr<MetadataSelection>> &elements)
            : elements_(elements)
    {
        for (const auto& element : elements_)
            objects_.insert(objects_.end(), element->objects().begin(), element->objects().end());
    }

    OrMetadataSelection(const std::vector<std::string> &objects)
    {
        elements_.resize(objects.size());
        std::transform(objects.begin(), objects.end(), elements_.begin(), [](std::string object) {
            return std::make_shared<SingleMetadataSelection>(object);
        });

        for (const auto& element : elements_)
            objects_.insert(objects_.end(), element->objects().begin(), element->objects().end());
    }

    std::string labelConstraints() const override {
        std::string constraint = "(";
        auto numElements = elements_.size();
        for (auto i = 0u; i < numElements; ++i) {
            constraint += elements_[i]->labelConstraints();
            if (i < numElements - 1)
                constraint += " OR ";
        }
        constraint += ")";
        return constraint;
    }

    const std::vector<std::string> &objects() const override {
        return objects_;
    }

private:
    std::vector<std::shared_ptr<MetadataSelection>> elements_;
    std::vector<std::string> objects_;
};

} // namespace tasm

#endif //TASM_SEMANTICSELECTION_H
