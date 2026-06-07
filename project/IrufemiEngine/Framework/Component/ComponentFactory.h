#pragma once
#include <string>
#include <memory>
#include <map>
#include <functional>
#include "Component.h"

class ComponentFactory {
public:
    using CreatorFunc = std::function<std::shared_ptr<Component>()>;

    static void Register(const std::string& typeName, CreatorFunc func);
    static std::shared_ptr<Component> Create(const std::string& typeName);
    static const std::map<std::string, CreatorFunc>& GetFactoryMap();

    /// @brief エンジン組み込みのコンポーネントを一括登録する
    static void RegisterAllCoreComponents();

private:
    static std::map<std::string, CreatorFunc>& GetMap();
};
