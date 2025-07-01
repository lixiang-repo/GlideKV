#include <iostream>
#include <glob.h>
#include <cstring>
#include <vector>
#include <fstream>
#include <sstream>
#include <zlib.h>
#include <tbb/concurrent_hash_map.h>


inline std::string file_pattern_replace(std::string str, std::string oldSubstr, std::string newSubstr) {
    size_t pos = 0;
    while ((pos = str.find(oldSubstr, pos)) != std::string::npos) {
        str.replace(pos, oldSubstr.length(), newSubstr);
        pos += newSubstr.length(); // 移动到新插入内容的后面
    }
    return str;
}

inline int64_t load_int_from_file(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        int64_t key;
        if (iss >> key) {
            return key;
        }
    }
    return -1; // 如果没有找到有效的整数，返回-1
}


inline std::vector<std::string> get_files(const std::string& dir, const std::string& pattern="sparse_*.gz") {
    std::vector<std::string> files;
    glob_t glob_result;
    memset(&glob_result, 0, sizeof(glob_result));
    std::string full_pattern = dir + "/" + pattern;
    int ret = glob(full_pattern.c_str(), GLOB_TILDE, NULL, &glob_result);
    if (ret == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
            files.push_back(glob_result.gl_pathv[i]);
        }
    } else if (ret == GLOB_NOMATCH) {
        std::cout << "tbb cache file: No matches found." << std::endl;
    } else {
        std::cout << "tbb cache file: glob() failed." << std::endl;
    }
    globfree(&glob_result);
    return files;
}

// Load data from gz compressed file where first column is key and remaining columns form a vector
template<typename K, typename V>
int64_t load_from_gz_file(const std::string& filename, tbb::concurrent_hash_map<K, std::unique_ptr<std::vector<V>>>& data, size_t dim=8) {
    gzFile file = gzopen(filename.c_str(), "r");
    
    if (!file) {
        std::cerr << "Error: Could not open gz file " << filename << std::endl;
        return -1;
    }
    
    char buffer[4096];
    std::string line;
    int count = 0;

    while (gzgets(file, buffer, sizeof(buffer)) != nullptr) {
        line = buffer;
        
        // Remove trailing newline characters
        if (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        if (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        
        std::istringstream iss(line);
        K key;
        auto vector_ptr = std::make_unique<std::vector<V>>();
        
        // Read the key (first column)
        if (!(iss >> key)) {
            continue; // Skip invalid lines
        }
        
        // Read the remaining values as vector
        V value;
        while (iss >> value) {
            vector_ptr->push_back(value);
        }
        
        if (vector_ptr->size() == dim) {
            bool inserted = data.emplace(key, std::move(vector_ptr));
            if (inserted) {
                count++;
            }
        }
    }
    gzclose(file);
    
    std::string line_file = file_pattern_replace(filename, ".gz", "_line_check.txt");
    int64_t line_count = load_int_from_file(line_file);
    if (count > 0 && count == line_count) {
        LOG(INFO) << "successfully loaded " << count << " keys from " << filename;
        return count;
    } else {
        LOG(INFO) << "file: " << filename << " has " << count << " keys, but " << line_count << " lines in " << line_file;
        return -1;
    }
    
}
