#ifndef LIGHTDB_CONTAINSCAR_H
#define LIGHTDB_CONTAINSCAR_H

#include <string>

class ContainsCarPredicate {
public:
    ContainsCarPredicate()
        : modelName_("data/detrac.model")
    {}

    void loadModel();
    void detectImage(const std::string &imagePath);

    ~ContainsCarPredicate() {
        freeModel();
    }

private:
    void freeModel();
    const std::string modelName_;
};



#endif //LIGHTDB_CONTAINSCAR_H
