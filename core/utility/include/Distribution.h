#ifndef LIGHTDB_DISTRIBUTION_H
#define LIGHTDB_DISTRIBUTION_H

#include <cassert>
#include <random>

// Based on https://www.csee.usf.edu/~kchriste/tools/genzipf.c
class ZipfDistribution {
public:
    ZipfDistribution(unsigned int n, double alpha)
        : n_(n), alpha_(alpha), distribution_(0.0, 1.0) {
        setInitializationConstant();
    }

    unsigned int operator()(std::default_random_engine &generator) {
        double z;
        do {
            z = distribution_(generator);
        } while ((z == 0) || (z == 1));

        // Map z to a value.
        double sum_prob = 0;
        for (auto i = 1u; i <= n_; ++i) {
            sum_prob += c_ / pow((double) i, alpha_);

            if (sum_prob >= z)
                return i;
        }
        assert(false);
    }

private:
    void setInitializationConstant() {
        c_ = 0;
        for (auto i = 1u; i <= n_; ++i)
            c_ += (1.0 / pow((double) i, alpha_));

        c_ = 1.0 / c_;
    }

    unsigned int n_;
    double alpha_;
    std::uniform_real_distribution<double> distribution_;
    double c_;
};

#endif //LIGHTDB_DISTRIBUTION_H
