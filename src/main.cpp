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

void Task(ScriptManager&sm, bool&running, FixedRate&fr, std::shared_ptr<ADBC::ADBClient> adbc) {
    sm.Invokes("Awake");
    sm.Invokes("Start", adbc);
    while (running) {
        sm.Invokes("Update", 1.f / fr.frequency);

        std::this_thread::sleep_for(fr.interval);
    }
    sm.Invokes("OnDestroy");
}


int main() {
    auto devices = ADBC::ADBClient::Devices(RC::Utils::File::PlatformPath("bin/platform-tools/adb"));
    if (devices.empty()) {
        std::cout << "No device found" << std::endl;
        return 0;
    }
    auto adbc = ADBC::ADBClient::Create(RC::Utils::File::PlatformPath("bin/platform-tools/adb"), devices[1]);

    ScriptManager sm;

    sm.Adds(ScriptManager::Scan("scripts"));
    sm.Initialize();
    bool running = true;
    FixedRate loop(60.0f);
    ThreadWrapper t(Task, std::ref(sm), std::ref(running), std::ref(loop), adbc);
    t.start();
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
