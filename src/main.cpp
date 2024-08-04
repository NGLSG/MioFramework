#include <iostream>

#include "ScriptManager.h"
#include <yaml-cpp/yaml.h>
#include <chrono>
#include "LoadManager.h"
#include "../MUI/GUIManifest.h"
#include "../MUI/UI.h"
#include "../MUI/Components/Event.h"

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

    Mio::UI ui;
    ui.Initialize();
    ImVec4 clear = {0.45f, 0.55f, 0.60f, 1.00f};
    Mio::Event::Declare("SetColEditorActive");
    auto manifest = Mio::ResourceManager::LoadManifest("Test");
    auto colEditor = manifest->GetUIByName<Mio::ColorPicker>("colorPicker1");
    colEditor->GetData().v = &clear;
    auto style = colEditor->style.GetStyle();
    //style.Alpha = 0.5f;
    colEditor->style.Modify(style);
    Mio::Event::Modify("SetColEditorActive", [&]() {
        colEditor->SetActive(!colEditor->ActiveSelf());
    });
    Mio::ResourceManager::SaveManifest(manifest);
    ui.AddManifest(manifest);

    while (!ui.ShouldClose()) {
        ui.SetClearColor(clear);
        ui.Update();
    }
    running = false;
    ui.Shutdown();

    return 0;
}
