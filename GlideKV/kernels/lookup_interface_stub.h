#pragma once
#include "tensorflow/core/kernels/lookup_table_op.h"
#include <ctime>
#include <string>
#include <fstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include <random>

namespace tensorflow {
namespace lookup {

// Stub base class for LookupInterface, implements no-op or default methods
class LookupInterfaceStub : public LookupInterface {
 public:
  std::atomic<bool> initialized_{false};

 public:
  size_t size() const override { return 0; }
  Status DoInsert(bool, const Tensor&, const Tensor&) { return OkStatus(); }
  Status Insert(OpKernelContext*, const Tensor&, const Tensor&) override { return OkStatus(); }
  Status Remove(OpKernelContext*, const Tensor&) override { return OkStatus(); }
  Status ImportValues(OpKernelContext*, const Tensor&, const Tensor&) override { return OkStatus(); }
  Status ExportValues(OpKernelContext*) override { return OkStatus(); }
  int64_t MemoryUsed() const override { return 0; }


};

} // namespace lookup
} // namespace tensorflow 