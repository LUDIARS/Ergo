// Fixed-input runner for the libFuzzer entry points (tests/<module>/fuzz_*.cpp).
// Built with any compiler (GCC included) and registered in ctest: each argument
// is a seed file or a directory of seed files, each passed once to
// LLVMFuzzerTestOneInput, plus a few built-in edge inputs. A crash or a
// sanitizer report fails the test. The same entry points build as libFuzzer
// binaries with -DERGO_BUILD_FUZZERS=ON (Clang); see spec/setup/hardening.md.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

namespace {

int run_bytes(const std::string& bytes) {
    // A copy in its own allocation, so reads past the end are visible to ASan.
    const std::vector<std::uint8_t> copy(bytes.begin(), bytes.end());
    return LLVMFuzzerTestOneInput(copy.empty() ? nullptr : copy.data(), copy.size());
}

bool run_file(const std::filesystem::path& p, std::size_t& count) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "fuzz_regression: cannot read %s\n", p.string().c_str());
        return false;
    }
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    run_bytes(bytes);
    ++count;
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::size_t count = 0;
    // Built-in edge inputs: empty, one byte of each extreme, a run of NULs and of 0xFF.
    for (const std::string& s : {std::string(), std::string(1, '\0'), std::string(1, '\xff'),
                                 std::string(4096, '\0'), std::string(4096, '\xff')}) {
        run_bytes(s);
        ++count;
    }
    for (int i = 1; i < argc; ++i) {
        const std::filesystem::path arg(argv[i]);
        if (std::filesystem::is_directory(arg)) {
            for (const auto& e : std::filesystem::directory_iterator(arg)) {
                if (e.is_regular_file() && !run_file(e.path(), count)) return 1;
            }
        } else if (!run_file(arg, count)) {
            return 1;
        }
    }
    std::printf("fuzz_regression: %zu inputs\n", count);
    return 0;
}
