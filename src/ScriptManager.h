#ifndef SCRIPTMANAGER_H
#define SCRIPTMANAGER_H
#include <string>
#include <vector>
#include "Script.h"

struct ScriptFunctions {
    using iterator = std::vector<sol::protected_function>::iterator;

    iterator begin() {
        return funcs.begin();
    }

    iterator end() {
        return funcs.end();
    }

    template<typename T, typename... Args>
    std::vector<T> Invokes(Args&&... args);

    template<typename... Args>
    void Invokes(Args&&... args);

    sol::protected_function operator[](int index);

    void Add(sol::protected_function func);

private:
    std::vector<sol::protected_function> funcs;
};

class ScriptManager {
public:
    ScriptManager() = default;

    void Add(std::string scriptPath);

    void Adds(std::vector<std::string>);

    void Initialize();

    void Delete(std::string scriptPath);

    std::shared_ptr<Script> GetScript(std::string scriptPath);


    template<typename T, typename... Args>
    std::vector<T> Invokes(std::string funcName, Args&&... args);

    template<typename T, typename... Args>
    std::vector<T> Invokes(ScriptFunctions funcs, Args&&... args);

    template<typename... Args>
    void Invokes(std::string funcName, Args&&... args);

    template<typename... Args>
    void Invokes(ScriptFunctions funcs, Args&&... args);

    static std::vector<std::string> Scan(std::string scriptsRoot = "scripts");

    ScriptFunctions GetFuncs(std::string funcName) const;

private:
    std::string scriptsRoot = "scripts";
    std::queue<std::string> scriptsQueue;
    std::vector<std::shared_ptr<Script>> scripts;
};

template<typename T, typename... Args>
std::vector<T> ScriptFunctions::Invokes(Args&&... args) {
    std::vector<T> results;
    for (auto&func: funcs) {
        results.push_back(func(std::forward<Args>(args)...));
    }
    return results;
}

template<typename... Args>
void ScriptFunctions::Invokes(Args&&... args) {
    for (auto&func: funcs) {
        sol::protected_function_result r = func(std::forward<Args>(args)...);
        if (!r.valid()) {
            sol::error err = r;
            std::cerr << "Error call function: " << err.what() << std::endl;
        }
    }
}

template<typename T, typename... Args>
std::vector<T> ScriptManager::Invokes(std::string funcName, Args&&... args) {
    std::vector<T> results;
    for (auto&script: scripts) {
        results.push_back(script->Invoke(funcName, std::forward<Args>(args)...));
    }
    return results;
}

template<typename T, typename... Args>
std::vector<T> ScriptManager::Invokes(ScriptFunctions funcs, Args&&... args) {
    std::vector<T> results;
    for (auto&func: funcs) {
        results.push_back(func(std::forward<Args>(args)...));
    }
    return results;
}

template<typename... Args>
void ScriptManager::Invokes(ScriptFunctions funcs, Args&&... args) {
    for (auto&func: funcs) {
        sol::protected_function_result r = func(std::forward<Args>(args)...);
        if (!r.valid()) {
            sol::error err = r;
            std::cerr << "Error call function: " << err.what() << std::endl;
        }
    }
}

template<typename... Args>
void ScriptManager::Invokes(std::string funcName, Args&&... args) {
    for (auto&script: scripts) {
        script->Invoke(funcName, std::forward<Args>(args)...);
    }
}


#endif //SCRIPTMANAGER_H
