#ifndef TASM_OPERATOR_H
#define TASM_OPERATOR_H

template <class T>
class Operator {
public:
    Operator(const Operator&) = delete;
    Operator(const Operator&&) = delete;

    virtual bool hasNext() = 0;
    virtual T next() = 0;
};

#endif //TASM_OPERATOR_H
