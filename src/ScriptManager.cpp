//
// Created by 92703 on 2024/7/26.
//

#include "ScriptManager.h"

sol::protected_function ScriptFunctions::operator[](int index) {
    return funcs[index];
}

void ScriptFunctions::Add(sol::protected_function func) {
    funcs.push_back(func);
}

void ScriptManager::Add(std::string scriptPath) {
    scriptsQueue.push(scriptPath);
}

void ScriptManager::Adds(std::vector<std::string> scriptPaths) {
    for (auto&scriptPath: scriptPaths) {
        scriptsQueue.push(scriptPath);
    }
}

void ScriptManager::Initialize() {
    while (!scriptsQueue.empty()) {
        std::string scriptPath = scriptsQueue.front();
        scriptsQueue.pop();
        std::shared_ptr<Script> script = std::make_shared<Script>();
        if (script->Initialize(scriptPath)) {
            scripts.push_back(script);
        }
    }
}

void ScriptManager::Delete(std::string scriptPath) {
    for (auto it = scripts.begin(); it != scripts.end(); ++it) {
        if ((*it)->scriptPath == scriptPath) {
            scripts.erase(it);
            break;
        }
    }
}

std::shared_ptr<Script> ScriptManager::GetScript(std::string scriptPath) {
    scriptPath = RC::Utils::File::PlatformPath(scriptPath);
    for (auto&script: scripts) {
        if (script->scriptPath == scriptPath) {
            return script;
        }
    }
    return nullptr;
}

std::vector<std::string> ScriptManager::Scan(std::string scriptsRoot) {
    return RC::Utils::Directory::List(scriptsRoot);
}

ScriptFunctions ScriptManager::GetFuncs(std::string funcName) const {
    ScriptFunctions funcs;
    for (auto&script: scripts) {
        funcs.Add(script->getFunction(funcName));
    }
    return funcs;
}
