#include "io/netlist_io.h"

// TODO(A): 每完成一项对照 docs/test-plan.md 的 T-04/T-05 核对。

namespace editor {

bool NetlistIO::save(const Schematic& schematic, const std::string& path) {
    (void)schematic;
    (void)path;
    return false;  // TODO(A)
}

bool NetlistIO::load(Schematic& schematic, const std::string& path) {
    (void)schematic;
    (void)path;
    return false;  // TODO(A)
}

bool NetlistIO::exportNetlist(const Schematic& schematic, const std::string& path) {
    (void)schematic;
    (void)path;
    return false;  // TODO(A)
}

} // namespace editor
