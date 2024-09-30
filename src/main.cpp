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
using namespace Mio;

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

struct FixedRate {
    const std::chrono::microseconds interval;
    const float frequency;

    FixedRate(float freq)
        : interval(std::chrono::microseconds(static_cast<int>(1'000'000 / freq))),
          frequency(freq) {
    }
};

struct AutomationTask {
    enum State {
        Recording,
        Awaking,
        Starting,
        Updating,
        Stopping,
        Idle,
    };

    static std::string stateToString(State state) {
        switch (state) {
            case Recording: return "Recording";
            case Awaking: return "Awaking";
            case Starting: return "Starting";
            case Updating: return "Updating";
            case Stopping: return "Stopping";
            case Idle: return "Idle";
            default: return "Unknown";
        }
    }

    std::shared_ptr<std::string> Device = std::make_shared<std::string>("");
    std::shared_ptr<std::string> RunningScript = std::make_shared<std::string>("");
    std::map<std::string, std::vector<AndroidEvent>> Replays;
    std::atomic<bool> running;
    std::thread ScriptThread;
    std::thread RecordingThread;
    std::atomic<State> state;

    static std::shared_ptr<AutomationTask> Create() {
        return std::make_shared<AutomationTask>();
    }
};

void Task(std::shared_ptr<Script>&script, std::atomic<bool>&running, FixedRate&fr,
          std::shared_ptr<ADBC::ADBClient> adbc, std::atomic<AutomationTask::State>&state) {
    state = AutomationTask::Awaking;
    script->Invoke("Awake");
    state = AutomationTask::Starting;
    script->Invoke("Start", adbc);
    state = AutomationTask::Updating;
    while (running) {
        script->Invoke("Update", 1.f / fr.frequency);

        std::this_thread::sleep_for(fr.interval);
    }
    state = AutomationTask::Stopping;
    script->Invoke("OnDestroy");
}

template<>
struct YAML::convert<std::shared_ptr<AutomationTask>> {
    static Node encode(const std::shared_ptr<AutomationTask>&rhs) {
        Node node;
        node["Device"] = *rhs->Device;
        node["Replays"] = rhs->Replays;
        return node;
    }

    static bool decode(const Node&node, std::shared_ptr<AutomationTask>&rhs) {
        if (!node.IsMap())
            return false;
        rhs = AutomationTask::Create();
        *rhs->Device = node["Device"].as<std::string>();
        if (!node["Replays"].IsDefined() && !node["Replays"].IsNull())
            rhs->Replays = {};
        else
            rhs->Replays = node["Replays"].as<std::map<std::string, std::vector<AndroidEvent>>>();
        return true;
    }
};

int main(int argc, char** argv) {
    Resource.UnpackAll();
    if (!RC::Utils::Directory::Exists("assets")) {
        RC::Utils::Directory::Create("assets");
    }
    if (!RC::Utils::File::Exists(RC::Utils::File::PlatformPath(std::string("bin/platform-tools/adb") + suffix))) {
        std::cout << "Downloading adb" << std::endl;
        eurl::Download(ADB_LINK, RC::Utils::File::PlatformPath("bin.zip").c_str());
        RC::Compression::Extract("bin.zip", "bin");
    }
    auto devices = ADBClient::Devices(RC::Utils::File::PlatformPath("bin/platform-tools/adb"));
    auto adbc = ADBClient::Create(RC::Utils::File::PlatformPath("bin/platform-tools/adb"));
    std::vector<std::shared_ptr<AutomationTask>> tasks;
    if (RC::Utils::File::Exists("events.yml"))
        tasks = LoadManager::Load<std::vector<std::shared_ptr<AutomationTask>>>("events.yml");
    else
        tasks = {};
    std::vector<AndroidEvent> ret;

    ScriptManager sm;
    FixedRate fr(60);
    sm.Adds(ScriptManager::Scan());
    sm.Initialize();

    Application app("Mio Framework", "");
    app.Initialize();
    auto manifest = ResourceManager::LoadManifest((ResourcePath / "Scenes/Main").string());
    //auto manifest = GUIManifest::Create("Main", (ResourcePath / "Scenes").string());
    app.AddManifest(manifest);
    auto window = manifest->GetUI<Window>("Main");
    auto DevicesBox = manifest->GetUI<ListBox>("Devices");
    auto scriptsList = manifest->GetUI<ListBox>("Scripts");
    auto window2 = manifest->GetUI<Window>("InputNameWindow");
    auto window3 = manifest->GetUI<Window>("WarningWindow");
    auto iptName = manifest->GetUI<InputText>("InputName");
    auto RecordingList = manifest->GetUI<Mio::ListBox>("RecordingList");
    //auto btn1 = Button::Create({"刷新脚本列表"});
    auto WarningText = manifest->GetUI<Text>("Text2");
    auto search = manifest->GetUI<InputText>("SearchInputText");
    auto runningList = manifest->GetUI<ListBox>("RunningList");
    auto console = manifest->GetUI<Console>("ConsoleLog");
    for (auto&task: tasks) {
        for (auto&it: task->Replays)
            RecordingList->GetData().items.push_back(it.first);
    }
    std::vector<std::string> scripts;
    Event::Modify("SearchScripts", [&] {
        for (auto&it: scripts) {
            scriptsList->GetData().items.clear();
            if (it.find(search->GetValue()) != std::string::npos)
                scriptsList->GetData().items.push_back(it);
        }
    });
    Event::Modify("BtnRefreshScripts", [&]() {
        console->AddLog({"刷新脚本列表", Console::LogData::LogInfo});
        scriptsList->GetData().items.clear();
        scripts.clear();
        for (auto&it: ScriptManager::Scan()) {
            scriptsList->GetData().items.push_back(RC::Utils::File::FileName(it));
            scripts.push_back(it);
        }
    });
    Event::Modify("BtnConfirm", [&]() {
        RecordingList->GetData().items.push_back(iptName->GetValue());
        auto item = std::ranges::find_if(
            tasks, [&](const std::shared_ptr<AutomationTask>&it) {
                return *it->Device == DevicesBox->GetSelectedItem();
            });
        if (item != tasks.end()) {
            (*item)->Replays[iptName->GetValue()] = ret;
        }
        window2->SetActive(false);
    });
    Event::Modify("BtnCancel", [&]() {
        window2->SetActive(false);
    });
    Event::Modify("BtnConfirm1", [&]() {
        window3->SetActive(false);
    });
    Event::Modify("BtnCancel1", [&]() {
        window3->SetActive(false);
    });
    Event::Modify("BtnGetDevices", [&]() {
        devices = ADBC::ADBClient::Devices(RC::Utils::File::PlatformPath("bin/platform-tools/adb"));
        DevicesBox->GetData().items = devices;
        console->AddLog({"刷新设备列表", Console::LogData::LogInfo});
    });
    Event::Modify("BtnRecord", [&]() {
        if (DevicesBox->GetData().items.empty()) {
            window3->SetActive(true);
            WarningText->GetData().text = "暂无可用设备";
            console->AddLog({"目前没有可用设备", Console::LogData::LogWarning});
            return;
        }
        std::shared_ptr<AutomationTask> task;
        auto item = std::ranges::find_if(
            tasks, [&](const std::shared_ptr<AutomationTask>&it) {
                return *it->Device == DevicesBox->GetSelectedItem();
            });

        if (item != tasks.end()) {
            task = *item;
            tasks.erase(item);
        }
        else
            task = AutomationTask::Create();
        *task->Device = DevicesBox->GetSelectedItem();
        task->state = AutomationTask::State::Recording;
        task->RecordingThread = std::thread([task,adbc,console]() {
            console->AddLog({"开始录制行为", Console::LogData::LogInfo});
            adbc->setID(*task->Device);
            adbc->startRecordingAct();
        });
        task->RecordingThread.detach();
        tasks.push_back(task);
    });
    Event::Modify("BtnStopRecord", [&]() {
        if (DevicesBox->GetData().items.empty()) {
            window3->SetActive(true);
            WarningText->GetData().text = "暂无可用设备";
            console->AddLog({"目前没有可用设备", Console::LogData::LogWarning});
            return;
        }
        auto item = std::ranges::find_if(
            tasks, [&](const std::shared_ptr<AutomationTask>&it) {
                return *it->Device == DevicesBox->GetSelectedItem();
            });
        if (item != tasks.end()) {
            window2->SetActive(true);
            console->AddLog({*item->get()->Device + ": 录制停止", Console::LogData::LogInfo});
            adbc->setID(*item->get()->Device);
            ret = adbc->stopRecordingAct();
            item->get()->state = AutomationTask::State::Idle;
        }
    });
    Event::Modify("BtnReplay", [&]() {
        if (DevicesBox->GetData().items.empty()) {
            window3->SetActive(true);
            WarningText->GetData().text = "暂无可用设备";
            console->AddLog({"目前没有可用设备", Console::LogData::LogWarning});
            return;
        }
        auto item = std::ranges::find_if(
            tasks, [&](const std::shared_ptr<AutomationTask>&it) {
                return *it->Device == DevicesBox->GetSelectedItem();
            });
        if (item != tasks.end()) {
            *item->get()->RunningScript = RecordingList->GetSelectedItem();
            console->AddLog({
                *item->get()->Device + ":开始回放行为: " + *item->get()->RunningScript, Console::LogData::LogInfo
            });

            item->get()->ScriptThread = std::thread([item,adbc,console,RecordingList] {
                adbc->setID(*item->get()->Device);
                item->get()->state = AutomationTask::State::Updating;
                if (item->get()->Replays.contains(*item->get()->RunningScript)) {
                    std::string name = *item->get()->RunningScript;
                    item->get()->running = true;
                    while (item->get()->state == AutomationTask::State::Updating) {
                        adbc->ReplayEvents(item->get()->Replays[name], item->get()->running);
                        console->AddLog({
                            *item->get()->Device + ":回放完成: " + *item->get()->RunningScript,
                            Console::LogData::LogInfo
                        });
                        item->get()->state = AutomationTask::State::Idle;
                    }
                }
            });
            item->get()->ScriptThread.detach();
        }
    });
    Event::Modify("BtnStopReplay", [&] {
        auto item = std::ranges::find_if(
            tasks, [&](const std::shared_ptr<AutomationTask>&it) {
                return *it->Device == DevicesBox->GetSelectedItem();
            });
        if (item->get() != nullptr) {
            console->AddLog({
                *item->get()->Device + ":停止回放: " + *item->get()->RunningScript, Console::LogData::LogInfo
            });
            item->get()->state = AutomationTask::State::Idle;
            item->get()->running = false;
        }
    });
    Event::Modify("BtnRun", [&] {
        if (DevicesBox->GetData().items.empty()) {
            window3->SetActive(true);
            WarningText->GetData().text = "暂无可用设备";
            return;
        }
        auto item = std::ranges::find_if(
            tasks, [&](const std::shared_ptr<AutomationTask>&it) {
                return *it->Device == DevicesBox->GetSelectedItem();
            });
        if (item == tasks.end()) {
            auto task = AutomationTask::Create();
            *task.get()->Device = DevicesBox->GetSelectedItem();
            tasks.emplace_back(task);
            item = tasks.end() - 1;
        }
        *item->get()->RunningScript = RC::Utils::File::FileName(scriptsList->GetSelectedItem());
        console->AddLog({
            *item->get()->Device + ":开始运行脚本: " + *item->get()->RunningScript, Console::LogData::LogInfo
        });
        item->get()->running = true;
        item->get()->ScriptThread = std::thread([item,adbc,&sm,scriptsList,&fr] {
            auto script = sm.GetScript(RC::Utils::File::FileName(scriptsList->GetSelectedItem()));

            adbc->setID(*item->get()->Device);
            Task(script, item->get()->running, fr, adbc, item->get()->state);
        });

        item->get()->ScriptThread.detach();
    });
    Event::Modify("BtnStopRun", [&] {
        if (DevicesBox->GetData().items.empty()) {
            window3->SetActive(true);
            WarningText->GetData().text = "暂无可用设备";
            return;
        }
        auto item = std::ranges::find_if(
            tasks, [&](const std::shared_ptr<AutomationTask>&it) {
                return *it->Device == DevicesBox->GetSelectedItem();
            });
        if (item != tasks.end()) {
            console->AddLog({
                *item->get()->Device + ":停止运行脚本: " + *item->get()->RunningScript, Console::LogData::LogInfo
            });
            item->get()->running = false;
        }
    });

    ImVec4 clear = {0.f, 0.f, 0.f, 1.f};
    while (!app.ShouldClose()) {
        runningList->GetData().items.clear();
        for (const auto&task: tasks) {
            runningList->GetData().items.emplace_back(
                *task.get()->Device + ": " + *task.get()->RunningScript + ": " + AutomationTask::stateToString(
                    task.get()->state));
        }
        app.SetClearColor(clear);
        app.Update();
    }

    LoadManager::Save(tasks, "events.yml");
    app.Shutdown();
    if (RC::Utils::File::Exists("Resources"))
        RC::Utils::Directory::Remove("Resources");
    return 0;
}