/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#include "ItemScalingFormula.h"
#include "ItemScalingConfig.h"
#include <algorithm>
#include <cmath>

namespace ItemScalingFormula
{
    int8 GetSlotFamily(uint32 inventoryType)
    {
        switch (inventoryType)
        {
            // Family 0: Head, Body/Chest, Legs, 2H Weapon, Robe
            case INVTYPE_HEAD:
            case INVTYPE_BODY:
            case INVTYPE_CHEST:
            case INVTYPE_LEGS:
            case INVTYPE_2HWEAPON:
            case INVTYPE_ROBE:
                return 0;

            // Family 1: Shoulders, Waist, Feet, Hands, Trinket
            case INVTYPE_SHOULDERS:
            case INVTYPE_WAIST:
            case INVTYPE_FEET:
            case INVTYPE_HANDS:
            case INVTYPE_TRINKET:
                return 1;

            // Family 2: Neck, Wrists, Finger, Shield, Cloak, Holdable, Relic
            case INVTYPE_NECK:
            case INVTYPE_WRISTS:
            case INVTYPE_FINGER:
            case INVTYPE_SHIELD:
            case INVTYPE_CLOAK:
            case INVTYPE_HOLDABLE:
            case INVTYPE_RELIC:
                return 2;

            // Family 3: 1H Weapon, Main-Hand, Off-Hand
            case INVTYPE_WEAPON:
            case INVTYPE_WEAPONMAINHAND:
            case INVTYPE_WEAPONOFFHAND:
                return 3;

            // Family 4: Ranged, Thrown, Ranged Right
            case INVTYPE_RANGED:
            case INVTYPE_THROWN:
            case INVTYPE_RANGEDRIGHT:
                return 4;

            default:
                return -1;
        }
    }

    double GetRandomPropertiesPoints(uint32 itemLevel, uint32 quality, uint8 slotFamily)
    {
        if (slotFamily > 4)
        {
            slotFamily = 2; // Relic / accessory fallback
        }

        RandomPropertiesPointsEntry const* entry = sRandomPropertiesPointsStore.LookupEntry(itemLevel);
        if (!entry)
        {
            // Search nearest available entry in DBC
            for (int32 delta = 1; delta <= 50; ++delta)
            {
                if (itemLevel > static_cast<uint32>(delta))
                {
                    entry = sRandomPropertiesPointsStore.LookupEntry(itemLevel - delta);
                    if (entry)
                    {
                        break;
                    }
                }
                entry = sRandomPropertiesPointsStore.LookupEntry(itemLevel + delta);
                if (entry)
                {
                    break;
                }
            }
        }

        if (!entry)
        {
            return static_cast<double>(std::max<uint32>(1, itemLevel));
        }

        uint32 points = 0;
        switch (quality)
        {
            case ITEM_QUALITY_POOR:
            case ITEM_QUALITY_NORMAL:
            case ITEM_QUALITY_UNCOMMON:
                points = entry->UncommonPropertiesPoints[slotFamily];
                break;
            case ITEM_QUALITY_RARE:
                points = entry->RarePropertiesPoints[slotFamily];
                break;
            case ITEM_QUALITY_EPIC:
            case ITEM_QUALITY_LEGENDARY:
            case ITEM_QUALITY_ARTIFACT:
            case ITEM_QUALITY_HEIRLOOM:
            default:
                points = entry->EpicPropertiesPoints[slotFamily];
                break;
        }

        return points > 0 ? static_cast<double>(points) : static_cast<double>(std::max<uint32>(1, itemLevel));
    }

    double GetBudgetRatio(uint32 baseIlvl, uint32 targetIlvl, uint32 quality, uint8 slotFamily)
    {
        if (baseIlvl == targetIlvl)
        {
            return 1.0;
        }

        double b0 = GetRandomPropertiesPoints(baseIlvl, quality, slotFamily);
        double b1 = GetRandomPropertiesPoints(targetIlvl, quality, slotFamily);

        if (b0 > 0.0)
        {
            return b1 / b0;
        }

        return static_cast<double>(targetIlvl) / static_cast<double>(std::max<uint32>(1, baseIlvl));
    }

    int32 ScaleAdditiveStat(int32 v0, double r, bool preserveNonZero)
    {
        if (v0 == 0)
        {
            return 0;
        }
        if (r <= 0.0)
        {
            return v0;
        }

        double scaled = static_cast<double>(v0) * r;
        int32 v1 = static_cast<int32>(std::round(scaled));

        if (preserveNonZero && v1 == 0)
        {
            v1 = (v0 > 0) ? 1 : -1;
        }

        return v1;
    }

    uint32 ScaleArmor(ItemTemplate const* baseProto, double rBudget, uint8 l0, uint8 lTarget)
    {
        if (!baseProto || baseProto->Armor == 0)
        {
            return 0;
        }

        if (l0 == lTarget)
        {
            return baseProto->Armor;
        }

        // Determine armor scaling mask
        uint32 mask = 0;
        if (baseProto->InventoryType == INVTYPE_CLOAK)
        {
            mask = 0x00080000; // Cloak
        }
        else if (baseProto->InventoryType == INVTYPE_SHOULDERS)
        {
            switch (baseProto->SubClass)
            {
                case ITEM_SUBCLASS_ARMOR_CLOTH:
                    mask = 0x00000020;
                    break;
                case ITEM_SUBCLASS_ARMOR_LEATHER:
                    mask = 0x00000040;
                    break;
                case ITEM_SUBCLASS_ARMOR_MAIL:
                    mask = 0x00000080;
                    break;
                case ITEM_SUBCLASS_ARMOR_PLATE:
                    mask = 0x00000100;
                    break;
                default:
                    break;
            }
        }
        else
        {
            switch (baseProto->SubClass)
            {
                case ITEM_SUBCLASS_ARMOR_CLOTH:
                    mask = 0x00100000;
                    break;
                case ITEM_SUBCLASS_ARMOR_LEATHER:
                    mask = 0x00200000;
                    break;
                case ITEM_SUBCLASS_ARMOR_MAIL:
                    mask = 0x00400000;
                    break;
                case ITEM_SUBCLASS_ARMOR_PLATE:
                    mask = 0x00800000;
                    break;
                default:
                    break;
            }
        }

        double rArmor = rBudget;
        if (mask != 0)
        {
            ScalingStatValuesEntry const* ssv0 = sScalingStatValuesStore.LookupEntry(l0);
            ScalingStatValuesEntry const* ssv1 = sScalingStatValuesStore.LookupEntry(lTarget);
            if (ssv0 && ssv1)
            {
                uint32 a0 = ssv0->getArmorMod(mask);
                uint32 a1 = ssv1->getArmorMod(mask);
                if (a0 > 0 && a1 > 0)
                {
                    rArmor = static_cast<double>(a1) / static_cast<double>(a0);
                }
            }
        }

        int32 scaledArmor = static_cast<int32>(std::round(static_cast<double>(baseProto->Armor) * rArmor));
        return static_cast<uint32>(std::max(1, scaledArmor));
    }

    void ScaleWeaponDamage(ItemTemplate const* baseProto, ItemTemplate& scaledProto, double rBudget, uint8 l0, uint8 lTarget)
    {
        if (!baseProto)
        {
            return;
        }

        if (l0 == lTarget)
        {
            for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
            {
                scaledProto.Damage[i] = baseProto->Damage[i];
            }
            return;
        }

        // Determine weapon DPS mask
        bool isCaster = false;
        for (uint32 i = 0; i < baseProto->StatsCount; ++i)
        {
            uint32 st = baseProto->ItemStat[i].ItemStatType;
            if (st == ITEM_MOD_SPELL_POWER || st == ITEM_MOD_INTELLECT || st == ITEM_MOD_SPIRIT)
            {
                isCaster = true;
                break;
            }
        }

        uint32 mask = 0;
        switch (baseProto->InventoryType)
        {
            case INVTYPE_WEAPON:
            case INVTYPE_WEAPONMAINHAND:
            case INVTYPE_WEAPONOFFHAND:
                mask = isCaster ? 0x00000800 : 0x00000200; // Caster 1H vs Weapon 1H
                break;
            case INVTYPE_2HWEAPON:
                mask = isCaster ? 0x00001000 : 0x00000400; // Caster 2H vs Weapon 2H
                break;
            case INVTYPE_RANGED:
            case INVTYPE_THROWN:
            case INVTYPE_RANGEDRIGHT:
                if (baseProto->SubClass == ITEM_SUBCLASS_WEAPON_WAND)
                {
                    mask = 0x00004000; // Wand
                }
                else
                {
                    mask = 0x00002000; // Ranged
                }
                break;
            default:
                break;
        }

        double rDPS = rBudget;
        if (mask != 0)
        {
            ScalingStatValuesEntry const* ssv0 = sScalingStatValuesStore.LookupEntry(l0);
            ScalingStatValuesEntry const* ssv1 = sScalingStatValuesStore.LookupEntry(lTarget);
            if (ssv0 && ssv1)
            {
                uint32 dps0 = ssv0->getDPSMod(mask);
                uint32 dps1 = ssv1->getDPSMod(mask);
                if (dps0 > 0 && dps1 > 0)
                {
                    rDPS = static_cast<double>(dps1) / static_cast<double>(dps0);
                }
            }
        }

        for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            float min0 = baseProto->Damage[i].DamageMin;
            float max0 = baseProto->Damage[i].DamageMax;
            scaledProto.Damage[i].DamageType = baseProto->Damage[i].DamageType;

            if (min0 <= 0.0f && max0 <= 0.0f)
            {
                scaledProto.Damage[i].DamageMin = 0.0f;
                scaledProto.Damage[i].DamageMax = 0.0f;
                continue;
            }

            float avg0 = (min0 + max0) * 0.5f;
            float spread = (max0 + min0 > 0.0f) ? ((max0 - min0) / (max0 + min0)) : 0.0f;

            float avg1 = avg0 * static_cast<float>(rDPS);
            float min1 = avg1 * (1.0f - spread);
            float max1 = avg1 * (1.0f + spread);

            if (min1 < 1.0f && avg0 > 0.0f)
            {
                min1 = 1.0f;
            }
            if (max1 < min1)
            {
                max1 = min1;
            }

            scaledProto.Damage[i].DamageMin = std::round(min1);
            scaledProto.Damage[i].DamageMax = std::round(max1);
        }
    }

    uint8 CalculateRequiredLevel(ItemTemplate const* baseProto, uint8 targetEffectiveLevel, uint8 highestRealPlayerLevel)
    {
        if (!baseProto)
        {
            return 0;
        }

        switch (sItemScalingConfig->ReqLevelPolicy)
        {
            case REQ_POLICY_PLAYER:
                return std::clamp<uint8>(highestRealPlayerLevel, 1, 80);
            case REQ_POLICY_TARGET:
                return std::clamp<uint8>(targetEffectiveLevel, 1, 80);
            case REQ_POLICY_TARGET_CAPPED_PLAYER:
            default:
                return std::clamp<uint8>(std::min<uint8>(targetEffectiveLevel, highestRealPlayerLevel), 1, 80);
        }
    }

    bool IsScalableEquipment(ItemTemplate const* proto)
    {
        if (!proto)
        {
            return false;
        }

        // Combat equipment only
        if (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR)
        {
            return false;
        }

        int8 family = GetSlotFamily(proto->InventoryType);
        if (family < 0)
        {
            return false;
        }

        // Exclude native heirlooms unless explicitly configured
        if (!sItemScalingConfig->ScaleExistingScalingItems &&
            (proto->ScalingStatDistribution != 0 || proto->ScalingStatValue != 0))
        {
            return false;
        }

        // Exclude qualities based on config
        if (!sItemScalingConfig->IsQualityEnabled(proto->Quality))
        {
            return false;
        }

        // Exclude specific configured item IDs
        if (sItemScalingConfig->IsItemExcluded(proto->ItemId))
        {
            return false;
        }

        // Must contain at least one scalable combat attribute
        bool hasScalableField = (proto->StatsCount > 0) ||
                                (proto->Armor > 0) ||
                                (proto->Block > 0) ||
                                (proto->Damage[0].DamageMax > 0.0f) ||
                                (proto->HolyRes != 0) ||
                                (proto->FireRes != 0) ||
                                (proto->NatureRes != 0) ||
                                (proto->FrostRes != 0) ||
                                (proto->ShadowRes != 0) ||
                                (proto->ArcaneRes != 0);

        return hasScalableField;
    }

    ItemTemplate CreateScaledTemplate(ItemTemplate const* baseProto, uint32 newEntry, uint8 targetEffectiveLevel, uint16 targetItemLevel, uint8 /*formulaVersion*/, uint8 highestRealPlayerLevel)
    {
        // 1. Full clone of base item template
        ItemTemplate scaledProto = *baseProto;

        // 2. Assign synthetic entry
        scaledProto.ItemId = newEntry;

        uint8 l0 = static_cast<uint8>(baseProto->RequiredLevel);
        if (l0 == 0)
        {
            l0 = std::clamp<uint8>(static_cast<uint8>(baseProto->ItemLevel), 1, 80);
        }

        uint8 lTarget = std::clamp<uint8>(targetEffectiveLevel, 1, 80);

        // 3. Assign scaled item level & required level
        scaledProto.ItemLevel = targetItemLevel;
        scaledProto.RequiredLevel = CalculateRequiredLevel(baseProto, lTarget, highestRealPlayerLevel);

        // 4. Calculate budget growth ratio
        int8 family = GetSlotFamily(baseProto->InventoryType);
        if (family < 0)
        {
            family = 2;
        }

        double rBudget = GetBudgetRatio(baseProto->ItemLevel, targetItemLevel, baseProto->Quality, static_cast<uint8>(family));

        // 5. Scale additive stats
        for (uint32 i = 0; i < baseProto->StatsCount; ++i)
        {
            scaledProto.ItemStat[i].ItemStatType = baseProto->ItemStat[i].ItemStatType;
            scaledProto.ItemStat[i].ItemStatValue = ScaleAdditiveStat(
                baseProto->ItemStat[i].ItemStatValue,
                rBudget,
                sItemScalingConfig->PreserveNonZeroStats
            );
        }

        // 6. Scale armor
        scaledProto.Armor = ScaleArmor(baseProto, rBudget, l0, lTarget);

        // 7. Scale weapon damage
        ScaleWeaponDamage(baseProto, scaledProto, rBudget, l0, lTarget);

        // 8. Scale shield block value
        if (baseProto->Block > 0)
        {
            scaledProto.Block = static_cast<uint32>(ScaleAdditiveStat(baseProto->Block, rBudget, false));
        }

        // 9. Scale resistances
        scaledProto.HolyRes = ScaleAdditiveStat(baseProto->HolyRes, rBudget, false);
        scaledProto.FireRes = ScaleAdditiveStat(baseProto->FireRes, rBudget, false);
        scaledProto.NatureRes = ScaleAdditiveStat(baseProto->NatureRes, rBudget, false);
        scaledProto.FrostRes = ScaleAdditiveStat(baseProto->FrostRes, rBudget, false);
        scaledProto.ShadowRes = ScaleAdditiveStat(baseProto->ShadowRes, rBudget, false);
        scaledProto.ArcaneRes = ScaleAdditiveStat(baseProto->ArcaneRes, rBudget, false);

        return scaledProto;
    }
}
