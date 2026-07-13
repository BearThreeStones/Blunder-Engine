#include "runtime/core/reflection/class_db.h"

#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  ClassDB::clear();
  expect_true("unknown before register", !ClassDB::hasClass("Object"));

  ClassDB::registerClass("Object");
  expect_true("known after register", ClassDB::hasClass("Object"));
  expect_true("still unknown other", !ClassDB::hasClass("NotExported"));

  const TypeInfo* info = ClassDB::getClass("Object");
  expect_true("getClass returns non-null", info != nullptr);
  if (info != nullptr) {
    expect_true("type name is Object", info->name == "Object");
  }

  ClassDB::clear();
  expect_true("cleared removes class", !ClassDB::hasClass("Object"));

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
