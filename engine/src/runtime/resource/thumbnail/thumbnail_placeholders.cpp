#include "runtime/resource/thumbnail/thumbnail_placeholders.h"

#include <algorithm>
#include <cmath>

namespace Blunder {

namespace {

void setPixel(eastl::vector<uint8_t>& rgba, uint32_t width, uint32_t x,
              uint32_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  const size_t index =
      (static_cast<size_t>(y) * static_cast<size_t>(width) + x) * 4u;
  rgba[index + 0] = r;
  rgba[index + 1] = g;
  rgba[index + 2] = b;
  rgba[index + 3] = a;
}

void fillSolid(eastl::vector<uint8_t>& rgba, uint32_t width, uint32_t height,
               uint8_t r, uint8_t g, uint8_t b) {
  rgba.resize(static_cast<size_t>(width) * height * 4u);
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      setPixel(rgba, width, x, y, r, g, b, 255);
    }
  }
}

void drawFolderIcon(eastl::vector<uint8_t>& rgba, uint32_t width,
                    uint32_t height) {
  fillSolid(rgba, width, height, 40, 44, 52);
  const uint32_t tab_w = width / 3;
  const uint32_t tab_h = height / 6;
  for (uint32_t y = height / 8; y < height / 8 + tab_h; ++y) {
    for (uint32_t x = width / 8; x < width / 8 + tab_w; ++x) {
      setPixel(rgba, width, x, y, 90, 140, 210, 255);
    }
  }
  for (uint32_t y = height / 4; y < height * 7 / 8; ++y) {
    for (uint32_t x = width / 8; x < width * 7 / 8; ++x) {
      setPixel(rgba, width, x, y, 70, 110, 170, 255);
    }
  }
}

void drawMeshIcon(eastl::vector<uint8_t>& rgba, uint32_t width,
                  uint32_t height) {
  fillSolid(rgba, width, height, 36, 38, 44);
  const float cx = static_cast<float>(width) * 0.5f;
  const float cy = static_cast<float>(height) * 0.55f;
  const float scale = static_cast<float>(width) * 0.22f;

  // Simple isometric cube outline (2D projection) for readability at 128px.
  const auto project = [&](float x, float y, float z, float& out_x,
                           float& out_y) {
    out_x = cx + (x - z) * scale * 0.9f;
    out_y = cy + (-y + (x + z) * 0.35f) * scale * 0.9f;
  };

  const float cube[8][3] = {
      {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
      {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1},
  };
  float px[8], py[8];
  for (int i = 0; i < 8; ++i) {
    project(cube[i][0], cube[i][1], cube[i][2], px[i], py[i]);
  }

  const int edges[][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                          {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
  for (const auto& edge : edges) {
    const int i0 = edge[0];
    const int i1 = edge[1];
    const int steps = static_cast<int>(std::max(
        std::fabs(px[i1] - px[i0]), std::fabs(py[i1] - py[i0])));
    const int n = std::max(steps, 1);
    for (int s = 0; s <= n; ++s) {
      const float t = static_cast<float>(s) / static_cast<float>(n);
      const int x = static_cast<int>(px[i0] + (px[i1] - px[i0]) * t);
      const int y = static_cast<int>(py[i0] + (py[i1] - py[i0]) * t);
      if (x >= 0 && y >= 0 && x < static_cast<int>(width) &&
          y < static_cast<int>(height)) {
        setPixel(rgba, width, static_cast<uint32_t>(x),
                 static_cast<uint32_t>(y), 180, 200, 230, 255);
      }
    }
  }

  // Fill top face lightly
  const int top[] = {2, 3, 7, 6};
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      float fx = static_cast<float>(x);
      float fy = static_cast<float>(y);
      float min_x = px[top[0]];
      float max_x = px[top[0]];
      float min_y = py[top[0]];
      float max_y = py[top[0]];
      for (int k = 1; k < 4; ++k) {
        min_x = std::min(min_x, px[top[k]]);
        max_x = std::max(max_x, px[top[k]]);
        min_y = std::min(min_y, py[top[k]]);
        max_y = std::max(max_y, py[top[k]]);
      }
      if (fx >= min_x && fx <= max_x && fy >= min_y && fy <= max_y) {
        const size_t idx = (static_cast<size_t>(y) * width + x) * 4u;
        rgba[idx + 0] = static_cast<uint8_t>((rgba[idx + 0] + 100) / 2);
        rgba[idx + 1] = static_cast<uint8_t>((rgba[idx + 1] + 120) / 2);
        rgba[idx + 2] = static_cast<uint8_t>((rgba[idx + 2] + 150) / 2);
      }
    }
  }
}

void drawFileIcon(eastl::vector<uint8_t>& rgba, uint32_t width, uint32_t height) {
  fillSolid(rgba, width, height, 44, 46, 52);
  const uint32_t margin = width / 6;
  for (uint32_t y = margin; y < height - margin; ++y) {
    for (uint32_t x = margin; x < width - margin; ++x) {
      setPixel(rgba, width, x, y, 200, 205, 215, 255);
    }
  }
  for (uint32_t y = height / 3; y < height / 3 + 4; ++y) {
    for (uint32_t x = margin + 8; x < width - margin - 8; ++x) {
      setPixel(rgba, width, x, y, 120, 130, 150, 255);
    }
  }
  for (uint32_t y = height / 2; y < height / 2 + 4; ++y) {
    for (uint32_t x = margin + 8; x < width - margin - 8; ++x) {
      setPixel(rgba, width, x, y, 120, 130, 150, 255);
    }
  }
}

}  // namespace

void fillThumbnailPlaceholder(ThumbnailPlaceholderKind kind, uint32_t width,
                              uint32_t height, eastl::vector<uint8_t>& out_rgba) {
  switch (kind) {
    case ThumbnailPlaceholderKind::Folder:
      drawFolderIcon(out_rgba, width, height);
      break;
    case ThumbnailPlaceholderKind::Mesh:
      drawMeshIcon(out_rgba, width, height);
      break;
    case ThumbnailPlaceholderKind::File:
    default:
      drawFileIcon(out_rgba, width, height);
      break;
  }
}

}  // namespace Blunder
