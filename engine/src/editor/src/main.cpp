#include <filesystem>
#include <iostream>
#include <thread>

#include "EASTL/string.h"
#include "EASTL/unordered_map.h"

// #include "editor/include/editor.h"
#include "runtime/engine.h"

#define BLUNDER_XSTR(s) BLUNDER_STR(s)
#define BLUNDER_STR(s) #s

int main(int argc, char** argv) {
  // std::filesystem::path executable_path(argv[0]);
  // std::filesystem::path config_file_path =
  //     executable_path.parent_path() / "PiccoloEditor.ini";

  Blunder::BlunderEngine* engine = new Blunder::BlunderEngine();

  engine->startEngine();
  engine->initialize();

  engine->run();

  // Blunder::BlunderEditor* editor = new Blunder::BlunderEditor();
  // editor->initialize(engine);

  // editor->run();

  // editor->clear();

  engine->clear();
  engine->shutdownEngine();

  return 0;
}