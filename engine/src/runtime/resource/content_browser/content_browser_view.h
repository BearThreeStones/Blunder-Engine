#pragma once

#include <cstdint>

#include "EASTL/string.h"
#include "EASTL/vector.h"
#include "runtime/resource/content_browser/content_browser_types.h"

namespace Blunder {

BrowserEntryKind classifyBrowserEntry(bool is_directory,
                                      const eastl::string& virtual_path);

const char* browserEntryTypeLabel(BrowserEntryKind kind);

eastl::string formatBrowserSize(uint64_t size_bytes, bool is_directory);

eastl::string formatBrowserDate(uint64_t modified_time, bool is_directory);

void fillBrowserGridItemMeta(ContentBrowserGridItem& item);

void toggleBrowserGridSort(BrowserGridSortColumn& column, bool& ascending,
                           BrowserGridSortColumn clicked);

void sortBrowserGridItems(eastl::vector<ContentBrowserGridItem>& items,
                          BrowserGridSortColumn column, bool ascending);

}  // namespace Blunder
