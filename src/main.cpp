#include <iostream>

#include "ScriptManager.h"
#include <yaml-cpp/yaml.h>

#include "LoadManager.h"
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
    auto devices = ADBC::ADBClient::Devices(RC::Utils::File::PlatformPath("bin/platform-tools/adb"));
    if (devices.empty()) {
        std::cout << "No device found" << std::endl;
        return 0;
    }
    auto adbc = ADBC::ADBClient::Create(RC::Utils::File::PlatformPath("bin/platform-tools/adb"), devices[0]);

    ScriptManager sm;

    sm.Adds(ScriptManager::Scan());
    sm.Initialize();
    bool running = true;
    FixedRate loop(60.0f);
    adbc->startRecordingAct();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    auto ret = adbc->stopRecordingAct();
    adbc->ReplayEvents(ret);
    LoadManager::Save(ret, "test.yml");
    /*UI ui;
    ui.Initialize();
    while (!glfwWindowShouldClose(ui.GetWindow())) {
        ui.Render();
        ui.Update();
    }
    running = false;
    ui.Shutdown();*/

    return 0;
}
