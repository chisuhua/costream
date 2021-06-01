
#include "Shared.h"

namespace core {
std::function<void*(size_t, size_t, uint32_t)> BaseShared::allocate_ = nullptr;
std::function<void(void*)> BaseShared::free_ = nullptr;
}   // namespace core
