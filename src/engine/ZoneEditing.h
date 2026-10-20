#pragma once
#include "engine/LayoutModel.h"
#include <algorithm>
#include <cmath>

namespace layrix {
// Move a boundary and all adjoining edges without creating gaps or overlaps.
inline void ResizeZoneBoundary(std::vector<NormRect>& zones, const std::vector<NormRect>& original,
                               int selected, bool vertical, bool leading, double position,
                               double tolerance = 1e-6) {
    if (selected < 0 || selected >= static_cast<int>(original.size())) return;
    const auto& active = original[static_cast<size_t>(selected)];
    const double boundary = vertical ? active.x + (leading ? 0 : active.w) : active.y + (leading ? 0 : active.h);
    double spanStart = vertical ? active.y : active.x;
    double spanEnd = spanStart + (vertical ? active.h : active.w);
    const double epsilon = tolerance;
    constexpr double minimum = 0.04;
    double low = 0, high = 1;
    auto touches = [&](const NormRect& zone) {
        const double start = vertical ? zone.y : zone.x;
        const double end = start + (vertical ? zone.h : zone.w);
        return std::min(end, spanEnd) - std::max(start, spanStart) > epsilon;
    };
    // A long edge can meet several shorter edges. Follow the entire connected
    // boundary so moving either half also updates its other neighbours.
    for (size_t pass = 0; pass < original.size(); ++pass) {
        const double previousStart = spanStart, previousEnd = spanEnd;
        for (const auto& zone : original) {
            const double start = vertical ? zone.x : zone.y;
            const double end = start + (vertical ? zone.w : zone.h);
            if (!touches(zone) || (std::abs(start - boundary) >= epsilon && std::abs(end - boundary) >= epsilon)) continue;
            const double cross = vertical ? zone.y : zone.x;
            spanStart = std::min(spanStart, cross);
            spanEnd = std::max(spanEnd, cross + (vertical ? zone.h : zone.w));
        }
        if (previousStart == spanStart && previousEnd == spanEnd) break;
    }
    for (const auto& zone : original) {
        if (!touches(zone)) continue;
        const double start = vertical ? zone.x : zone.y;
        const double end = start + (vertical ? zone.w : zone.h);
        if (std::abs(start - boundary) < epsilon) high = std::min(high, end - minimum);
        if (std::abs(end - boundary) < epsilon) low = std::max(low, start + minimum);
    }
    if (low > high) return;
    position = std::clamp(position, low, high);
    for (size_t i = 0; i < original.size(); ++i) {
        const auto& before = original[i];
        if (!touches(before)) continue;
        const double start = vertical ? before.x : before.y;
        const double end = start + (vertical ? before.w : before.h);
        auto& after = zones[i];
        if (std::abs(start - boundary) < epsilon) {
            if (vertical) { after.x = position; after.w = end - position; }
            else { after.y = position; after.h = end - position; }
        }
        if (std::abs(end - boundary) < epsilon) {
            if (vertical) after.w = position - start;
            else after.h = position - start;
        }
    }
}
} // namespace layrix
