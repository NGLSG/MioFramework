#include <iostream>

#include "ScriptManager.h"
#include <yaml-cpp/yaml.h>
#include <chrono>

#include "Encryption.h"
#include "LoadManager.h"
#include "../MUI/GUIManifest.h"
#include "../MUI/Application.h"
#include "../MUI/Variables.h"
#include "../MUI/Components/Event.h"

#define AES256 1

#ifdef _WIN32
#define ADB_LINK "https://dl.google.com/android/repository/platform-tools-latest-windows.zip"
#define suffix ".exe"
#elif defined(__APPLE__) && defined(__MACH__)
#define ADB_LINK "https://dl.google.com/android/repository/platform-tools-latest-darwin.zip"
#define suffix ""
#else
#define ADB_LINK "https://dl.google.com/android/repository/platform-tools-latest-linux.zip"
#define suffix ""
#endif
using namespace ADBC;

size_t streamStrEg(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t newLength = size * nmemb;
    try {
        std::string* response = static_cast<std::string *>(userp);
        response->append((char *)contents, newLength);

        // 在这里实时处理数据块
        std::cout << "Received chunk: " << std::string((char *)contents, newLength) << std::endl;
    }
    catch (std::bad_alloc&e) {
        return 0;
    }
    return newLength;
}

size_t streamJsonEg(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t newLength = size * nmemb;
    try {
        json* response = static_cast<json *>(userp);
        *response = json::parse(static_cast<char *>(contents), static_cast<char *>(contents) + newLength);

        // 在这里实时处理数据块
        std::cout << "Received chunk: " << std::string((char *)contents, newLength) << std::endl;
    }
    catch (std::bad_alloc&e) {
        return 0;
    }
    return newLength;
}

class ThreadWrapper {
public:
    template<typename Func, typename... Args>
    ThreadWrapper(Func&&func, Args&&... args)
        : stopFlag(false),
          task([&]() {
              func(std::forward<Args>(args)...);
          }),
          workerThread() {
    }

    ~ThreadWrapper() {
        stop();
    }

    void start() {
        if (!workerThread.joinable()) {
            workerThread = std::thread([this]() {
                while (!stopFlag.load()) {
                    task();
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                std::cout << "Thread is stopping" << std::endl;
            });
        }
        else {
            std::cerr << "Thread is already running" << std::endl;
        }
    }

    void stop() {
        stopFlag.store(true);
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }

private:
    std::atomic<bool> stopFlag;
    std::function<void()> task;
    std::thread workerThread;
};

struct FixedRate {
    const std::chrono::microseconds interval;
    const float frequency;

    FixedRate(float freq)
        : interval(std::chrono::microseconds(static_cast<int>(1'000'000 / freq))),
          frequency(freq) {
    }
};

void Task(std::shared_ptr<Script>&script, bool&running, FixedRate&fr, std::shared_ptr<ADBC::ADBClient> adbc) {
    script->Invoke("Awake");
    script->Invoke("Start", adbc);
    while (running) {
        script->Invoke("Update", 1.f / fr.frequency);

        std::this_thread::sleep_for(fr.interval);
    }
    script->Invoke("OnDestroy");
}

void PackResources(const std::string&packageFilePath, const std::vector<std::string>&resourcePaths) {
    std::ofstream packageFile(packageFilePath, std::ios::binary);
    if (!packageFile) {
        std::cerr << "Unable to open package file for writing." << std::endl;
        return;
    }

    std::vector<Mio::ResourceIndex> metadataList;

    for (const auto&resourcePath: resourcePaths) {
        std::ifstream resourceStream(resourcePath, std::ios::binary);
        if (!resourceStream) {
            std::cerr << "Unable to open resource file: " << resourcePath << std::endl;
            continue;
        }

        std::vector<char> resourceData(std::istreambuf_iterator<char>(resourceStream), {});
        resourceStream.close();

        packageFile.write(resourceData.data(), resourceData.size());

        Mio::ResourceIndex metadata;
        metadata.name = resourcePath;
        metadata.offset = packageFile.tellp();
        metadata.size = resourceData.size();
        metadataList.push_back(metadata);
    }

    for (const auto&metadata: metadataList) {
        packageFile.write((char *)(&metadata), sizeof(Mio::ResourceIndex));
    }

    packageFile.close();
}

void UnpackResources(const std::string&packageFilePath) {
    std::ifstream packageFile(packageFilePath, std::ios::binary);
    if (!packageFile) {
        std::cerr << "Unable to open package file for reading." << std::endl;
        return;
    }

    // 首先跳到文件末尾，确定元数据的总数
    packageFile.seekg(0, std::ios::end);
    size_t metadataSize = packageFile.tellg() % sizeof(Mio::ResourceIndex) == 0
                              ? packageFile.tellg() / sizeof(Mio::ResourceIndex)
                              : 0;
    packageFile.seekg(0, std::ios::end);

    std::vector<Mio::ResourceIndex> metadataList;
    for (size_t i = 0; i < metadataSize; ++i) {
        Mio::ResourceIndex metadata;
        packageFile.seekg(-(static_cast<long>(sizeof(Mio::ResourceIndex)) * (i + 1)), std::ios::end);
        packageFile.read(reinterpret_cast<char *>(&metadata), sizeof(Mio::ResourceIndex));
        metadataList.push_back(metadata);
    }

    // 然后根据元数据提取每个资源
    for (const auto&metadata: metadataList) {
        packageFile.seekg(metadata.offset);
        std::ofstream resourceFile(metadata.name, std::ios::binary);
        if (!resourceFile) {
            std::cerr << "Unable to create or open resource file: " << metadata.name << std::endl;
            continue;
        }

        resourceFile.write((char *)packageFile.rdbuf(), metadata.size);
        resourceFile.close();
    }

    packageFile.close();
}

int main(int argc, char** argv) {
    /*if (!RC::Utils::File::Exists(RC::Utils::File::PlatformPath(std::string("bin/platform-tools/adb") + suffix))) {
        std::cout << "Downloading adb" << std::endl;
        EURL::eurl::Download(ADB_LINK, RC::Utils::File::PlatformPath("bin.zip").c_str());
        RC::Compression::Extract("bin.zip", "bin");
    }
    auto devices = ADBC::ADBClient::Devices(RC::Utils::File::PlatformPath("bin/platform-tools/adb"));
    if (devices.empty()) {
        std::cout << "No device found" << std::endl;
        return 0;
    }
    auto adbc = ADBC::ADBClient::Create(RC::Utils::File::PlatformPath("bin/platform-tools/adb"), devices[0]);

    ScriptManager sm;

    sm.Adds(ScriptManager::Scan());
    sm.Initialize();*/
    bool running = true;

    Mio::Application app("Mio Framework", "");
    app.Initialize();
    std::vector<std::string> resourcePaths = {
        "imgui.ini",
        "assets/font/font.TTF",
        "assets/apk.apk",

    };
    // 打包资源到单个文件
    PackResources("resources.package", resourcePaths);
    auto [key, iv] = Encryption::GenerateKeyAndIV(256, 128);
    Encryption::AES::Encrypt("resources.package", key, iv, "resources.package.encrypted");
    Encryption::AES::Decrypt("resources.package.encrypted", key, iv, "resources.package.decrypted");


    auto manifest = Mio::GUIManifest::Create("Main");
    manifest->SavePath = "Scenes/Main";
    app.AddManifest(manifest);
    ImVec4 clear = {0.f, 0.f, 0.f, 1.f};
    Mio::ResourceManager::SaveManifest(manifest);
    while (!app.ShouldClose()) {
        app.SetClearColor(clear);
        app.Update();
    }
    running = false;
    app.Shutdown();

    return 0;
}
