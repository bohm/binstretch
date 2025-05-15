#pragma once

// Preset for disabling all heuristics, which leads to a pure exploration/generation minimax.
// Heuristic constants:
inline constexpr bool ADVERSARY_HEURISTICS = false;
inline constexpr bool EXPAND_HEURISTICS = false;
inline constexpr bool LARGE_ITEM_ACTIVE = false;
inline constexpr bool LARGE_ITEM_ACTIVE_EVERYWHERE = false;
inline constexpr bool FIVE_NINE_ACTIVE = false;
inline constexpr bool FIVE_NINE_ACTIVE_EVERYWHERE = false;

inline constexpr bool USING_HEURISTIC_VISITS = false;
inline constexpr bool HEURISTIC_VISITS_USING_CACHE = false;
inline constexpr bool HEURISTIC_VISITS_USING_MINIBS = false;
inline constexpr bool USING_HEURISTIC_KNOWNSUM = false;

constexpr bool KNOWNSUM_EXTENSION_GS5 = false;
constexpr bool USING_HEURISTIC_GS = false;
constexpr bool USING_KNOWNSUM_LOWSEND = false;
constexpr bool USING_MINIBINSTRETCHING = false;
