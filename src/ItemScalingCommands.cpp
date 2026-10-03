/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#include "ItemScalingCommands.h"
#include "Chat.h"
#include "CommandScript.h"
#include "ItemScalingBaseline.h"
#include "ItemScalingCommon.h"
#include "ItemScalingConfig.h"
#include "ItemScalingFormula.h"
#include "ItemScalingRegistry.h"
#include "ItemScalingLive.h"
#include "ItemScalingSafety.h"
#include "ItemTemplate.h"
#include "Player.h"
#include <algorithm>

using namespace Acore::ChatCommands;

static char const* GetStatName(uint32 statType)
{
    switch (statType)
    {
        case ITEM_MOD_MANA: return "Mana";
        case ITEM_MOD_HEALTH: return "Health";
        case ITEM_MOD_AGILITY: return "Agility";
        case ITEM_MOD_STRENGTH: return "Strength";
        case ITEM_MOD_INTELLECT: return "Intellect";
        case ITEM_MOD_SPIRIT: return "Spirit";
        case ITEM_MOD_STAMINA: return "Stamina";
        case ITEM_MOD_DEFENSE_SKILL_RATING: return "Defense Rating";
        case ITEM_MOD_DODGE_RATING: return "Dodge Rating";
        case ITEM_MOD_PARRY_RATING: return "Parry Rating";
        case ITEM_MOD_BLOCK_RATING: return "Block Rating";
        case ITEM_MOD_HIT_MELEE_RATING: return "Hit Rating (Melee)";
        case ITEM_MOD_HIT_RANGED_RATING: return "Hit Rating (Ranged)";
        case ITEM_MOD_HIT_SPELL_RATING: return "Hit Rating (Spell)";
        case ITEM_MOD_CRIT_MELEE_RATING: return "Crit Rating (Melee)";
        case ITEM_MOD_CRIT_RANGED_RATING: return "Crit Rating (Ranged)";
        case ITEM_MOD_CRIT_SPELL_RATING: return "Crit Rating (Spell)";
        case ITEM_MOD_HIT_RATING: return "Hit Rating";
        case ITEM_MOD_CRIT_RATING: return "Crit Rating";
        case ITEM_MOD_RESILIENCE_RATING: return "Resilience";
        case ITEM_MOD_HASTE_RATING: return "Haste Rating";
        case ITEM_MOD_ATTACK_POWER: return "Attack Power";
        case ITEM_MOD_RANGED_ATTACK_POWER: return "Ranged Attack Power";
        case ITEM_MOD_SPELL_HEALING_DONE: return "Healing";
        case ITEM_MOD_SPELL_DAMAGE_DONE: return "Spell Damage";
        case ITEM_MOD_MANA_REGENERATION: return "MP5";
        case ITEM_MOD_ARMOR_PENETRATION_RATING: return "Armor Penetration";
        case ITEM_MOD_SPELL_POWER: return "Spell Power";
        case ITEM_MOD_HEALTH_REGEN: return "HP5";
        case ITEM_MOD_SPELL_PENETRATION: return "Spell Penetration";
        case ITEM_MOD_BLOCK_VALUE: return "Block Value";
        default: return "Stat";
    }
}

static char const* GetPolicyName(RequiredLevelPolicy policy)
{
    switch (policy)
    {
        case REQ_POLICY_TARGET: return "target";
        case REQ_POLICY_PLAYER: return "player";
        case REQ_POLICY_TARGET_CAPPED_PLAYER: return "target-capped-player";
        default: return "unknown";
    }
}

class ItemScaling_CommandScript : public CommandScript
{
public:
    ItemScaling_CommandScript() : CommandScript("ItemScaling_CommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable itemScalingCommandTable =
        {
            { "status",  HandleItemScalingStatusCommand,  SEC_GAMEMASTER, Console::Yes },
            { "preview", HandleItemScalingPreviewCommand, SEC_GAMEMASTER, Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "itemscaling", itemScalingCommandTable },
            { "mils",        itemScalingCommandTable },
        };

        return commandTable;
    }

    static bool HandleItemScalingStatusCommand(ChatHandler* handler)
    {
        handler->PSendSysMessage("|cff3399ff=== [Item Level Scaling Status & Diagnostics] ===|r");
        handler->PSendSysMessage("Module State: {}", sItemScalingConfig->Enable ? "|cff00ff00ENABLED|r" : "|cffff0000DISABLED|r");
        handler->PSendSysMessage("Scaling Method: {}", sItemScalingConfig->Method == SCALING_METHOD_DYNAMIC ? "Dynamic" : "Fixed");
        handler->PSendSysMessage("AutoBalance Synergy: {}", sItemScalingConfig->UseAutoBalanceSettings ? "|cff00ff00ACTIVE (Default 1 - Adopting AutoBalance Settings)|r" : "|cffff8000INACTIVE (Using Standalone Config)|r");
        handler->PSendSysMessage("Dynamic Dungeons: Floor -{}, Ceiling +{}", sItemScalingConfig->DynamicFloorDungeons, sItemScalingConfig->DynamicCeilingDungeons);
        handler->PSendSysMessage("Dynamic Raids: Floor -{}, Ceiling +{}", sItemScalingConfig->DynamicFloorRaids, sItemScalingConfig->DynamicCeilingRaids);
        handler->PSendSysMessage("Directional Scaling: ScaleUp={}, ScaleDown={}", sItemScalingConfig->ScaleUp ? "Yes" : "No", sItemScalingConfig->ScaleDown ? "Yes" : "No");
        handler->PSendSysMessage("RealPlayersOnly: {}, IncludeGameMasters: {}", sItemScalingConfig->RealPlayersOnly ? "Yes" : "No", sItemScalingConfig->IncludeGameMasters ? "Yes" : "No");
        handler->PSendSysMessage("RequiredLevel Policy: {}", GetPolicyName(sItemScalingConfig->ReqLevelPolicy));
        handler->PSendSysMessage("RandomSuffix Mode: {} (0=Skip, 1=Bake)", static_cast<uint32>(sItemScalingConfig->RandomSuffixMode));
        handler->PSendSysMessage("Formula Version: {}, Generator Revision: {}", sItemScalingConfig->FormulaVersion, ITEM_SCALING_GENERATOR_REVISION);
        handler->PSendSysMessage("Database Synchronized: {}", sItemScalingRegistry->IsDbSynchronized() ? "|cff00ff00YES|r" : "|cffff0000NO|r");
        handler->PSendSysMessage("Registry Initialized: {}", sItemScalingRegistry->IsInitialized() ? "|cff00ff00YES|r" : "|cffff0000NO|r");
        handler->PSendSysMessage("In-Memory Scaled Variants: |cff00ff00{}|r", sItemScalingRegistry->GetIndexedVariantCount());
        handler->PSendSysMessage("Allocated Synthetic Entry IDs: |cff00ff00{}|r", sItemScalingRegistry->GetSyntheticEntryCount());
        auto live = sItemScalingLive->GetDiagnostics();
        handler->PSendSysMessage("Live Scaling: {} | Mode: {} (1=Entry, 2=Drop) | Available slots: {}",
            live.enabled ? "Enabled" : "Inactive", uint32(sItemScalingConfig->LiveGenerationMode), live.available);
        handler->PSendSysMessage("Live Variants: Pending={} Durable={} Ready={} | Failures={}",
            live.pending, live.durable, live.ready, live.failures);
        handler->PSendSysMessage("|cffaaaaaaUsage: .itemscaling preview <itemLink|itemId> [targetLevel]|r");
        return true;
    }

    static bool HandleItemScalingPreviewCommand(ChatHandler* handler, ItemTemplate const* item, Optional<uint8> targetLevel)
    {
        if (!item)
        {
            handler->PSendSysMessage("|cffff0000Item not found.|r Usage: .itemscaling preview <itemLink|itemId> [targetLevel]");
            return false;
        }

        uint8 lTarget = 80;
        if (targetLevel.has_value())
        {
            lTarget = std::clamp<uint8>(*targetLevel, 1, 80);
        }
        else if (Player* player = handler->GetPlayer())
        {
            lTarget = player->GetLevel();
        }

        uint8 origRefLevel = static_cast<uint8>(item->RequiredLevel);
        if (origRefLevel == 0)
        {
            origRefLevel = static_cast<uint8>(std::clamp<uint32>(item->ItemLevel, 1, 80));
        }

        handler->PSendSysMessage("|cff3399ff=== [Item Scaling Preview] ===|r");
        handler->PSendSysMessage("Item: |cffffff00{}|r (ID: {}, Quality: {}, Type: {}/{})",
            item->Name1, item->ItemId, item->Quality, item->Class, item->SubClass);

        bool isScalableEquipment = ItemScalingFormula::IsScalableEquipment(item);
        if (!isScalableEquipment)
        {
            handler->PSendSysMessage("|cffff0000[INELIGIBLE]|r Item is not scalable equipment (InventoryType: {}).", item->InventoryType);
            return true;
        }

        if (item->MaxCount != 0 || item->StartQuest != 0 || item->ScriptId != 0 || item->HasFlag(ITEM_FLAG_UNIQUE_EQUIPPABLE))
        {
            handler->PSendSysMessage("|cffff8000[RESTRICTED]|r Item has MaxCount/Quest/Script/Unique-Equip flags; preserved natively on real drop.");
        }

        if (!sItemScalingConfig->IsQualityEnabled(item->Quality))
        {
            handler->PSendSysMessage("|cffff8000[NOTE]|r Quality {} is currently disabled in ItemScaling configuration.", item->Quality);
        }

        if (sItemScalingConfig->IsItemExcluded(item->ItemId))
        {
            handler->PSendSysMessage("|cffff8000[NOTE]|r Item ID {} is explicitly excluded in configuration.", item->ItemId);
        }

        if (sItemScalingConfig->IsLevelExcluded(origRefLevel))
        {
            handler->PSendSysMessage("|cffff8000[NOTE]|r Native level {} is in ItemScaling.ExcludedLevels.", origRefLevel);
        }

        if (origRefLevel == lTarget)
        {
            handler->PSendSysMessage("|cffff8000[NOTE]|r Native level {} matches target level {}. Original item drops without scaling.", origRefLevel, lTarget);
        }

        if (!sItemScalingConfig->ScaleDown && lTarget < origRefLevel)
        {
            handler->PSendSysMessage("|cffff8000[NOTE]|r ScaleDown is disabled in configuration; downscaling will be skipped in real drops.");
        }
        else if (!sItemScalingConfig->ScaleUp && lTarget > origRefLevel)
        {
            handler->PSendSysMessage("|cffff8000[NOTE]|r ScaleUp is disabled in configuration; upscaling will be skipped in real drops.");
        }

        // Calculate target ilvl
        uint16 targetIlvl = sItemScalingBaseline->CalculateTargetItemLevel(item, lTarget, origRefLevel);
        if (targetIlvl == 0)
        {
            handler->PSendSysMessage("|cffff0000[ERROR]|r Failed to calculate target item level for level {}.", lTarget);
            return true;
        }

        // Generate preview scaled template
        ItemTemplate scaledProto = ItemScalingFormula::CreateScaledTemplate(item, 0, lTarget, targetIlvl,
            sItemScalingConfig->FormulaVersion, lTarget);

        // Check if indexed in memory
        uint32 existingEntry = sItemScalingRegistry->FindExistingVariant(item, lTarget, targetIlvl,
            sItemScalingConfig->FormulaVersion, lTarget);

        handler->PSendSysMessage("Target Player Level: |cff00ff00{}|r (Native Level: {})", lTarget, origRefLevel);
        handler->PSendSysMessage("Item Level (iLvl): |cffffd100{}|r -> |cff00ff00{}|r | Required Level: |cffffd100{}|r -> |cff00ff00{}|r",
            item->ItemLevel, scaledProto.ItemLevel, item->RequiredLevel, scaledProto.RequiredLevel);

        if (existingEntry != 0)
        {
            handler->PSendSysMessage("Synthetic Variant Status: |cff00ff00INDEXED IN-MEMORY (ID: {})|r", existingEntry);
        }
        else
        {
            handler->PSendSysMessage("Synthetic Variant Status: |cffffd100Not ready (generated on entry/drop in live mode, or next restart in legacy mode)|r");
        }

        // Armor
        if (item->Armor > 0 || scaledProto.Armor > 0)
        {
            handler->PSendSysMessage("Armor: |cffffd100{}|r -> |cff00ff00{}|r", item->Armor, scaledProto.Armor);
        }

        // Weapon Damage & DPS
        if (item->Class == ITEM_CLASS_WEAPON && item->Delay > 0)
        {
            float speed = item->Delay / 1000.0f;
            float baseDps = ((item->Damage[0].DamageMin + item->Damage[0].DamageMax) / 2.0f) / speed;
            float scaledDps = ((scaledProto.Damage[0].DamageMin + scaledProto.Damage[0].DamageMax) / 2.0f) / speed;
            handler->PSendSysMessage("Weapon Speed: {:.2f}s | Damage: |cffffd100{}-{}|r -> |cff00ff00{}-{}|r | DPS: |cffffd100{:.1f}|r -> |cff00ff00{:.1f}|r",
                speed, static_cast<uint32>(item->Damage[0].DamageMin), static_cast<uint32>(item->Damage[0].DamageMax),
                static_cast<uint32>(scaledProto.Damage[0].DamageMin), static_cast<uint32>(scaledProto.Damage[0].DamageMax),
                baseDps, scaledDps);
        }

        // Primary & Secondary Stats
        if (scaledProto.StatsCount > 0 || item->StatsCount > 0)
        {
            handler->PSendSysMessage("|cff33ccff--- Scaled Item Stats ---|r");
            for (uint32 i = 0; i < scaledProto.StatsCount; ++i)
            {
                uint32 statType = scaledProto.ItemStat[i].ItemStatType;
                int32 scaledVal = scaledProto.ItemStat[i].ItemStatValue;
                int32 baseVal = 0;
                for (uint32 j = 0; j < item->StatsCount; ++j)
                {
                    if (item->ItemStat[j].ItemStatType == statType)
                    {
                        baseVal = item->ItemStat[j].ItemStatValue;
                        break;
                    }
                }
                char const* statName = GetStatName(statType);
                if (baseVal != 0)
                {
                    handler->PSendSysMessage("  {}: |cffffd100+{}|r -> |cff00ff00+{}|r", statName, baseVal, scaledVal);
                }
                else
                {
                    handler->PSendSysMessage("  {}: |cff00ff00+{}|r (Bonus/Baked)", statName, scaledVal);
                }
            }
        }

        // Resistances
        if (scaledProto.HolyRes != item->HolyRes && (scaledProto.HolyRes > 0 || item->HolyRes > 0))
            handler->PSendSysMessage("  Holy Resistance: {} -> {}", item->HolyRes, scaledProto.HolyRes);
        if (scaledProto.FireRes != item->FireRes && (scaledProto.FireRes > 0 || item->FireRes > 0))
            handler->PSendSysMessage("  Fire Resistance: {} -> {}", item->FireRes, scaledProto.FireRes);
        if (scaledProto.NatureRes != item->NatureRes && (scaledProto.NatureRes > 0 || item->NatureRes > 0))
            handler->PSendSysMessage("  Nature Resistance: {} -> {}", item->NatureRes, scaledProto.NatureRes);
        if (scaledProto.FrostRes != item->FrostRes && (scaledProto.FrostRes > 0 || item->FrostRes > 0))
            handler->PSendSysMessage("  Frost Resistance: {} -> {}", item->FrostRes, scaledProto.FrostRes);
        if (scaledProto.ShadowRes != item->ShadowRes && (scaledProto.ShadowRes > 0 || item->ShadowRes > 0))
            handler->PSendSysMessage("  Shadow Resistance: {} -> {}", item->ShadowRes, scaledProto.ShadowRes);
        if (scaledProto.ArcaneRes != item->ArcaneRes && (scaledProto.ArcaneRes > 0 || item->ArcaneRes > 0))
            handler->PSendSysMessage("  Arcane Resistance: {} -> {}", item->ArcaneRes, scaledProto.ArcaneRes);

        return true;
    }
};

void AddItemScalingCommandScripts()
{
    new ItemScaling_CommandScript();
}
