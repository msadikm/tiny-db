#include "./json.hpp"
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>

using json = nlohmann::json;

void touch(const std::string &path, bool create_dirs) {
  if (create_dirs) {
    std::string base_dir = path.substr(0, path.find_last_of("/\\"));
    struct stat info;

    if (stat(base_dir.c_str(), &info) != 0) {
      std::system(("mkdir -p " + base_dir).c_str());
    }
  }

  std::ofstream ofs(path, std::ios::app);
  if (!ofs) {
    throw std::runtime_error("Failed to create or open the file: " + path);
  }
}

class Storage {
public:
  virtual ~Storage() = default;

  virtual std::optional<
      std::unordered_map<std::string, std::unordered_map<std::string, json>>>
  read() = 0;
  virtual void
  write(const std::unordered_map<
        std::string, std::unordered_map<std::string, json>> &data) = 0;
  virtual void close() {}
};

class JSONStorage : public Storage {
public:
  JSONStorage(const std::string &path, bool create_dirs = false,
              const std::string &access_mode = "r+")
      : path_(path), mode_(access_mode) {
    if (access_mode != "r" && access_mode != "r+" && access_mode != "rb" &&
        access_mode != "rb+") {
      throw std::invalid_argument("Invalid access mode: " + access_mode);
    }

    if (access_mode.find_first_of("+wa") != std::string::npos) {
      touch(path, create_dirs);
    }

    openFile();
  }

  ~JSONStorage() { close(); }

  std::optional<
      std::unordered_map<std::string, std::unordered_map<std::string, json>>>
  read() override {
    handle_.seekg(0, std::ios::end);
    std::streampos size = handle_.tellg();

    if (size == 0) {
      return std::nullopt;
    } else {
      handle_.seekg(0, std::ios::beg);
      json j;
      handle_ >> j;
      return j.get<std::unordered_map<std::string,
                                      std::unordered_map<std::string, json>>>();
    }
  }

  void
  write(const std::unordered_map<
        std::string, std::unordered_map<std::string, json>> &data) override {
    handle_.seekp(0, std::ios::beg);
    json j = data;
    handle_ << j.dump(4); // Pretty-print JSON with indent of 4
    handle_.flush();
    if (handle_.bad()) {
      throw std::runtime_error("Failed to write data to file: " + path_);
    }
    handle_.close();
    openFile();
  }

  void close() override {
    if (handle_.is_open()) {
      handle_.close();
    }
  }

private:
  std::string path_;
  std::string mode_;
  std::fstream handle_;

  void openFile() {
    handle_.open(path_, modeToOpenMode(mode_));
    if (!handle_.is_open()) {
      throw std::runtime_error("Could not open file: " + path_);
    }
  }

  std::ios_base::openmode modeToOpenMode(const std::string &mode) const {
    if (mode == "r") {
      return std::ios::in;
    } else if (mode == "r+") {
      return std::ios::in | std::ios::out;
    } else if (mode == "rb") {
      return std::ios::in | std::ios::binary;
    } else if (mode == "rb+") {
      return std::ios::in | std::ios::out | std::ios::binary;
    } else {
      throw std::invalid_argument("Invalid access mode: " + mode);
    }
  }
};

class MemoryStorage : public Storage {
public:
  MemoryStorage() = default;

  std::optional<
      std::unordered_map<std::string, std::unordered_map<std::string, json>>>
  read() override {
    return memory_;
  }

  void
  write(const std::unordered_map<
        std::string, std::unordered_map<std::string, json>> &data) override {
    memory_ = data;
  }

private:
  std::optional<
      std::unordered_map<std::string, std::unordered_map<std::string, json>>>
      memory_;
};

int main() {
  try {
    // Example usage
    JSONStorage jsonStorage("data.json", true, "r+");
    MemoryStorage memoryStorage;

    // Write some data
    std::unordered_map<std::string, std::unordered_map<std::string, json>>
        data = {{"key1", {{"subkey1", "value1"}, {"subkey2", "value2"}}},
                {"key2", {{"subkey1", 123}, {"subkey2", 456}}}};
    jsonStorage.write(data);
    memoryStorage.write(data);

    // Read the data
    auto jsonData = jsonStorage.read();
    auto memoryData = memoryStorage.read();

    if (jsonData) {
      std::cout << "JSONStorage Data: " << json(*jsonData).dump(4) << std::endl;
    }
    if (memoryData) {
      std::cout << "MemoryStorage Data: " << json(*memoryData).dump(4)
                << std::endl;
    }

    jsonStorage.close();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
