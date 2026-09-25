/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#include "ItemScalingWorldScript.h"
#include "ItemScalingConfig.h"
#include "ItemScalingRegistry.h"
#include "Log.h"

ItemScalingWorldScript::ItemScalingWorldScript()
    : WorldScript("ItemScalingWorldScript", {
        WORLDHOOK_ON_STARTUP,
        WORLDHOOK_ON_LOAD_CUSTOM_DATABASE_TABLE,
        WORLDHOOK_ON_AFTER_CONFIG_LOAD
    })
{
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

void ItemScalingWorldScript::OnStartup()
{
    LOG_INFO("server.loading", ">> Initializing Mod-Item-Level-Scaling...");

    if (!sItemScalingConfig->Enable)
    {
        LOG_INFO("server.loading", ">> Mod-Item-Level-Scaling is disabled in configuration.");
        return;
    }

    // Index only templates the core has already loaded.
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
