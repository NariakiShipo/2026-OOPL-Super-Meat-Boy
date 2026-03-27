#include "common/AssetPath.hpp"

#include <filesystem>
#include <vector>

#include "Util/Logger.hpp"
#include "config.hpp"

namespace Common {
std::string ResolveAssetPath(const std::string &relativePath) {
    const std::vector<std::string> candidates = {
        std::string(RESOURCE_DIR) + "/" + relativePath,
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
