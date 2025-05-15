#pragma once

// Switch from recursion based minimax to stack-based minimax.
// This is very handy for testing their identical properties.
// Ultimately, if the stack-based minimax proves to be overall faster, this might go away.
inline constexpr bool USING_RECURSION = true;