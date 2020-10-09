#include "ContainsCar.h"
#include <msd_wrapper.h>

void ContainsCarPredicate::loadModel() {
    _init_msd_models();
    _load_msd_model(modelName_.c_str());
    _print_msd_model_names();
}

void ContainsCarPredicate::freeModel() {
    _free_msd_models();
}

void ContainsCarPredicate::detectImage(const std::string &imagePath) {
    auto detection = _detect_msd_model(modelName_.c_str(), imagePath.c_str());
    auto count = 0u;
    if (detection)
        count ++;
}
