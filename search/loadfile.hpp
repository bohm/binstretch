#ifndef _LOADFILE_HPP
#define _LOADFILE_HPP 1

#include <optional>
#include <cstring>

#include "common.hpp"
#include "dag/dag.hpp"
#include "dag/partial.hpp"

// Read an input file and load the aforementioned tree into memory.

enum class line_type { adversary_vertex, algorithm_vertex, algorithm_outedge, adversary_outedge, command, header, footer, other};


line_type recognize(const char *line)
{
    // Current (slightly hacky) rules.

    if (strstr(line, "player=adv") != nullptr)
    {
	return line_type::adversary_vertex;
    } else if (strstr(line, "player=alg") != nullptr)
    {
	return line_type::algorithm_vertex;
    } else if (strstr(line, "->") != nullptr && strstr(line, "next") != nullptr)
    {
	return line_type::adversary_outedge;
    } else if (strstr(line, "->") != nullptr && strstr(line, "bin") != nullptr)
    {
	return line_type::algorithm_outedge;
    } else if (strstr(line, "strict digraph") != nullptr)
    {
	return line_type::header;
    } else if (strcmp(line, "}") != 0)
    {
	return line_type::footer;
    }

    return line_type::other;
}

// Parses a main command which is not a part of the graph,
// such as bins = 4; or R = 19;
void parse_command(const char* line)
{
    char start[255] = {0}, end[255] = {0};
    sscanf(line, "%s = %s;", start, end);

    if (strcmp(start, "R") == 0)
    {
	int r = 0;
	sscanf(end, "%d", &r);
	assert(r == R);
	return;
    } else if (strcmp(start, "S") == 0)
    {
	int s = 0;
	sscanf(end, "%d", &s);
	assert(s == S);
	return;
    } else if (strcmp(start, "BINS") == 0)
    {
	int bins = 0;
	sscanf(end, "%d", &bins);
	assert(bins == BINS);
	return;
    } else if (strcmp(start, "overlap") == 0)
    {
	// expected and harmless
	return;
    } else {
	ERROR("Parsed an unrecognizable command: %s.\n", line);
	return;
    }
}

std::tuple<int,int,int> parse_adv_outedge(const char *line)
{
    int name_from = -1, name_to = -1, next_item = -1;
    sscanf(line, "%d -> %d [next=%d]", &name_from, &name_to, &next_item);
    return std::make_tuple(name_from, name_to, next_item);
}

std::tuple<int,int,int> parse_alg_outedge(const char *line)
{
    int name_from = -1, name_to = -1, target_bin = -1;
    sscanf(line, "%d -> %d [bin=%d]", &name_from, &name_to, &target_bin);
    return std::make_tuple(name_from, name_to, target_bin);
}

std::tuple<int,std::string> parse_alg_vertex(const char *line)
{
    int name = -1;
    sscanf(line, "%d", &name);
    // Check for optimal="", and load the content.
    std::stringstream ss;
    const char *startoptimal = strstr(line, "optimal=\"");
    if (startoptimal != nullptr)
    {
	const char *after = startoptimal + 9;
	while (after != nullptr && (*after) != '\"')
	{
	    ss << *after;
	    after++;
	}
    }

    return std::make_tuple(name, ss.str());


}

std::tuple<int,bool,std::string> parse_adv_vertex(const char *line)
{
    int name = -1;
    sscanf(line, "%d", &name);
    bool is_sapling = false;
    if (strstr(line, "sapling=true") != nullptr)
    {
	is_sapling = true;
    }

    // Check for heur="", and load the content.
    std::stringstream ss;
    const char *startheur = strstr(line, "heur=\"");
    if (startheur != nullptr)
    {
	const char *after = startheur + 6;
	while (after != nullptr && (*after) != '\"')
	{
	    ss << *after;
	    after++;
	}
    }
    
    return std::make_tuple(name, is_sapling, ss.str());

}

// Builds a partial dag out of an input file.
// Currently very simple/lenient -- only recognizes each line and then runs the appropriate
// parser. Skips any line which it does not recognize.
partial_dag* loadfile(const char* filename)
{

    char line[256];

    partial_dag *pd = new partial_dag;
    
    FILE* fin = fopen(filename, "r");
    if (fin == NULL)
    {
	ERROR("Unable to open file %s\n", filename);
    }

    bool first_adversary = true;
    
    while(!feof(fin))
    {
	std::ignore = fgets(line, 255, fin);
	line_type l = recognize(line);
	int name = -1, name_from = -1, name_to = -1, bin = -1, next_item = -1;
	bool is_sapling = false;
	std::string heurstring;
	std::string optimal;

	switch(l)
	{
	case line_type::adversary_vertex:
	    std::tie(name, is_sapling, heurstring) = parse_adv_vertex(line);
	    if(name == -1)
	    {
		ERROR("Unable to parse adv. vertex line: %s\n", line);
	    }
	    if (first_adversary)
	    {
		pd->add_root(name, is_sapling, heurstring);
		first_adversary = false;
	    } else {
		pd->add_adv_vertex(name, is_sapling, heurstring);
	    }
	    break;
	case line_type::algorithm_vertex:
	    std::tie(name, optimal) = parse_alg_vertex(line);
	    if(name == -1)
	    {
		ERROR("Unable to parse alg. vertex line: %s\n", line);
	    }
	    
	    pd->add_alg_vertex(name, optimal);
	    
	    break;
	case line_type::adversary_outedge:
	    std::tie(name_from, name_to, next_item) = parse_adv_outedge(line);
	    if (name_from == -1 || name_to == -1 || next_item == -1)
	    {
		ERROR("Unable to parse adversary outedge line: %s\n", line);
	    }
	    pd->add_adv_outedge(name_from, name_to, next_item);
	    break;
	case line_type::algorithm_outedge:
	    std::tie(name_from, name_to, bin) = parse_alg_outedge(line);
	    if (name_from == -1 || name_to == -1 || bin == -1)
	    {
		ERROR("Unable to parse algorithm outedge line: %s\n", line);
	    }
	    pd->add_alg_outedge(name_from, name_to, bin);
	    break;
	default:
	    print_if<PARSING_DEBUG>("Skipping auxiliary line %s.\n", line);
	    break;
	} 
    }
    fclose(fin);
    return pd;
}



#endif // _LOADFILE_HPP
