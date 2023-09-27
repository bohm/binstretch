#pragma once

// A test that the variable I_SCALE is provided. The main (search) code will not require this check, but
// some minitools will make use of it.


#ifndef I_SCALE
#error "The macro constant I_SCALE needs to be passed by the compiler!"
#define I_SCALE 3 // This line is a small trick to make G++ spit out only the error above.
#endif

constexpr int MINITOOL_MINIBS_SCALE = I_SCALE;