/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#include "ItemScalingWorldScript.h"
#include "ItemScalingConfig.h"
#include "ItemScalingRegistry.h"
#include "Log.h"

namespace
{
    // Stable module salt: changes to client-visible synthetic item metadata can bump this value deliberately.
    constexpr uint32 ITEM_SCALING_CLIENT_CACHE_SALT = 0x49534C01u;
}

ItemScalingWorldScript::ItemScalingWorldScript()
    : WorldScript("ItemScalingWorldScript", {
        WORLDHOOK_ON_BEFORE_WORLD_INITIALIZED,
        WORLDHOOK_ON_BEFORE_FINALIZE_PLAYER_WORLD_SESSION,
        WORLDHOOK_ON_LOAD_CUSTOM_DATABASE_TABLE,
        WORLDHOOK_ON_AFTER_CONFIG_LOAD
    })
{
}

void ItemScalingWorldScript::OnBeforeFinalizePlayerWorldSession(uint32& cacheVersion)
{
    if (sItemScalingConfig->Enable)
        cacheVersion ^= ITEM_SCALING_CLIENT_CACHE_SALT;
}

void ItemScalingWorldScript::OnLoadCustomDatabaseTable()
{
    sItemScalingConfig->Load();

    if (!sItemScalingConfig->Enable)
    {
        return;
    }

    sItemScalingRegistry->OnLoadCustomDatabaseTable();
}

void ItemScalingWorldScript::OnBeforeWorldInitialized()
{
    LOG_INFO("server.loading", ">> Initializing Mod-Item-Level-Scaling...");

    if (!sItemScalingConfig->Enable)
    {
        LOG_INFO("server.loading", ">> Mod-Item-Level-Scaling is disabled in configuration.");
        return;
    }

    // Re-publish persisted scaled fields on top of the core-validated base template metadata,
    // then index the variants before the world network becomes connectable.
    sItemScalingRegistry->Initialize();
}

void ItemScalingWorldScript::OnAfterConfigLoad(bool reload)
{
    // Startup-generated templates depend on an immutable configuration snapshot.
    if (reload)
    {
        LOG_WARN("module.ItemScaling", "ItemScaling configuration changes require a server restart.");
        return;
    }
    sItemScalingConfig->Load();
}

void AddItemScalingWorldScripts()
{
    new ItemScalingWorldScript();
}
