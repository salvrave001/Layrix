#pragma once

#include "engine/LayoutModel.h"

#include <filesystem>
#include <functional>

namespace layrix {

AppSettings MakeDefaultSettings();

class LayoutStore {
public:
    explicit LayoutStore(std::filesystem::path path);
    static std::filesystem::path BesideExecutable();

    AppSettings Load();
    void Save(const AppSettings& settings);
    void Update(const std::function<void(AppSettings&)>& mutator);

    const std::filesystem::path& path() const { return path_; }

private:
    AppSettings LoadUnlocked();
    void SaveUnlocked(const AppSettings& settings);

    std::filesystem::path path_;
};

}  // namespace layrix
