#pragma once

#include <string>

class App {
public:
    explicit App(std::wstring startupCatalogPath = {});
    int Run();

private:
    std::wstring startupCatalogPath_;
};

