#pragma once
#include <cstdint>
#include <cassert>

#include "../system/transform.h"
#include "../system/IScene.h"

using ObjectId = std::uint64_t;

enum class ObjectKind : std::uint8_t {
    Local,   // 自分の入力などで動く
    Remote,  // ネットワークからの同期で動く（入力しない）
};

class gameobject {
public:
    gameobject() = delete;

    explicit gameobject(IScene* currentscene)
        : m_ownerscene(currentscene) {
        assert(m_ownerscene && "gameobject requires a valid scene pointer");
    }

    virtual ~gameobject() = default;
    virtual void update(uint64_t delta) = 0;
    virtual void draw(uint64_t delta) = 0;
    virtual void init() = 0;
    virtual void dispose() = 0;

    // ---- SRT ----
    const SRT& getSRT() const { return m_srt; }
    void setSRT(const SRT& srt) { m_srt = srt; }
    void setPosition(const Vector3& pos) { m_srt.pos = pos; }

    // ---- Scene ----
    IScene* GetOwnerScene() const { return m_ownerscene; }

    // ---- Object identity / kind ----
    ObjectId GetObjectId() const { return m_objectId; }
    ObjectKind GetKind() const { return m_kind; }
    bool IsLocal() const { return m_kind == ObjectKind::Local; }
    bool IsRemote() const { return m_kind == ObjectKind::Remote; }

    // ObjectManager だけが設定できるようにする
    friend class ObjectManager;
    void _SetObjectId(ObjectId id) { m_objectId = id; }
    void _SetKind(ObjectKind kind) { m_kind = kind; }

protected:
    SRT m_srt{};
    IScene* m_ownerscene = nullptr;

private:
    ObjectId   m_objectId = 0;               // 0 = invalid
    ObjectKind m_kind = ObjectKind::Local;
};
