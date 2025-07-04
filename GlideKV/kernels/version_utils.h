#ifndef VERSION_UTILS_H
#define VERSION_UTILS_H

#include <filesystem>
#include <string>
#include <regex>

namespace fs = std::filesystem;

inline std::filesystem::path get_model_path() {
    const char* warmup_path_env = std::getenv("WARMUP_PATH");
    std::string warmup_path = warmup_path_env ? std::string(warmup_path_env) : "";
    std::filesystem::path path(warmup_path);
    return path.parent_path().parent_path();
}

/**
 * 获取模型目录下的最大版本号
 */
inline int get_max_version(const std::string& path) {
    int max_version = -1;
    std::regex pattern("^\\d+$");
    
    // 先检测path是否存在
    if (!fs::exists(path) || !fs::is_directory(path)) {
        return max_version;
    }
    
    // 遍历path目录下的所有子目录
    for (const auto& entry : fs::directory_iterator(path)) {
        if (entry.is_directory()) {
            std::string version_str = entry.path().filename().string();
            if (std::regex_match(version_str, pattern)) {
                if (!fs::exists(path + "/" + version_str + "/saved_model.pb")) {
                    continue;
                }
                int version = std::stoi(version_str);
                if (version > max_version) {
                    max_version = version;
                }
            }
        }
    }
    
    return max_version;
}

#endif // VERSION_UTILS_H 