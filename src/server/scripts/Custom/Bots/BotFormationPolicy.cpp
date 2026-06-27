/*
 * Player-bot formation policy helpers -- see BotFormationPolicy.h.
 */

#include "BotFormationPolicy.h"
#include <cctype>
#include <cmath>

namespace
{
    constexpr float BOT_FORM_PI     = 3.14159265358979f;
    constexpr float BOT_FORM_BASE   = 2.5f;   // base follow depth behind the master (yd)
    constexpr float BOT_FORM_STEP   = 2.5f;   // spacing between adjacent slots (yd)
    constexpr float BOT_FORM_RADIUS = 3.5f;   // circle radius (yd)

    // forward = along the master's facing (+ = in front), side = perpendicular.
    FormationOffset FromCartesian(float forward, float side)
    {
        return FormationOffset{ std::sqrt(forward * forward + side * side),
                                std::atan2(side, forward) };
    }
}

namespace BotFormationPolicy
{
FormationOffset Offset(BotFormation preset, uint32 slot, uint32 count)
{
    switch (preset)
    {
        case BotFormation::Column:
            return FromCartesian(-(BOT_FORM_BASE + float(slot) * BOT_FORM_STEP), 0.0f);

        case BotFormation::Line:
        {
            float const center = float(count > 0 ? count - 1 : 0) * 0.5f;
            return FromCartesian(-BOT_FORM_BASE, (float(slot) - center) * BOT_FORM_STEP);
        }

        case BotFormation::Wedge:
        {
            uint32 const pair = slot / 2 + 1;
            float const sign = (slot % 2 == 0) ? 1.0f : -1.0f;
            return FromCartesian(-(float(pair) * BOT_FORM_STEP), sign * float(pair) * BOT_FORM_STEP);
        }

        case BotFormation::Circle:
        default:
        {
            uint32 const n = count > 0 ? count : 1;
            return FormationOffset{ BOT_FORM_RADIUS, (2.0f * BOT_FORM_PI * float(slot)) / float(n) };
        }
    }
}

Optional<BotFormation> Parse(std::string_view name)
{
    auto eq = [](std::string_view a, char const* b)
    {
        std::string_view const bv(b);
        if (a.size() != bv.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(a[i])) != static_cast<unsigned char>(bv[i]))
                return false;
        return true;
    };

    if (eq(name, "line"))   return BotFormation::Line;
    if (eq(name, "wedge"))  return BotFormation::Wedge;
    if (eq(name, "circle")) return BotFormation::Circle;
    if (eq(name, "column")) return BotFormation::Column;
    return {};
}
}
