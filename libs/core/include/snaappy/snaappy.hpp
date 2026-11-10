#pragma once

#include <cstdint>
#include <filesystem>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

#include <fmt/format.h>
#include <fmt/ranges.h>

namespace snaappy {

/**
 * @brief What a comparison does when the stored snapshot is missing or differs.
 */
enum class UpdateMode : std::uint8_t {
  /// Write the new value to `<name>.snap.new` and fail (default).
  pending,
  /// Write the new value to `<name>.snap` and pass.
  always,
};

/**
 * @brief Reads the `SNAAPPY_UPDATE` environment variable.
 *
 * Unset or `new` selects UpdateMode::pending, `always` selects UpdateMode::always.
 *
 * @throws std::invalid_argument for any other value.
 */
[[nodiscard]] auto update_mode_from_env() -> UpdateMode;

/**
 * @brief A value to compare, plus the metadata written to the file header.
 */
struct Snapshot {
  /// Source file that holds the assertion.
  std::string source;
  /// Expression that produced the value.
  std::string expression;
  /// The serialized value.
  std::string contents;
};

/**
 * @brief Outcome of check_snapshot().
 */
enum class Status : std::uint8_t {
  /// Stored snapshot equals the new value.
  matched,
  /// UpdateMode::always wrote the new value to `.snap`.
  updated,
  /// No `.snap` file; the new value went to `.snap.new`.
  missing,
  /// `.snap` differs; the new value went to `.snap.new`.
  mismatched,
};

/**
 * @brief Result of check_snapshot().
 */
struct Result {
  Status status;
  /// The `.snap` file compared against.
  std::filesystem::path path;
  /// Line diff from stored to new contents; empty unless Status::mismatched.
  std::string diff;

  [[nodiscard]] auto passed() const -> bool {
    return status == Status::matched || status == Status::updated;
  }
};

/**
 * @brief Compares a snapshot with `<dir>/<name>.snap`, writing files per mode.
 *
 * A match deletes any stale `<name>.snap.new`.
 * @param dir directory the snapshot is located in
 * @param name file name stem: letters, digits, `_`, `-` and `.`, not starting with `.`.
 * @param snapshot snapshot to check against
 * @param mode 
 * @throws std::invalid_argument when name is not a valid file name stem.
 * @throws SnapshotError when a file cannot be read, parsed or written.
 */
[[nodiscard]] auto check_snapshot(const std::filesystem::path& dir,
                                  std::string_view name,
                                  const Snapshot& snapshot, UpdateMode mode)
    -> Result;

/**
 * @brief Line diff of two texts; each line starts with `-`, `+` or a space.
 */
[[nodiscard]] auto diff_lines(std::string_view old_text,
                              std::string_view new_text) -> std::string;

/**
 * @brief Thrown when a snapshot assertion fails or its files cannot be used.
 */
class SnapshotError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

/**
 * @brief Checks contents against snapshot `name` of the calling source file.
 *
 * The file is `snapshots/<source stem>__<name>.snap`, next to the source file.
 * A relative source path (from `-ffile-prefix-map`, Bazel, ...) resolves
 * against the `SNAAPPY_WORKSPACE_ROOT` environment variable; an absolute one
 * is used as is. The mode comes from update_mode_from_env().
 *
 * @throws SnapshotError when the snapshot is missing or differs, or when the
 * source path is relative and `SNAAPPY_WORKSPACE_ROOT` is unset.
 */
void assert_snapshot(
    std::string_view name, std::string_view contents,
    std::string_view expression,
    std::source_location location = std::source_location::current());

}  // namespace snaappy

/**
 * @brief Asserts that the value, formatted with `fmt`, matches snapshot `name`.
 *
 * Variadic so values with commas need no extra parentheses. The value needs
 * a `fmt::formatter`; ranges work through `fmt/ranges.h`.
 */
#define SNAAPPY_ASSERT_SNAPSHOT(name, ...)                               \
  ::snaappy::assert_snapshot((name), ::fmt::format("{}", (__VA_ARGS__)), \
                             #__VA_ARGS__)
