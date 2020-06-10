#ifndef TASM_SPSC_QUEUE_H
#define TASM_SPSC_QUEUE_H

#include <boost/lockfree/spsc_queue.hpp>

namespace tasm {

template<typename T>
using spsc_queue = boost::lockfree::spsc_queue<T>;

} // namespace tasm

#endif //TASM_SPSC_QUEUE_H
