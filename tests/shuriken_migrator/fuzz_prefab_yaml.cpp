// libFuzzer entry: the Shuriken migrator's YAML scanners (flat and tree) and
// the GPU conversion of what they extract.
#include "ergo/shuriken_migrator/migrator.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    using namespace ergo::shuriken_migrator;
    const std::string yaml(reinterpret_cast<const char*>(data), size);
    MigrationReport report;
    std::vector<ShurikenSource> systems;
    if (ParsePrefabYaml(yaml, systems, report)) {
        for (const auto& s : systems) {
            (void)ConvertToErgo(s, report);
            (void)EmitterDescriptorToJson(ConvertToGpuEmitter(s, report));
        }
    }
    ShurikenNode root;
    if (ParsePrefabTree(yaml, root, report)) {
        std::vector<GpuEmitterNode> nodes;
        (void)ConvertTreeToGpuEmitters(root, nodes, report);
    }
    return 0;
}
