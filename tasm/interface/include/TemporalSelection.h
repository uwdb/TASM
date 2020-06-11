#ifndef TASM_TEMPORALSELECTION_H
#define TASM_TEMPORALSELECTION_H

#include <string>

namespace tasm {

class TemporalSelection {
public:
    virtual std::string frameConstraints() const = 0;
};

class EqualTemporalSelection : public TemporalSelection {
public:
    EqualTemporalSelection(int frame)
        : frame_(frame)
    {}

    std::string frameConstraints() const override {
        return "frame=" + std::to_string(frame_);
    }
private:
    int frame_;
};

class RangeTemporalSelection : public TemporalSelection {
public:
    RangeTemporalSelection(int lowerBoundInclusive, int upperBoundExclusive)
            : lowerBoundInclusive_(lowerBoundInclusive),
            upperBoundExclusive_(upperBoundExclusive)
    {}

    std::string frameConstraints() const override {
        return "frame >= " + std::to_string(lowerBoundInclusive_) + " and frame < " + std::to_string(upperBoundExclusive_);
    }
private:
    int lowerBoundInclusive_;
    int upperBoundExclusive_;
};

} // namespace tasm

#endif //TASM_TEMPORALSELECTION_H
