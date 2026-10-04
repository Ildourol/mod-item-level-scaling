/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef MOD_ITEM_SCALING_LIVE_H
#define MOD_ITEM_SCALING_LIVE_H

#include "ItemScalingCommon.h"
#include "ObjectGuid.h"
#include <cstddef>
#include <memory>
#include <unordered_set>

class Player;
class Map;
class GameObject;
class WorldSession;
class WorldPacket;
struct Loot;

// All mutations of core-owned template values occur at the world-update barrier.
// Map hooks enqueue value snapshots only. No game object pointer survives a hook.
class ItemScalingLive
{
public:
    static ItemScalingLive* instance();
    ItemScalingLive();
    ~ItemScalingLive();

    bool RecoverStagedTemplates(); // startup, before ObjectMgr loads items
    bool ReserveSlots();          // startup, pre-allocates live generation template slots
    void Initialize();
    void Update(uint32 diff);
    void IncludeOwnedEntries(std::unordered_set<uint32>& entries) const;

    uint32 FindOrRequest(ItemTemplate const& base, VariantKey const& key, uint8 playerLevel);
    uint32 Find(VariantKey const& key) const;
    bool IsReserved(uint32 entry) const;
    void BeginLoot(Loot const& loot, Player const& owner);
    void TrackLoot(Loot const& loot, Player const& owner, std::size_t index, VariantKey const& key);
    void EnterMap(Map const& map, Player* player = nullptr);
    void CancelSource(uint32 map, uint32 instance, ObjectGuid source);
    bool DeferPacket(WorldSession* session, WorldPacket const& packet);
    bool PrepareChest(Player* player, GameObject* go);

    struct Diagnostics
    {
        bool enabled{false};
        std::size_t available{0};
        std::size_t pending{0};
        std::size_t durable{0};
        std::size_t ready{0};
        uint64 failures{0};
    };
    Diagnostics GetDiagnostics() const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

#define sItemScalingLive ItemScalingLive::instance()
void AddItemScalingLiveScripts();

#endif
