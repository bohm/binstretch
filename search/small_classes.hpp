#pragma once

// Small classes and enums that can be shared throughout the code.

// Victory states.
// The meaning of uncertain = yet to be evaluated, not enough data, postponed (perhaps).
// The maaning of irrelevant = no longer important, can be freely passed above, computation is over.

enum class victory {
    uncertain, alg, adv, irrelevant
};

void print(FILE *stream, const victory &win) {
    switch (win) {
        case victory::uncertain:
            fprintf(stream, "uncertain");
            break;
        case victory::adv:
            fprintf(stream, "adv wins");
            break;
        case victory::alg:
            fprintf(stream, "alg wins");
            break;
        case victory::irrelevant:
            fprintf(stream, "irrelevant");
            break;
    }
}

template<bool PARAM>
void print(FILE *stream, const victory &win) {
    if (PARAM) {
        print(stream, win);
    }
}

// Types of adversarial heuristics.
// enum class heuristic {simple, large_item, five_nine };
enum class heuristic {
    large_item, five_nine
};

void print_heuristic(FILE *stream, const heuristic &type) {
    switch (type) {
//     case heuristic::simple:
// 	fprintf(stream, "simple"); break;
        case heuristic::large_item:
            fprintf(stream, "large item");
            break;
        case heuristic::five_nine:
            fprintf(stream, "five/nine");
            break;
    }
}

template<bool PARAM>
void print_heuristic(FILE *stream, const heuristic &type) {
    if (PARAM) {
        print_heuristic(stream, type);
    }
}


// Minimax states.
enum class minimax {
    generating, exploring, updating
};

// Some helper macros:

#define GENERATING (MODE == minimax::generating)
#define EXPLORING (MODE == minimax::exploring)

#define GEN_ONLY(x) if (MODE == minimax::generating) {x;}
#define EXP_ONLY(x) if (MODE == minimax::exploring) {x;}


// States of a task.
enum class task_status {
    available, batched, pruned, alg_win, adv_win, irrelevant
};

enum class updater_states {
    postponed, terminating, overdue, irrelevant
};

// States of a vertex in a currently evaluating minimax graph.
enum class vert_state {
    fresh, finished, expandable, expanding, fixed
};

std::string state_name(vert_state st) {
    switch (st) {
        case vert_state::fresh:
            return "fresh";
            break;
        case vert_state::finished:
            return "finished";
            break;
        case vert_state::expandable:
            return "expandable";
            break;
        case vert_state::expanding:
            return "expanding";
            break;
        default:
        case vert_state::fixed:
            return "fixed";
            break;
    }
}

enum class leaf_type {
    nonleaf, heuristical, trueleaf, boundary, assumption
};

std::string leaf_type_name(leaf_type lt) {
    switch (lt) {
        case leaf_type::heuristical:
            return "heuristical";
            break;
        case leaf_type::trueleaf:
            return "trueleaf";
            break;
        case leaf_type::boundary:
            return "boundary";
            break;
        case leaf_type::assumption:
            return "assumption";
            break;
        default:
        case leaf_type::nonleaf:
            return "nonleaf";
            break;
    }
}

// Shared memory between overseer and worker.
// In the MPI model, these two processes can share all global variables,
// but in the local model, these variables can also be shared by other
// processes in the system, including the queen.

// To make the API clean, we pass them on from the overseer to each worker
// at the spawn time.
struct worker_flags {
    bool root_solved = false;
};



