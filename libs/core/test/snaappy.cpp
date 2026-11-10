#include "snaappy/snaappy.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

auto read(const fs::path& path) -> std::string {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

void write(const fs::path& path, const std::string& text) {
  std::ofstream(path, std::ios::binary) << text;
}

auto value(const std::string& contents) -> snaappy::Snapshot {
  return {.source = "test.cpp", .expression = "value", .contents = contents};
}

class CheckSnapshot : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    dir_ = fs::path(::testing::TempDir()) / "snaappy" /
           (std::string(info->test_suite_name()) + "." + info->name());
    fs::remove_all(dir_);
  }

  void TearDown() override { fs::remove_all(dir_); }

  [[nodiscard]] auto snap() const -> fs::path { return dir_ / "case.snap"; }
  [[nodiscard]] auto pending() const -> fs::path {
    return dir_ / "case.snap.new";
  }

  auto check(const std::string& contents,
             snaappy::UpdateMode mode = snaappy::UpdateMode::pending)
      -> snaappy::Result {
    return snaappy::check_snapshot(dir_, "case", value(contents), mode);
  }

  fs::path dir_;
};

TEST_F(CheckSnapshot, MissingWritesPendingFile) {
  const auto result = check("hello");

  EXPECT_EQ(result.status, snaappy::Status::missing);
  EXPECT_FALSE(result.passed());
  EXPECT_FALSE(fs::exists(snap()));
  EXPECT_EQ(read(pending()),
            "---\nsource: test.cpp\nexpression: value\n---\nhello\n");
}

TEST_F(CheckSnapshot, AlwaysCreatesSnapshot) {
  const auto result = check("hello", snaappy::UpdateMode::always);

  EXPECT_EQ(result.status, snaappy::Status::updated);
  EXPECT_TRUE(result.passed());
  EXPECT_EQ(read(snap()),
            "---\nsource: test.cpp\nexpression: value\n---\nhello\n");
  EXPECT_FALSE(fs::exists(pending()));
}

TEST_F(CheckSnapshot, MatchPassesAndRemovesStalePendingFile) {
  (void)check("a\nb", snaappy::UpdateMode::always);
  (void)check("changed");
  ASSERT_TRUE(fs::exists(pending()));

  const auto result = check("a\nb");

  EXPECT_EQ(result.status, snaappy::Status::matched);
  EXPECT_FALSE(fs::exists(pending()));
}

TEST_F(CheckSnapshot, MismatchWritesPendingFileAndKeepsSnapshot) {
  (void)check("a\nb\nc", snaappy::UpdateMode::always);
  const auto before = read(snap());

  const auto result = check("a\nB\nc");

  EXPECT_EQ(result.status, snaappy::Status::mismatched);
  EXPECT_EQ(result.diff, " a\n-b\n+B\n c\n");
  EXPECT_EQ(read(snap()), before);
  EXPECT_TRUE(read(pending()).ends_with("---\na\nB\nc\n"));
}

TEST_F(CheckSnapshot, AlwaysOverwritesMismatch) {
  (void)check("old", snaappy::UpdateMode::always);

  const auto result = check("new", snaappy::UpdateMode::always);

  EXPECT_EQ(result.status, snaappy::Status::updated);
  EXPECT_TRUE(read(snap()).ends_with("---\nnew\n"));
}

TEST_F(CheckSnapshot, IgnoresCrlfLineEndings) {
  fs::create_directories(dir_);
  write(snap(),
        "---\r\nsource: test.cpp\r\nexpression: value\r\n---\r\na\r\nb\r\n");

  EXPECT_EQ(check("a\nb").status, snaappy::Status::matched);
  EXPECT_EQ(check("a\r\nb").status, snaappy::Status::matched);
}

TEST_F(CheckSnapshot, KeepsTrailingNewlineOfValue) {
  (void)check("line\n", snaappy::UpdateMode::always);

  EXPECT_EQ(check("line\n").status, snaappy::Status::matched);
  EXPECT_EQ(check("line").status, snaappy::Status::mismatched);
}

TEST_F(CheckSnapshot, RejectsFileWithoutHeader) {
  fs::create_directories(dir_);
  write(snap(), "no header");

  EXPECT_THROW((void)check("x"), snaappy::SnapshotError);
}

TEST_F(CheckSnapshot, RejectsUnsafeNames) {
  for (const auto* name : {"", ".hidden", "../escape", "a/b", "a b"}) {
    EXPECT_THROW((void)snaappy::check_snapshot(dir_, name, value("x"),
                                               snaappy::UpdateMode::always),
                 std::invalid_argument)
        << name;
  }
}

TEST(DiffLines, MarksAddedAndRemovedLines) {
  EXPECT_EQ(snaappy::diff_lines("a\nb\nc\nd", "a\nc\nd\ne"),
            " a\n-b\n c\n d\n+e\n");
  EXPECT_EQ(snaappy::diff_lines("", "x"), "+x\n");
  EXPECT_EQ(snaappy::diff_lines("x", ""), "-x\n");
}

TEST(UpdateModeFromEnv, ReadsVariable) {
  ::unsetenv("SNAAPPY_UPDATE");
  EXPECT_EQ(snaappy::update_mode_from_env(), snaappy::UpdateMode::pending);

  ::setenv("SNAAPPY_UPDATE", "always", 1);
  EXPECT_EQ(snaappy::update_mode_from_env(), snaappy::UpdateMode::always);

  ::setenv("SNAAPPY_UPDATE", "yes", 1);
  EXPECT_THROW((void)snaappy::update_mode_from_env(), std::invalid_argument);

  ::unsetenv("SNAAPPY_UPDATE");
}

// Uses the committed snapshots/ next to this file.
TEST(AssertSnapshot, MatchesCommittedSnapshot) {
  SNAAPPY_ASSERT_SNAPSHOT("vector", std::vector{1, 2, 3});
  SNAAPPY_ASSERT_SNAPSHOT("map",
                          std::map<std::string, int>{{"a", 1}, {"b", 2}});
}

// Must stay last: #line makes the rest of this file report a relative source
// path, as -ffile-prefix-map does.
#line 1000 "relative/dir/source.cpp"

TEST(AssertSnapshot, RelativeSourceNeedsWorkspaceRoot) {
  ::unsetenv("SNAAPPY_WORKSPACE_ROOT");

  try {
    SNAAPPY_ASSERT_SNAPSHOT("x", 1);
    FAIL() << "expected SnapshotError";
  } catch (const snaappy::SnapshotError& error) {
    EXPECT_NE(std::string(error.what()).find("SNAAPPY_WORKSPACE_ROOT"),
              std::string::npos);
  }
}

TEST(AssertSnapshot, RelativeSourceResolvesAgainstWorkspaceRoot) {
  const auto root =
      fs::path(::testing::TempDir()) / "snaappy" / "workspace_root";
  fs::remove_all(root);
  ::setenv("SNAAPPY_WORKSPACE_ROOT", root.c_str(), 1);
  ::unsetenv("SNAAPPY_UPDATE");

  EXPECT_THROW(SNAAPPY_ASSERT_SNAPSHOT("x", 1), snaappy::SnapshotError);
  EXPECT_TRUE(fs::exists(root / "relative/dir/snapshots/source__x.snap.new"));

  ::unsetenv("SNAAPPY_WORKSPACE_ROOT");
  fs::remove_all(root);
}

}  // namespace
