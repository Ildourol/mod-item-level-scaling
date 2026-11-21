/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#include "ItemScalingEquipSpell.h"

#include "ObjectMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

namespace
{
    // Keep the name stem and targeting/attribute signature. Sharing an aura type
    // alone is insufficient (for example, ratings and damage schools differ).
    using Family = std::tuple<std::string_view, uint32, int32, int32, int32, uint32,
        uint32, uint32, uint32, uint32, uint32, uint32, uint32, uint32, uint32>;
    using Rank = std::pair<int32, int32>; // Magnitude, spell ID.
    std::map<Family, std::vector<Rank>> ladders;

    std::optional<Family> GetFamily(SpellInfo const* spell)
    {
        if (!spell || !spell->IsPassive() || !spell->IsPositive() || spell->SpellFamilyName != SPELLFAMILY_GENERIC ||
            spell->ProcFlags || spell->ProcCharges || sSpellMgr->GetSpellProcEntry(spell->Id) ||
            spell->StackAmount > 1 || spell->EquippedItemClass != -1 || spell->Stances || spell->StancesNot ||
            spell->CasterAuraSpell || spell->TargetAuraSpell || spell->ExcludeCasterAuraSpell ||
            spell->ExcludeTargetAuraSpell || spell->AreaGroupId > 0 || spell->GetDuration() > 0 ||
            spell->GetMaxDuration() > 0 || !spell->SpellName[0])
            return std::nullopt;

        auto const [firstScript, lastScript] = sObjectMgr->GetSpellScriptsBounds(spell->Id);
        if (firstScript != lastScript)
            return std::nullopt;

        SpellEffectInfo const& effect = spell->Effects[0];
        if (!effect.IsEffect(SPELL_EFFECT_APPLY_AURA) || effect.TargetA.GetTarget() != TARGET_UNIT_CASTER ||
            effect.TargetB.GetTarget() != Targets(0) || effect.TriggerSpell || effect.Amplitude ||
            effect.DieSides < 0 || effect.DieSides > 1 || effect.RealPointsPerLevel != 0 ||
            effect.PointsPerComboPoint != 0 || effect.ImplicitTargetConditions || effect.BasePoints <= 0 ||
            spell->Effects[1].IsEffect() || spell->Effects[2].IsEffect())
            return std::nullopt;

        switch (effect.ApplyAuraName)
        {
            case SPELL_AURA_MOD_STAT:
            case SPELL_AURA_MOD_RESISTANCE:
            case SPELL_AURA_MOD_ATTACK_POWER:
            case SPELL_AURA_MOD_RANGED_ATTACK_POWER:
            case SPELL_AURA_MOD_RATING:
            case SPELL_AURA_MOD_DAMAGE_DONE:
            case SPELL_AURA_MOD_HEALING_DONE:
                break;
            default:
                return std::nullopt;
        }

        // Stock spell names often end in their magnitude (e.g. "Increased Attack Power 20").
        // Strip that suffix only if it is the actual effect amount.
        std::string_view name(spell->SpellName[0]);
        std::size_t separator = name.find_last_of(' ');
        if (separator != std::string_view::npos)
        {
            int32 suffix = 0;
            auto end = name.data() + name.size();
            auto parsed = std::from_chars(name.data() + separator + 1, end, suffix);
            if (parsed.ec == std::errc{} && parsed.ptr == end && suffix == effect.CalcValue())
                name = name.substr(0, separator);
        }
        if (name.empty())
            return std::nullopt;

        return Family{name, effect.ApplyAuraName, effect.MiscValue, effect.MiscValueB,
            spell->GetDuration(), spell->SchoolMask, spell->Attributes, spell->AttributesEx,
            spell->AttributesEx2, spell->AttributesEx3, spell->AttributesEx4, spell->AttributesEx5,
            spell->AttributesEx6, spell->AttributesEx7, spell->AttributesCu};
    }
}

namespace ItemScalingEquipSpell
{
    void Initialize()
    {
        // Source replacement ranks from the running core's spell data.
        for (uint32 id = 1; id < sSpellMgr->GetSpellInfoStoreSize(); ++id)
        {
            SpellInfo const* spell = sSpellMgr->GetSpellInfo(id);
            if (auto family = GetFamily(spell))
                ladders[*family].emplace_back(spell->Effects[0].CalcValue(), id);
        }
    }

    bool IsEquivalent(int32 originalId, int32 replacementId)
    {
        if (originalId <= 0 || replacementId <= 0)
            return false;
        auto family = GetFamily(sSpellMgr->GetSpellInfo(originalId));
        return family && family == GetFamily(sSpellMgr->GetSpellInfo(replacementId));
    }

    void Scale(ItemTemplate& item, double budgetRatio)
    {
        if (budgetRatio == 1.0)
            return;

        for (auto& slot : item.Spells)
        {
            if (slot.SpellId <= 0 || slot.SpellTrigger != ITEM_SPELLTRIGGER_ON_EQUIP)
                continue;
            SpellInfo const* spell = sSpellMgr->GetSpellInfo(slot.SpellId);
            auto family = GetFamily(spell);
            if (!family)
                continue;
            auto ladder = ladders.find(*family);
            if (ladder == ladders.end())
                continue;

            int32 originalAmount = spell->Effects[0].CalcValue();
            double target = originalAmount * budgetRatio;
            double bestDistance = std::abs(target - originalAmount);
            int32 bestId = slot.SpellId;
            for (auto const& [amount, id] : ladder->second)
            {
                double distance = std::abs(target - amount);
                if (distance >= bestDistance)
                    continue;
                // Keep distinct item spell slots distinct after substitution.
                auto hasId = [id](auto const& other) { return other.SpellId == id; };
                if (std::none_of(std::begin(item.Spells), std::end(item.Spells), hasId))
                {
                    bestId = id;
                    bestDistance = distance;
                }
            }
            slot.SpellId = bestId;
        }
    }
}
