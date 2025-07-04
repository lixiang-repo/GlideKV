#include "thread_local_random.h"

namespace tensorflow {
namespace lookup {

thread_local std::mt19937 ThreadLocalRandomGenerator::generator_;
thread_local std::uniform_real_distribution<double> ThreadLocalRandomGenerator::distribution_(0.0, 1.0);
thread_local std::atomic<bool> ThreadLocalRandomGenerator::initialized_(false);

} // namespace lookup
} // namespace tensorflow 