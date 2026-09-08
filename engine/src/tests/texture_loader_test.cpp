#include "runtime/function/job/job_system.h"
#include "runtime/function/render/texture_loader.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <system_error>

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/resource/asset/asset.h"
#include "runtime/resource/asset/texture2d_asset.h"

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

bool writeBmp1x1(const std::filesystem::path& path) {
  unsigned char bmp[58]{};
  bmp[0] = 'B';
  bmp[1] = 'M';
  const uint32_t file_size = 58;
  std::memcpy(bmp + 2, &file_size, 4);
  const uint32_t pixel_offset = 54;
  std::memcpy(bmp + 10, &pixel_offset, 4);
  const uint32_t dib_size = 40;
  std::memcpy(bmp + 14, &dib_size, 4);
  const int32_t width = 1;
  const int32_t height = 1;
  std::memcpy(bmp + 18, &width, 4);
  std::memcpy(bmp + 22, &height, 4);
  const uint16_t planes = 1;
  const uint16_t bits = 24;
  std::memcpy(bmp + 26, &planes, 2);
  std::memcpy(bmp + 28, &bits, 2);
  bmp[54] = 0;
  bmp[55] = 0;
  bmp[56] = 255;
  bmp[57] = 0;

  std::ofstream out(path, std::ios::binary);
  if (!out.is_open()) {
    return false;
  }
  out.write(reinterpret_cast<const char*>(bmp), sizeof(bmp));
  return static_cast<bool>(out);
}

}  // namespace

int main() {
  using namespace Blunder;

  const std::filesystem::path bmp_path =
      std::filesystem::temp_directory_path() / "blunder_texture_loader_test.bmp";
  expect_true("write fixture bmp", writeBmp1x1(bmp_path));

  JobSystem jobs;
  jobs.initialize();

  {
    TextureLoader loader;
    TextureLoader::InitInfo info;
    info.job_system = &jobs;
    loader.initialize(info);
    expect_true("cpu-only loader skips GPU", !loader.isGpuEnabled());

    const eastl::string path_a("assets/Textures/coalesce.bmp");
    loader.requestFile(path_a, bmp_path);
    loader.requestFile(path_a, bmp_path);
    expect_true("duplicate path submits one Job",
                loader.submittedJobCount() == 1u);
    expect_true("one in-flight record", loader.inFlightCount() == 1u);

    jobs.wait();
    expect_true("Job decode wrote pixels",
                loader.decodedByteSize(path_a) == 4u);

    loader.tick();
    expect_true("headless tick does not enqueue GPU",
                loader.gpuEnqueueCount() == 0u);
    expect_true("cpu buffer dropped without device",
                loader.decodedByteSize(path_a) == 0u);
    expect_true("record cleared after cpu-only complete",
                loader.inFlightCount() == 0u);
    loader.shutdown();
  }

  {
    TextureLoader loader;
    TextureLoader::InitInfo info;
    info.job_system = &jobs;
    loader.initialize(info);

    Asset::Meta meta;
    meta.virtual_path = "assets/Textures/preloaded.bmp";
    eastl::vector<uint8_t> pixels{255, 0, 0, 255};
    Texture2DAsset asset(eastl::move(meta), 1, 1, 4, eastl::move(pixels));
    expect_true("null asset is no-op", loader.request(nullptr) == nullptr);
    loader.request(&asset);
    expect_true("preload skips Job", loader.submittedJobCount() == 0u);
    expect_true("preload wrote pixels",
                loader.decodedByteSize(asset.getVirtualPath()) == 4u);
    loader.tick();
    expect_true("headless dropped preload",
                loader.decodedByteSize(asset.getVirtualPath()) == 0u);
    expect_true("preload record drained", loader.inFlightCount() == 0u);
    loader.shutdown();
  }

  {
    TextureLoader loader;
    TextureLoader::InitInfo info;
    info.job_system = &jobs;
    loader.initialize(info);

    const eastl::string missing_key("assets/Textures/missing.bmp");
    loader.requestFile(missing_key, "no_such_texture.bmp");
    jobs.wait();
    expect_true("fail is not done", loader.decodedByteSize(missing_key) == 0u);
    expect_true("failed record still in-flight", loader.inFlightCount() == 1u);
    loader.tick();
    expect_true("tick drops failed record", loader.inFlightCount() == 0u);
    expect_true("fail does not enqueue GPU", loader.gpuEnqueueCount() == 0u);
    loader.shutdown();
  }

  {
    TextureLoader loader;
    TextureLoader::InitInfo info;
    info.job_system = &jobs;
    loader.initialize(info);

    const eastl::string path_b("assets/Textures/cancel.bmp");
    const uint64_t generation_before = loader.generation();
    loader.requestFile(path_b, bmp_path);
    loader.dropScene();
    expect_true("scene drop bumps generation",
                loader.generation() == generation_before + 1u);

    jobs.wait();
    expect_true("stale Job still wrote Job data",
                loader.decodedByteSize(path_b) == 4u);

    loader.tick();
    expect_true("stale generation drops cpu buffer",
                loader.decodedByteSize(path_b) == 0u);
    expect_true("stale record not kept", loader.inFlightCount() == 0u);
    expect_true("cancel does not enqueue GPU", loader.gpuEnqueueCount() == 0u);
    loader.shutdown();
  }

  jobs.shutdown();
  std::error_code ec;
  std::filesystem::remove(bmp_path, ec);

  if (g_failures > 0) {
    std::fprintf(stderr, "%d texture_loader_test failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
