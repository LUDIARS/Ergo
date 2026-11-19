// test_package_cli_process の子: 受け取った引数を 1 行に 1 個、 [ ] で囲んで標準出力へ書く。
//   --exit <n>    : 残りを書いたあと終了コード n で終わる
//   --stderr <s>  : s を標準エラーへ書く (標準出力には出さない)
#include <charconv>
#include <cstdio>
#include <string_view>

int main(int argc, char** argv) {
    int code = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--exit" && i + 1 < argc) {
            const std::string_view value = argv[++i];
            const auto end = value.data() + value.size();
            const auto parsed = std::from_chars(value.data(), end, code);
            if (parsed.ec != std::errc{} || parsed.ptr != end || code < 0 || code > 125) return 126;
        } else if (arg == "--stderr" && i + 1 < argc) {
            std::fprintf(stderr, "%s\n", argv[++i]);
        } else {
            std::printf("[%s]\n", argv[i]);
        }
    }
    return code;
}
