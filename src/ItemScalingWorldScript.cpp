/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#include "ItemScalingWorldScript.h"
#include "ItemScalingBaseline.h"
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

    sItemScalingConfig->Load();

    if (!sItemScalingConfig->Enable)
    {
        LOG_INFO("server.loading", ">> Mod-Item-Level-Scaling is disabled in configuration.");
        return;
    }

    // Build data-driven Blizzard item-level baseline model
    sItemScalingBaseline->BuildBaseline();

    // Reconstruct and register all persisted scaled item variants
    sItemScalingRegistry->Initialize();

    LOG_INFO("server.loading", ">> Mod-Item-Level-Scaling initialized successfully.");
}

void ItemScalingWorldScript::OnAfterConfigLoad(bool /*reload*/)
{
    sItemScalingConfig->Load();
}

void AddItemScalingWorldScripts()
{
    new ItemScalingWorldScript();
}
