#include "snaappy/snaappy.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#include <fmt/std.h>

namespace snaappy {
namespace {

constexpr std::string_view kSeparator = "---\n";
// Above this many LCS table cells the diff skips alignment and lists every
// changed line as removed then added, to bound memory.
constexpr std::size_t kMaxDiffCells = std::size_t{4} * 1024 * 1024;

auto normalize_newlines(std::string_view text) -> std::string {
  std::string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
      continue;
    }
    out.push_back(text[i]);
  }
  return out;
}

auto is_valid_name(std::string_view name) -> bool {
  if (name.empty() || name.front() == '.') {
    return false;
  }
  return std::ranges::all_of(name, [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
  });
}

auto read_file(const std::filesystem::path& path)
    -> std::optional<std::string> {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (std::filesystem::exists(path)) {
      throw SnapshotError(fmt::format("cannot read {}", path));
    }
    return std::nullopt;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

void write_file(const std::filesystem::path& path, std::string_view text) {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
  out.close();
  if (error || !out) {
    throw SnapshotError(fmt::format("cannot write {}", path));
  }
}

void remove_file(const std::filesystem::path& path) {
  std::error_code error;
  std::filesystem::remove(path, error);
  if (error) {
    throw SnapshotError(
        fmt::format("cannot remove {}: {}", path, error.message()));
  }
}

auto serialize(const Snapshot& snapshot, std::string_view contents)
    -> std::string {
  return fmt::format("{}source: {}\nexpression: {}\n{}{}\n", kSeparator,
                     snapshot.source, snapshot.expression, kSeparator,
                     contents);
}

// Returns the contents of a `.snap` file: the text after the header, minus the
// trailing newline serialize() adds.
auto parse_contents(const std::filesystem::path& path, std::string_view file)
    -> std::string {
  const auto end = file.find(fmt::format("\n{}", kSeparator));
  if (!file.starts_with(kSeparator) || end == std::string_view::npos) {
    throw SnapshotError(fmt::format("{} has no snapshot header", path));
  }
  auto contents = file.substr(end + 1 + kSeparator.size());
  if (contents.ends_with('\n')) {
    contents.remove_suffix(1);
  }
  return std::string(contents);
}

auto split_lines(std::string_view text) -> std::vector<std::string_view> {
  std::vector<std::string_view> lines;
  if (text.empty()) {
    return lines;
  }
  std::size_t start = 0;
  while (true) {
    const auto end = text.find('\n', start);
    if (end == std::string_view::npos) {
      lines.push_back(text.substr(start));
      return lines;
    }
    lines.push_back(text.substr(start, end - start));
    start = end + 1;
  }
}

void append_line(std::string& out, char prefix, std::string_view line) {
  out.push_back(prefix);
  out.append(line);
  out.push_back('\n');
}

// Resolves the compiler-given source path against SNAAPPY_WORKSPACE_ROOT, like
// insta's INSTA_WORKSPACE_ROOT. An absolute path is kept as is.
auto resolve_source(std::string_view file) -> std::filesystem::path {
  const std::filesystem::path path(file);
  const char* root = std::getenv("SNAAPPY_WORKSPACE_ROOT");
  if (root != nullptr && *root != '\0') {
    return std::filesystem::path(root) / path;
  }
  if (path.is_relative()) {
    throw SnapshotError(fmt::format(
        "source path \"{}\" is relative; set SNAAPPY_WORKSPACE_ROOT to the "
        "directory it is relative to",
        file));
  }
  return path;
}

}  // namespace

auto update_mode_from_env() -> UpdateMode {
  const char* value = std::getenv("SNAAPPY_UPDATE");
  if (value == nullptr || std::string_view(value) == "new") {
    return UpdateMode::pending;
  }
  if (std::string_view(value) == "always") {
    return UpdateMode::always;
  }
  throw std::invalid_argument(fmt::format(
      "SNAAPPY_UPDATE must be \"new\" or \"always\", got \"{}\"", value));
}

auto diff_lines(std::string_view old_text, std::string_view new_text)
    -> std::string {
  const auto old_lines = split_lines(old_text);
  const auto new_lines = split_lines(new_text);

  // Common prefix and suffix need no alignment.
  std::size_t prefix = 0;
  while (prefix < old_lines.size() && prefix < new_lines.size() &&
         old_lines[prefix] == new_lines[prefix]) {
    ++prefix;
  }
  std::size_t suffix = 0;
  while (suffix < old_lines.size() - prefix &&
         suffix < new_lines.size() - prefix &&
         old_lines[old_lines.size() - 1 - suffix] ==
             new_lines[new_lines.size() - 1 - suffix]) {
    ++suffix;
  }

  const std::size_t rows = old_lines.size() - prefix - suffix;
  const std::size_t cols = new_lines.size() - prefix - suffix;
  const auto old_at = [&](std::size_t i) {
    return old_lines[prefix + i];
  };
  const auto new_at = [&](std::size_t j) {
    return new_lines[prefix + j];
  };

  std::string out;
  for (std::size_t i = 0; i < prefix; ++i) {
    append_line(out, ' ', old_lines[i]);
  }

  if ((rows + 1) * (cols + 1) > kMaxDiffCells) {
    for (std::size_t i = 0; i < rows; ++i) {
      append_line(out, '-', old_at(i));
    }
    for (std::size_t j = 0; j < cols; ++j) {
      append_line(out, '+', new_at(j));
    }
  } else {
    // lcs[i][j]: longest common subsequence of old[i..] and new[j..].
    std::vector<std::uint32_t> lcs((rows + 1) * (cols + 1), 0);
    const auto cell = [&](std::size_t i, std::size_t j) -> std::uint32_t& {
      return lcs[(i * (cols + 1)) + j];
    };
    for (std::size_t i = rows; i-- > 0;) {
      for (std::size_t j = cols; j-- > 0;) {
        cell(i, j) = old_at(i) == new_at(j)
                         ? cell(i + 1, j + 1) + 1
                         : std::max(cell(i + 1, j), cell(i, j + 1));
      }
    }
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < rows || j < cols) {
      if (i < rows && j < cols && old_at(i) == new_at(j)) {
        append_line(out, ' ', old_at(i++));
        ++j;
      } else if (i < rows && (j == cols || cell(i + 1, j) >= cell(i, j + 1))) {
        append_line(out, '-', old_at(i++));
      } else {
        append_line(out, '+', new_at(j++));
      }
    }
  }

  for (std::size_t i = old_lines.size() - suffix; i < old_lines.size(); ++i) {
    append_line(out, ' ', old_lines[i]);
  }
  return out;
}

auto check_snapshot(const std::filesystem::path& dir, std::string_view name,
                    const Snapshot& snapshot, UpdateMode mode) -> Result {
  if (!is_valid_name(name)) {
    throw std::invalid_argument(fmt::format(
        "invalid snapshot name \"{}\": use letters, digits, '_', '-' and '.', "
        "not starting with '.'",
        name));
  }

  const auto path = dir / fmt::format("{}.snap", name);
  const auto pending_path = dir / fmt::format("{}.snap.new", name);
  const auto contents = normalize_newlines(snapshot.contents);

  std::optional<std::string> stored;
  if (auto file = read_file(path)) {
    stored = parse_contents(path, normalize_newlines(*file));
  }

  if (stored == contents) {
    remove_file(pending_path);
    return {.status = Status::matched, .path = path, .diff = {}};
  }

  if (mode == UpdateMode::always) {
    write_file(path, serialize(snapshot, contents));
    remove_file(pending_path);
    return {.status = Status::updated, .path = path, .diff = {}};
  }

  write_file(pending_path, serialize(snapshot, contents));
  if (!stored) {
    return {.status = Status::missing, .path = path, .diff = {}};
  }
  return {.status = Status::mismatched,
          .path = path,
          .diff = diff_lines(*stored, contents)};
}

void assert_snapshot(std::string_view name, std::string_view contents,
                     std::string_view expression,
                     std::source_location location) {
  const auto source = resolve_source(location.file_name());
  const Snapshot snapshot{
      .source = source.filename().string(),
      .expression = std::string(expression),
      .contents = std::string(contents),
  };
  const auto result =
      check_snapshot(source.parent_path() / "snapshots",
                     fmt::format("{}__{}", source.stem().string(), name),
                     snapshot, update_mode_from_env());

  constexpr std::string_view kHint =
      "Accept it by renaming the .snap.new file to .snap, or rerun with "
      "SNAAPPY_UPDATE=always.";
  switch (result.status) {
    case Status::matched:
    case Status::updated:
      return;
    case Status::missing:
      throw SnapshotError(
          fmt::format("snapshot {} does not exist ({}:{})\nnew value:\n{}\n{}",
                      result.path, location.file_name(), location.line(),
                      normalize_newlines(contents), kHint));
    case Status::mismatched:
      throw SnapshotError(fmt::format(
          "snapshot {} does not match ({}:{})\n-old +new:\n{}{}", result.path,
          location.file_name(), location.line(), result.diff, kHint));
  }
}

}  // namespace snaappy
