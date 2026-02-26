#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <type_traits>
#include <utility>
#include <cassert>
#include <algorithm>

#include "gameobject.h"
#include "SnowFlakeid.h"

class IScene;

class ObjectManager {
public:
    explicit ObjectManager(IScene* ownerScene)
        : m_scene(ownerScene)
    {
        assert(m_scene);
        m_snowflake = std::make_unique<Snowflake>(m_scene->getmachineid());
    }

    ObjectManager(const ObjectManager&) = delete;
    ObjectManager& operator=(const ObjectManager&) = delete;

    // -------------------------
    // ローカル生成（ID自動採番）
    // -------------------------
    template<class T, class... Args>
    T* Create(ObjectKind kind, Args&&... args) {
        static_assert(std::is_base_of_v<gameobject, T>, "T must derive from gameobject");
        static_assert(std::is_constructible_v<T, IScene*, Args...>,
            "T must be constructible with (IScene*, Args...)");

        const ObjectId id = GenerateId();
        return CreateWithIdInternal<T>(id, kind, std::forward<Args>(args)...);
    }

    template<class T, class... Args>
    T* CreateLocal(Args&&... args) {
        return Create<T>(ObjectKind::Local, std::forward<Args>(args)...);
    }

    template<class T, class... Args>
    T* CreateRemote(Args&&... args) {
        // ★注意：これは「リモートだけどIDは自動」になってしまうので
        // 使い分けしたいなら残しても良いが、通常は下の CreateRemoteWithId を使う。
        return Create<T>(ObjectKind::Remote, std::forward<Args>(args)...);
    }

    // -------------------------
    // ★追加：リモート生成（外部からID指定）
    // -------------------------
    template<class T, class... Args>
    T* CreateRemoteWithId(ObjectId id, Args&&... args) {
        static_assert(std::is_base_of_v<gameobject, T>, "T must derive from gameobject");
        static_assert(std::is_constructible_v<T, IScene*, Args...>,
            "T must be constructible with (IScene*, Args...)");

        return CreateWithIdInternal<T>(id, ObjectKind::Remote, std::forward<Args>(args)...);
    }

    // 既に存在するIDか？
    bool Exists(ObjectId id) const {
        return m_objects.find(id) != m_objects.end();
    }

    gameobject* FindById(ObjectId id) const {
        auto it = m_objects.find(id);
        return (it == m_objects.end()) ? nullptr : it->second.get();
    }

    void DestroyById(ObjectId id) {
        if (m_objects.find(id) == m_objects.end()) return;
        m_destroyQueue.push_back(id);
    }

    void Destroy(gameobject* obj) {
        if (!obj) return;
        DestroyById(obj->GetObjectId());
    }

    void UpdateAll(std::uint64_t dt) {
        for (auto id : m_aliveOrder) {
            auto it = m_objects.find(id);
            if (it == m_objects.end()) continue;
            it->second->update(dt);
        }
        FlushDestroyQueue();
    }

    void DrawAll(std::uint64_t dt) {
        for (auto id : m_aliveOrder) {
            auto it = m_objects.find(id);
            if (it == m_objects.end()) continue;
            it->second->draw(dt);
        }
    }

    template<class T, class Func>
    void ForEach(Func&& func) {
        for (auto id : m_aliveOrder) {
            auto it = m_objects.find(id);
            if (it == m_objects.end()) continue;

            T* typed = dynamic_cast<T*>(it->second.get());
            if (!typed) continue;

            func(*typed);
        }
    }

    void Clear() {
        for (auto& kv : m_objects) {
            if (kv.second) kv.second->dispose();
        }
        m_objects.clear();
        m_aliveOrder.clear();
        m_destroyQueue.clear();
    }

private:
    ObjectId GenerateId() {
        return m_snowflake->next_id();
    }

    // -------------------------
    // 共通：ID指定で生成する内部関数
    // -------------------------
    template<class T, class... Args>
    T* CreateWithIdInternal(ObjectId id, ObjectKind kind, Args&&... args) {
        // ★ID衝突は必ず弾く（ネットワークの重複受信やバグ検出にもなる）
        if (Exists(id)) {
            // ここは assert / ログ / nullptr返す など運用に合わせて
            assert(false && "ObjectId already exists");
            return nullptr;
        }

        auto obj = std::make_unique<T>(m_scene, std::forward<Args>(args)...);

        // 種別とIDを注入
        obj->_SetObjectId(id);
        obj->_SetKind(kind);

        T* raw = static_cast<T*>(obj.get());

        m_objects.emplace(id, std::move(obj));
        m_aliveOrder.push_back(id);

        raw->init();
        return raw;
    }

    void FlushDestroyQueue() {
        if (m_destroyQueue.empty()) return;

        std::sort(m_destroyQueue.begin(), m_destroyQueue.end());
        m_destroyQueue.erase(std::unique(m_destroyQueue.begin(), m_destroyQueue.end()), m_destroyQueue.end());

        for (auto id : m_destroyQueue) {
            auto it = m_objects.find(id);
            if (it == m_objects.end()) continue;

            it->second->dispose();

            auto oit = std::find(m_aliveOrder.begin(), m_aliveOrder.end(), id);
            if (oit != m_aliveOrder.end()) m_aliveOrder.erase(oit);

            m_objects.erase(it);
        }
        m_destroyQueue.clear();
    }

private:
    IScene* m_scene = nullptr;

    std::unordered_map<ObjectId, std::unique_ptr<gameobject>> m_objects;
    std::vector<ObjectId> m_aliveOrder;
    std::vector<ObjectId> m_destroyQueue;

    std::unique_ptr<Snowflake> m_snowflake{};
};
