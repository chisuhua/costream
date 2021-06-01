#include "CoLocalStorage.h"

namespace co {


CLSMap* GetThreadLocalCLSMap() {
    static thread_local CLSMap tlm;
    return &tlm;
}

} //namespace co
