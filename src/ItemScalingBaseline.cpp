/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#include "ItemScalingBaseline.h"
#include "ItemScalingConfig.h"
#include "ItemScalingFormula.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Log.h"
#include "ObjectMgr.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

ItemScalingBaseline* ItemScalingBaseline::instance()
{
    static ItemScalingBaseline instance;
    return &instance;
}

void ItemScalingBaseline::BuildBaseline()
{
    if (_initialized)
    {
        return;
    }

    ItemTemplateContainer const* itemTemplates = sObjectMgr->GetItemTemplateStore();
    bool useStore = (itemTemplates && !itemTemplates->empty());

    // Temporary collector: [level][quality][family] -> list of item levels
    std::vector<uint32> buckets[MAX_BASELINE_LEVEL + 1][MAX_BASELINE_QUALITY][MAX_BASELINE_FAMILY];
    uint32 totalProcessed = 0;
    std::unordered_set<uint32> syntheticEntries;
    QueryResult variants = WorldDatabase.Query("SELECT variant_entry FROM scaled_item_variant");
    if (variants)
    {
        do
        {
            syntheticEntries.insert(variants->Fetch()[0].Get<uint32>());
        } while (variants->NextRow());
    }

    if (useStore)
    {
        for (auto const& pair : *itemTemplates)
        {
            ItemTemplate const& proto = pair.second;

            if (syntheticEntries.count(proto.ItemId))
                continue;

            if (proto.Class != ITEM_CLASS_WEAPON && proto.Class != ITEM_CLASS_ARMOR)
                continue;

            int8 family = ItemScalingFormula::GetSlotFamily(proto.InventoryType);
            if (family < 0 || family >= MAX_BASELINE_FAMILY)
                continue;

            if (proto.ScalingStatDistribution != 0 || proto.ScalingStatValue != 0)
                continue;

            if (proto.Quality >= MAX_BASELINE_QUALITY)
                continue;

            uint8 lvl = static_cast<uint8>(proto.RequiredLevel);
            if (lvl == 0 || lvl > MAX_BASELINE_LEVEL)
                continue;

            if (proto.ItemLevel == 0)
                continue;

            buckets[lvl][proto.Quality][family].push_back(proto.ItemLevel);
            ++totalProcessed;
        }
    }
    else
    {
        QueryResult result = WorldDatabase.Query(
            "SELECT entry, class, Quality, RequiredLevel, ItemLevel, InventoryType, ScalingStatDistribution, ScalingStatValue "
            "FROM item_template WHERE class IN (2, 4) AND RequiredLevel BETWEEN 1 AND 80 AND ItemLevel > 0 "
            "AND entry NOT IN (SELECT variant_entry FROM scaled_item_variant)"
        );

        if (result)
        {
            do
            {
                Field* f = result->Fetch();
                uint32 quality = f[2].Get<uint8>();
                uint8 lvl = f[3].Get<uint8>();
                uint32 ilvl = f[4].Get<uint16>();
                uint32 invType = f[5].Get<uint8>();
                uint32 ssd = f[6].Get<uint16>();
                uint32 ssv = f[7].Get<uint32>();

                if (ssd != 0 || ssv != 0)
                    continue;

                int8 family = ItemScalingFormula::GetSlotFamily(invType);
                if (family < 0 || family >= MAX_BASELINE_FAMILY)
                    continue;

                if (quality >= MAX_BASELINE_QUALITY || lvl == 0 || lvl > MAX_BASELINE_LEVEL)
                    continue;

                buckets[lvl][quality][family].push_back(ilvl);
                ++totalProcessed;
            } while (result->NextRow());
        }
    }

    // Calculate medians for populated buckets
    for (uint8 lvl = 1; lvl <= MAX_BASELINE_LEVEL; ++lvl)
    {
        for (uint8 q = 0; q < MAX_BASELINE_QUALITY; ++q)
        {
            for (uint8 f = 0; f < MAX_BASELINE_FAMILY; ++f)
            {
                auto& list = buckets[lvl][q][f];
                if (!list.empty())
                {
                    std::sort(list.begin(), list.end());
                    size_t mid = list.size() / 2;
                    if (list.size() % 2 == 1)
                    {
                        _medians[lvl][q][f] = static_cast<double>(list[mid]);
                    }
                    else
                    {
                        _medians[lvl][q][f] = (static_cast<double>(list[mid - 1]) + static_cast<double>(list[mid])) * 0.5;
                    }
                }
                else
                {
                    _medians[lvl][q][f] = 0.0;
                }
            }
        }
    }

    InterpolateMissingEntries();
    _initialized = true;

    LOG_INFO("server.loading", ">> ItemScaling: Built Blizzard item-level baseline model from {} equipment records.", totalProcessed);
}

void ItemScalingBaseline::InterpolateMissingEntries()
{
    // For each quality and family, fill gaps using nearest populated levels or proportional curve
    for (uint8 q = 0; q < MAX_BASELINE_QUALITY; ++q)
    {
        for (uint8 f = 0; f < MAX_BASELINE_FAMILY; ++f)
        {
            // Find lowest and highest populated levels
            uint8 minPop = 0;
            uint8 maxPop = 0;
            for (uint8 lvl = 1; lvl <= MAX_BASELINE_LEVEL; ++lvl)
            {
                if (_medians[lvl][q][f] > 0.0)
                {
                    if (minPop == 0)
                    {
                        minPop = lvl;
                    }
                    maxPop = lvl;
                }
            }

            if (minPop == 0)
            {
                // Quality/family completely unpopulated, fallback to quality-generic or linear formula
                // Typical Blizzard baseline for rare/epic is approx level + 5 (level 1-60) scaling to 200 at 80
                for (uint8 lvl = 1; lvl <= MAX_BASELINE_LEVEL; ++lvl)
                {
                    double qualityOffset = 0.0;
                    switch (q)
                    {
                        case ITEM_QUALITY_POOR:
                            qualityOffset = -5.0;
                            break;
                        case ITEM_QUALITY_NORMAL:
                            qualityOffset = 0.0;
                            break;
                        case ITEM_QUALITY_UNCOMMON:
                            qualityOffset = 5.0;
                            break;
                        case ITEM_QUALITY_RARE:
                            qualityOffset = 10.0;
                            break;
                        case ITEM_QUALITY_EPIC:
                            qualityOffset = 15.0;
                            break;
                        case ITEM_QUALITY_LEGENDARY:
                            qualityOffset = 25.0;
                            break;
                        default:
                            qualityOffset = 5.0;
                            break;
                    }
                    double estimatedIlvl = std::max(1.0, static_cast<double>(lvl) + qualityOffset);
                    if (lvl > 60)
                    {
                        // Expansion inflation scaling
                        double factor = 1.0 + (static_cast<double>(lvl - 60) / 20.0) * 1.5;
                        estimatedIlvl *= factor;
                    }
                    _medians[lvl][q][f] = estimatedIlvl;
                }
                continue;
            }

            // Extrapolate below minPop
            double minVal = _medians[minPop][q][f];
            for (uint8 lvl = 1; lvl < minPop; ++lvl)
            {
                _medians[lvl][q][f] = std::max(1.0, minVal * (static_cast<double>(lvl) / static_cast<double>(minPop)));
            }

            // Extrapolate above maxPop
            double maxVal = _medians[maxPop][q][f];
            for (uint8 lvl = maxPop + 1; lvl <= MAX_BASELINE_LEVEL; ++lvl)
            {
                _medians[lvl][q][f] = maxVal * (static_cast<double>(lvl) / static_cast<double>(maxPop));
            }

            // Interpolate internal gaps
            for (uint8 lvl = minPop + 1; lvl < maxPop; ++lvl)
            {
                if (_medians[lvl][q][f] == 0.0)
                {
                    // Find next populated
                    uint8 nextPop = lvl + 1;
                    while (nextPop < maxPop && _medians[nextPop][q][f] == 0.0)
                    {
                        ++nextPop;
                    }
                    uint8 prevPop = lvl - 1;
                    while (prevPop > minPop && _medians[prevPop][q][f] == 0.0)
                    {
                        --prevPop;
                    }

                    double pVal = _medians[prevPop][q][f];
                    double nVal = _medians[nextPop][q][f];
                    double fraction = static_cast<double>(lvl - prevPop) / static_cast<double>(nextPop - prevPop);
                    _medians[lvl][q][f] = pVal + fraction * (nVal - pVal);
                }
            }
        }
    }
}

double ItemScalingBaseline::GetMedianItemLevel(uint8 level, uint32 quality, uint8 slotFamily) const
{
    level = std::clamp<uint8>(level, 1, MAX_BASELINE_LEVEL);
    quality = std::min<uint32>(quality, MAX_BASELINE_QUALITY - 1);
    slotFamily = std::min<uint8>(slotFamily, MAX_BASELINE_FAMILY - 1);

    double val = _medians[level][quality][slotFamily];
    if (val > 0.0)
    {
        return val;
    }

    return static_cast<double>(level);
}

uint16 ItemScalingBaseline::CalculateTargetItemLevel(ItemTemplate const* baseProto, uint8 targetLevel, uint8 originalRefLevel) const
{
    if (!baseProto)
    {
        return 0;
    }

    uint32 i0 = baseProto->ItemLevel;
    if (i0 == 0)
    {
        i0 = std::max<uint32>(1, baseProto->RequiredLevel);
    }

    uint8 l0 = originalRefLevel;
    if (l0 == 0)
    {
        l0 = static_cast<uint8>(baseProto->RequiredLevel);
    }
    if (l0 == 0)
    {
        l0 = static_cast<uint8>(std::clamp<uint32>(i0, 1, 80));
    }

    uint8 lTarget = std::clamp<uint8>(targetLevel, 1, MAX_BASELINE_LEVEL);
    if (lTarget == l0)
    {
        return static_cast<uint16>(i0);
    }

    int8 family = ItemScalingFormula::GetSlotFamily(baseProto->InventoryType);
    if (family < 0)
    {
        family = 2; // fallback
    }

    uint32 q = std::min<uint32>(baseProto->Quality, MAX_BASELINE_QUALITY - 1);

    double m0 = GetMedianItemLevel(l0, q, static_cast<uint8>(family));
    double m1 = GetMedianItemLevel(lTarget, q, static_cast<uint8>(family));

    double targetIlvl = 0.0;
    if (m0 > 0.0 && m1 > 0.0)
    {
        targetIlvl = std::round(static_cast<double>(i0) * m1 / m0);
    }
    else
    {
        targetIlvl = std::round(static_cast<double>(i0) * static_cast<double>(lTarget) / static_cast<double>(std::max<uint8>(1, l0)));
    }

    // Clamp to valid WotLK ItemLevel range
    uint16 clampedIlvl = static_cast<uint16>(std::clamp<int32>(static_cast<int32>(targetIlvl), 1, 300));
    return clampedIlvl;
}
