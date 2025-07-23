# pragma once
#include "constants.hpp"

// Default setting for heuristics. Formerly in common.hpp, but we keep it separate now so that individual
// executables can choose their own heuristic presets.

// Heuristic constants:
inline constexpr bool ADVERSARY_HEURISTICS = true;
inline constexpr bool EXPAND_HEURISTICS = false;
inline constexpr bool LARGE_ITEM_ACTIVE = true;
inline constexpr bool LARGE_ITEM_ACTIVE_EVERYWHERE = false;
inline constexpr bool FIVE_NINE_ACTIVE = false;
inline constexpr bool FIVE_NINE_ACTIVE_EVERYWHERE = false;

inline constexpr bool USING_HEURISTIC_VISITS = true;
inline constexpr bool HEURISTIC_VISITS_USING_CACHE = true;
inline constexpr bool HEURISTIC_VISITS_USING_MINIBS = true;
inline constexpr bool USING_HEURISTIC_KNOWNSUM = false; // Recommend turning off when WEIGHTSUM is true.

// GS5+ extension. GS5+ is one of the currently only good situations which
// makes use of tracking items -- in this case, items of size at least alpha
// and at most 1-alpha. In this sense, it is not a valid GS for the game of known sum of
// processing times, because this game does not handle combinatorics.

// However, if we are solving bin stretching ultimately, we might want to wish to turn it on already for
// the known sum layer, because this is the backbone of winning positions.

// Currently, it applies only for the setting of BINS == 3, so that the winning tables produced by our tools
// such as all-losing or alg-winning-table provide more meaningful results this way.
    
// Setting KNOWNSUM_EXTENSION_GS5 to false makes the computation structurally cleaner,
// as there are no special cases, but the understanding of winning and losing
// positions does not match the human understanding exactly.

// Setting KNOWNSUM_EXTENSION_GS5 to true should include more winning positions in the system,
// which helps performance.

constexpr bool KNOWNSUM_EXTENSION_GS5 = true && (BINS == 3);

constexpr bool USING_HEURISTIC_GS = false;
constexpr bool USING_KNOWNSUM_LOWSEND = false;
constexpr bool USING_MINIBINSTRETCHING = true;
