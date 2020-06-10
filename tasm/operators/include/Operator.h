#ifndef TASM_OPERATOR_H
#define TASM_OPERATOR_H

#include "Configuration.h"
#include <optional>

template <class T>
class Operator {
public:
    Operator() = default;
    Operator(const Operator&) = delete;
    Operator(const Operator&&) = delete;
    virtual ~Operator() = default;

    virtual std::optional<T> next() = 0;
    virtual bool isComplete() = 0;
    virtual const Configuration &configuration() = 0;
};

#endif //TASM_OPERATOR_H
