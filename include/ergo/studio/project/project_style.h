#pragma once
/// What kind of project a profile's root is (spec/plan/2026-11-23-studio-gen5-intake.md, 1):
///   unity        ProjectSettings/ProjectVersion.txt and Assets/
///   cpp-ludiars  CMakeLists.txt that pulls in Pictor / Ergo / Lapilli
///                (find_package / add_subdirectory / FetchContent, ERGO_PACKAGES, or a lib/lapilli submodule)
///   cpp-other    CMakeLists.txt without those
///   other        none of the above
/// detect() only looks through a FileProbe, so it is pure and tested with a
/// fake one; DiskProbe reads a real folder. A profile's explicit "style" wins
/// over detection (the caller decides that).
#include <filesystem>
#include <optional>
#include <string>

namespace ergo::studio::project {

enum class ProjectStyle { unity, cpp_ludiars, cpp_other, other };

/// Files under a project root, by "/"-separated relative path.
class FileProbe {
public:
    virtual ~FileProbe() = default;
    virtual bool                       exists(const std::string& rel) const = 0;   ///< file or folder
    virtual std::optional<std::string> read(const std::string& rel) const = 0;     ///< a file's text
};

/// A folder on disk. Reads at most 1 MiB of a file (enough for a CMakeLists.txt).
class DiskProbe final : public FileProbe {
public:
    explicit DiskProbe(std::filesystem::path root) : root_(std::move(root)) {}
    bool                       exists(const std::string& rel) const override;
    std::optional<std::string> read(const std::string& rel) const override;

private:
    std::filesystem::path root_;
};

ProjectStyle detect(const FileProbe& probe);

/// m_EditorVersion of ProjectSettings/ProjectVersion.txt ("" when absent).
std::string unity_editor_version(const FileProbe& probe);

/// "unity" / "cpp-ludiars" / "cpp-other" / "other".
const char* style_name(ProjectStyle style);
/// The style with that name, or nullopt.
std::optional<ProjectStyle> parse_style(const std::string& name);

} // namespace ergo::studio::project
