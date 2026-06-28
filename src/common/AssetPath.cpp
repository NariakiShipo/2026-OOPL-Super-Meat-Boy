#include "common/AssetPath.hpp"

#include <filesystem>
#include <vector>

#include "Util/Logger.hpp"
#include "config.hpp"

namespace Common {
std::string ResolveAssetPath(const std::string &relativePath) {
    const std::vector<std::string> candidates = {
        // 絕對路徑（編譯時內嵌，與執行時工作目錄無關）優先，跨平台最穩定
        std::string(RESOURCE_DIR) + "/" + relativePath,
#ifdef PTSD_ASSETS_ROOT
        std::string(PTSD_ASSETS_ROOT) + "/" + relativePath,
#endif
        // 以下為相對工作目錄的後備（僅在從專案根目錄執行時有效）
        "Resources/" + relativePath,
        "../Resources/" + relativePath,
        "PTSD/assets/" + relativePath,
        "../PTSD/assets/" + relativePath,
    };

    for (const auto &candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    LOG_WARN("Asset not found in known locations: {}", relativePath);
    return candidates.front();
}
} // namespace Common
