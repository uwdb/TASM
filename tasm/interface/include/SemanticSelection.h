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

} // namespace tasm

#endif //TASM_SEMANTICSELECTION_H
