#ifndef PYTASM_UTILITIES_H
#define PYTASM_UTILITIES_H

#include <vector>
#include <boost/python/list.hpp>

template <typename T>
std::vector<T> extract(boost::python::list l) {
    std::vector<T> result;
    for (auto i = 0; i < boost::python::len(l); ++i) {
        result.push_back(boost::python::extract<T>(l[i]));
    }
    return result;
}

template <typename T>
boost::python::list listify(std::vector<T> vec) {
    auto list = boost::python::list();
    for (const auto &i : vec)
        list.append(i);
    return list;
}

#endif //PYTASM_UTILITIES_H
