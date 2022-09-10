#ifndef _FUNCTIONS_HPP
#define _FUNCTIONS_HPP

// Small helper functions that do not need to be adjusted often.


void binary_print(FILE* stream, uint64_t num)
{
    uint64_t bit = 0;
    for (int i = 63; i >= 0; i--)
    {
	bit = num >> i;
	if (bit % 2 == 0)
	{
	    fprintf(stream, "0");
	} else
	{
	    fprintf(stream, "1");
	}
    }
}

#endif
