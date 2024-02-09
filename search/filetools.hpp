#pragma once

#include <ctime>
#include <iostream>
#include <sstream>

#include "common.hpp"
#include "functions.hpp"
#include "binconf.hpp"

void folder_checks() {
    if (!fs::is_directory(LOG_DIR)) {
        fprintf(stderr, "The log directory is not found, terminating.\n");
        exit(-1);
    }

    if (!fs::is_directory(OUTPUT_DIR)) {
        fprintf(stderr, "The output directory is not found, terminating.\n");
        exit(-1);
    }
}

std::string filename_timestamp(const std::tm *time) {
    char bufstr[1000];
    strftime(bufstr, sizeof(bufstr), "--%Y-%m-%d--%H-%M-%S", time);
    std::string ret(bufstr);
    return ret;
}

std::string filename_binstamp() {
    std::stringstream ss;
    ss << BINS;
    ss << "bins-";
    ss << R;
    ss << "-";
    ss << S;
    return ss.str();
}

// Builds a filename for the top of the tree, which is output to logs/
// for analysis/logging purposes.
// We pass the timestamp so that this file and other log files have the same stamp.
std::string build_treetop_filename(std::tm *timestamp) {
    std::string binstamp = filename_binstamp();
    std::string timestamp_str = filename_timestamp(timestamp);
    std::string log_path = LOG_DIR + "/" + binstamp + timestamp_str + "-treetop.log";

    return log_path;

}

std::string build_output_filename(std::tm *timestamp) {
    std::string binstamp = filename_binstamp();
    std::string timestamp_str = filename_timestamp(timestamp);
    std::string gen_path = OUTPUT_DIR + "/" + binstamp + timestamp_str + "-tree.gen";
    return gen_path;
}


// A quick and dirty load binconf for a startup of a computation.
// In the grand scheme of things, this should not be needed, but to create
// a full loadfile takes too long.


std::array<int, BINS + 1> load_segment_with_loads(std::stringstream &str_s) {
    std::array<int, BINS + 1> ret = {};

    char c = 0;
    int load = -1;
    str_s.get(c);
    if (c != '[') {
        ERRORPRINT("Missing opening bracket '[' at the start of the load list.\n");
    }

    for (int i = 1; i <= BINS; i++) {
        str_s >> load;
        if (load < 0 || load >= R) {
            ERRORPRINT("The %d-th load from the loads list is out of bounds.", i);
        }
        ret[i] = load;
        load = -1;
    }

    str_s.get(c);
    if (c != ']') {
        ERRORPRINT("Missing closing bracket ']' at the end of the load list.\n");
    }
    return ret;
}

template<int SCALE>
std::array<int, SCALE + 1> load_segment_with_items(std::stringstream &str_s) {
    std::array<int, SCALE + 1> ret = {};

    char c = 0;
    int item_size = -1;
    str_s.get(c);
    if (c != ' ') {
        ERRORPRINT("The separator between loads and items does not match ' '.\n");
    }
    str_s.get(c);
    if (c != '(') {
        ERRORPRINT("Missing opening bracket '(' at the start of the item list.\n");
    }

    for (int j = 1; j <= SCALE; j++) {
        str_s >> item_size;
        if (item_size < 0 || item_size > BINS * SCALE) {
            ERRORPRINT("The %d-th item from the items segment (value %d) is out of bounds.\n", j, item_size);
        }

        ret[j] = item_size;
        item_size = -1;
    }

    str_s.get(c);
    if (c != ')') {
        ERRORPRINT("Missing closing bracket ')' at the end of the item list.\n");
    }
    return ret;
}

int load_last_item_segment(std::stringstream &str_s) {
    int last_item = -1;
    str_s >> last_item;

    if (last_item < 0 || last_item > BINS * S) {
        ERRORPRINT("Could not scan the last item field from the input file.\n");
    }
    return last_item;
}

binconf loadbinconf(std::stringstream &str_s) {
    std::array<int, BINS + 1> loads = load_segment_with_loads(str_s);
    std::array<int, S + 1> items = load_segment_with_items<S>(str_s);
    int last_item = load_last_item_segment(str_s);

    binconf retbc(loads, items, last_item);
    retbc.hashinit();
    return retbc;

}

binconf loadbinconf_singlefile(const char *filename) {
    FILE *fin = fopen(filename, "r");
    if (fin == NULL) {
        ERRORPRINT("Unable to open file %s\n", filename);
    }

    char linebuf[20000];
    char *retptr = fgets(linebuf, 20000, fin);
    if (retptr == nullptr) {
        ERRORPRINT("File %s found, but a line could not be loaded.\n", filename);
    }

    fclose(fin);
    std::string line(linebuf);
    std::stringstream str_s(line);

    return loadbinconf(str_s);
}


class debug_logger {
public:
    FILE *logfile;

    void init_logfile(std::string filename) {
        std::string path = LOG_DIR + "/" + filename;
        logfile = fopen(path.c_str(), "a");
        // fprintf(stderr, "File %s open for writing.\n", path.c_str());
        if (logfile == nullptr) {
            ERRORPRINT("File %s not possible to be opened.\n", path.c_str());
        }
    }

    debug_logger(std::string filename) {
        init_logfile(filename);
    }

    debug_logger(int worker_id) {
        init_logfile(std::string("workerlog") + std::to_string(worker_id) + std::string(".txt"));
    }

    void log_binconf(const binconf *b) {
        print_binconf_stream(logfile, b, true);
    }


    void log_loadconf(const loadconf *b) {
        print_loadconf_stream(logfile, b, true);
    }

    void log_loadconf(const loadconf *b, std::string s) {
        print_loadconf_stream(logfile, b, false);
        fprintf(logfile, " %s\n", s.c_str());
    }

    void log_binconf_with_move(binconf *b, int pres_item, int target_bin) {
        binconf copy(b->loads, b->ic.items, b->last_item);
        copy.assign_and_rehash(pres_item, target_bin);
        print_binconf_stream(logfile, &copy, true);
    }

    void log_loadconf_with_move(binconf *b, int pres_item, int target_bin) {
        loadconf copy(*b, pres_item, target_bin);
        print_loadconf_stream(logfile, &copy, true);
    }

    ~debug_logger() {
        fclose(logfile);
    }
};

thread_local debug_logger *dlog = nullptr;
