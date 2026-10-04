/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef ITEM_SCALING_VARIANCE_H
#define ITEM_SCALING_VARIANCE_H

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

struct VarianceWeights
{
    // Index 0: -3, Index 1: -2, Index 2: -1, Index 3: +1, Index 4: +2, Index 5: +3
    float chances[6]{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    [[nodiscard]] bool HasAny() const
    {
        for (float c : chances)
        {
            if (c > 0.0f)
            {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] float TotalWeight() const
    {
        float sum = 0.0f;
        for (float c : chances)
        {
            sum += c;
        }
        return sum;
    }
};

namespace ItemScalingVariance
{
    [[nodiscard]] inline std::string_view TrimWhitespace(std::string_view sv)
    {
        while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r' || sv.front() == '\n'))
        {
            sv.remove_prefix(1);
        }
        while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r' || sv.back() == '\n'))
        {
            sv.remove_suffix(1);
        }
        return sv;
    }

    [[nodiscard]] inline float ParseFloat(std::string_view sv)
    {
        sv = TrimWhitespace(sv);
        if (sv.empty())
        {
            return 0.0f;
        }

        std::string s(sv);
        char* end = nullptr;
        float val = std::strtof(s.c_str(), &end);
        return (end != s.c_str()) ? val : 0.0f;
    }

    [[nodiscard]] inline VarianceWeights ParseWeights(std::string_view configStr)
    {
        VarianceWeights weights;
        if (configStr.empty())
        {
            return weights;
        }

        bool hasColon = configStr.find(':') != std::string_view::npos;
        if (hasColon)
        {
            // Named pair syntax: "-1: 20.0, -2: 10.0, -3: 5.0"
            std::size_t start = 0;
            while (start < configStr.size())
            {
                std::size_t comma = configStr.find(',', start);
                std::string_view token = (comma == std::string_view::npos)
                    ? configStr.substr(start)
                    : configStr.substr(start, comma - start);

                start = (comma == std::string_view::npos) ? configStr.size() : comma + 1;

                token = TrimWhitespace(token);
                if (token.empty())
                {
                    continue;
                }

                std::size_t colon = token.find(':');
                if (colon == std::string_view::npos)
                {
                    continue;
                }

                std::string_view deltaStr = TrimWhitespace(token.substr(0, colon));
                std::string_view pctStr = TrimWhitespace(token.substr(colon + 1));

                if (!deltaStr.empty() && deltaStr.front() == '+')
                {
                    deltaStr.remove_prefix(1);
                }

                char* end = nullptr;
                std::string dStr(deltaStr);
                long delta = std::strtol(dStr.c_str(), &end, 10);
                if (end == dStr.c_str())
                {
                    continue;
                }

                float pct = ParseFloat(pctStr);
                if (pct < 0.0f)
                {
                    pct = 0.0f;
                }

                int idx = -1;
                switch (delta)
                {
                    case -3: idx = 0; break;
                    case -2: idx = 1; break;
                    case -1: idx = 2; break;
                    case 1:  idx = 3; break;
                    case 2:  idx = 4; break;
                    case 3:  idx = 5; break;
                    default: break;
                }

                if (idx >= 0)
                {
                    weights.chances[idx] = pct;
                }
            }
        }
        else
        {
            // Positional comma-separated syntax: "5.0, 10.0, 20.0, 0, 0, 0"
            std::size_t start = 0;
            std::size_t idx = 0;
            while (start < configStr.size() && idx < 6)
            {
                std::size_t comma = configStr.find(',', start);
                std::string_view token = (comma == std::string_view::npos)
                    ? configStr.substr(start)
                    : configStr.substr(start, comma - start);

                start = (comma == std::string_view::npos) ? configStr.size() : comma + 1;

                token = TrimWhitespace(token);
                if (!token.empty())
                {
                    float pct = ParseFloat(token);
                    weights.chances[idx] = std::max(0.0f, pct);
                }
                ++idx;
            }
        }

        return weights;
    }

    [[nodiscard]] inline std::int8_t RollDelta(VarianceWeights const& weights, float randomRoll)
    {
        if (!weights.HasAny())
        {
            return 0;
        }

        float total = weights.TotalWeight();
        if (total <= 0.0f)
        {
            return 0;
        }

        static constexpr std::int8_t deltas[6] = {-3, -2, -1, 1, 2, 3};
        float cumulative = 0.0f;
        for (std::size_t i = 0; i < 6; ++i)
        {
            if (weights.chances[i] <= 0.0f)
            {
                continue;
            }

            cumulative += weights.chances[i];
            if (randomRoll < cumulative)
            {
                return deltas[i];
            }
        }

        return 0;
    }
}

#endif // ITEM_SCALING_VARIANCE_H
