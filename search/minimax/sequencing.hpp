#pragma once

// Sequencing routines -- generating a fixed tree as the start of the search.

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <map>

#include "common.hpp"
#include "binconf.hpp"
#include "optconf.hpp"
#include "advisor.hpp"
#include "thread_attr.hpp"
#include "minimax/computation.hpp"
#include "dag/dag.hpp"
#include "hash.hpp"
#include "fits.hpp"
#include "gs.hpp"
#include "tasks/tasks.hpp"
#include "heur_adv.hpp"

// Currently sequencing is templated but "not really" -- it is only for generation.
// This should be fixed soon.

template<minimax MODE, int MINIBS_SCALE>
victory sequencing_adversary(binconf *b, unsigned int depth, computation<MODE, MINIBS_SCALE> *comp,
                             adversary_vertex *adv_to_evaluate, algorithm_vertex *parent_alg,
                             const advisor &advis);

template<minimax MODE, int MINIBS_SCALE>
victory sequencing_algorithm(binconf *b, int k, unsigned int depth, computation<MODE, MINIBS_SCALE> *comp,
                             algorithm_vertex *alg_to_evaluate, adversary_vertex *parent_adv,
                             const advisor &advis);


// Generates a tree with saplings (not tasks) from a sequence of items
victory sequencing(binconf &root, adversary_vertex *root_vertex, guar_cache* dpcache) {

    computation<minimax::generating, 0> comp(dpcache, nullptr);

    onlineloads_init(comp.ol, &root);
    advisor simple_advis;

    bool advice_file_found = false;
    if (USING_ADVISOR && fs::exists(ADVICE_FILENAME)) {
        simple_advis.load_advice_file(ADVICE_FILENAME);
        advice_file_found = true;
        print_if<PROGRESS>("Loaded advice file %s.\n", ADVICE_FILENAME);
    }

    if (USING_ASSUMPTIONS && fs::exists(ASSUMPTIONS_FILENAME)) {
        comp.assumer.load_file(ASSUMPTIONS_FILENAME);
    }

    if (!USING_ADVISOR || !advice_file_found) {
        print_if<PROGRESS>("Not using the advisor.\n");
        root_vertex->leaf = leaf_type::boundary;
        root_vertex->sapling = true;
        root_vertex->win = victory::uncertain;
        return victory::uncertain;
    } else {
        print_if<PROGRESS>("Sequencing with advisor starts.\n");
        victory ret = sequencing_adversary<minimax::generating, 0>(&root, 0, &comp, root_vertex, NULL, simple_advis);
        print_if<PROGRESS>("Sequencing with advisor ends.\n");
        return ret;
    }
}

template<minimax MODE, int MINIBS_SCALE>
victory sequencing_adversary(binconf *b, unsigned int depth, computation<MODE, MINIBS_SCALE> *comp,
                             adversary_vertex *adv_to_evaluate, algorithm_vertex *parent_alg,
                             const advisor &advis) {
    algorithm_vertex *upcoming_alg = NULL;
    adv_outedge *new_edge = NULL;
    bool switch_to_heuristic = false;
    int suggestion = 0;

    /* Everything can be packed into one bin, return 1. */
    if ((b->loads[BINS] + (BINS * S - b->totalload())) < R) {
        adv_to_evaluate->win = victory::alg;
        // The vertex should be deleted anyway, but we mark it as a heuristical leaf.
        adv_to_evaluate->leaf = leaf_type::heuristical;
        return victory::alg;
    }

    // Check the assumptions cache and immediately mark this vertex as solved if present.
    // (Mild TODO: we should probably mark it in the tree as solved in some way, to avoid issues from checking the bound.

    if (USING_ASSUMPTIONS) {
        victory check = comp->assumer.check(*b);
        if (check != victory::uncertain) {
            adv_to_evaluate->win = check;
            adv_to_evaluate->leaf = leaf_type::assumption;
            return check;
        }
    }

    if (ADVERSARY_HEURISTICS) {
        // The procedure may generate the vertex in question.
        // Note: we pass S as the maximum_feasible_next, which effectively turns it off.
        auto [vic, strategy] = adversary_heuristics<minimax::generating>(comp->dpcache, b, comp->dpdata, &(comp->meas),
                                                                         adv_to_evaluate, S);

        if (vic == victory::adv) {
            print_if<DEBUG>("Sequencing: Adversary heuristic ");
            if (DEBUG) {
                print_heuristic(stderr, adv_to_evaluate->heur_strategy->type);
            }
            print_if<DEBUG>(" is successful.\n");

            comp->heuristic_regime = true;
            comp->heuristic_starting_depth = depth;
            comp->current_strategy = strategy;
            switch_to_heuristic = true;
        }

    }

    if (comp->heuristic_regime) {
        adv_to_evaluate->mark_as_heuristical(comp->current_strategy);
        // We can already mark the vertex as "won", but the question is
        // whether not to do it later.
        adv_to_evaluate->win = victory::adv;
        adv_to_evaluate->leaf = leaf_type::heuristical;
    }


    if (!comp->heuristic_regime && USING_ADVISOR) {
        suggestion = advis.suggest_advice(b);
        // fprintf(stderr, "Suggestion %d for binconf ", (int) suggestion); // Debug.
        // print_binconf_stream(stderr, b);

    }

    if (!comp->heuristic_regime && suggestion == 0) {
        adv_to_evaluate->leaf = leaf_type::boundary;
        adv_to_evaluate->win = victory::uncertain;
        adv_to_evaluate->sapling = true;
        // fprintf(stderr, "Marking as sapling: ");
        // adv_to_evaluate->print(stderr, true);
        return victory::uncertain;
    }

    victory below = victory::alg;
    victory r = victory::alg;
    int item_size;
    // send items based on the array and depth
    if (comp->heuristic_regime) {
        item_size = comp->current_strategy->next_item(b);
    } else {
        assert(suggestion != 0);
        item_size = suggestion;
    }
    // print_if<DEBUG>("Trying player zero choices, with maxload starting at %d\n", comp->maximum_feasible);
    print_if<DEBUG>("Sending item %d to algorithm.\n", item_size);
    // algorithm's vertex for the next step
    // Check vertex cache if this adversarial vertex is already present.
    // std::map<llu, adversary_vertex*>::iterator it;
    bool already_generated = false;
    auto it = qdag->alg_by_hash.find(b->alghash(item_size));
    if (it == qdag->alg_by_hash.end()) {
        upcoming_alg = qdag->add_alg_vertex(*b, item_size);
        new_edge = qdag->add_adv_outedge(adv_to_evaluate, upcoming_alg, item_size);
    } else {
        already_generated = true;
        upcoming_alg = it->second;
        // create new edge
        new_edge = qdag->add_adv_outedge(adv_to_evaluate, upcoming_alg, item_size);
        below = it->second->win;
    }


    if (!already_generated) {
        // descend
        if (comp->current_strategy != nullptr) {
            comp->current_strategy->increase_depth();
        }

        below = sequencing_algorithm(b, item_size, depth + 1, comp, upcoming_alg, adv_to_evaluate, advis);

        // ascend
        if (comp->current_strategy != nullptr) {
            comp->current_strategy->decrease_depth();
        }

    }

    // If we were in heuristics mode, switch back to normal.
    if (switch_to_heuristic) {
        comp->heuristic_regime = false;
        comp->current_strategy = nullptr;
    }


    if (below == victory::adv) {
        r = victory::adv;
        qdag->remove_outedges_except<minimax::generating>(adv_to_evaluate, item_size);
    } else if (below == victory::alg) {
        qdag->remove_edge<minimax::generating>(new_edge);
    } else if (below == victory::uncertain) {
        if (r == victory::alg) {
            r = victory::uncertain;
        }
    }

    if (r == victory::alg) {
        assert(adv_to_evaluate->out.empty());
    }

    adv_to_evaluate->win = r;
    return r;
}

template<minimax MODE, int MINIBS_SCALE>
victory sequencing_algorithm(binconf *b, int k, unsigned int depth, computation<MODE, MINIBS_SCALE> *comp,
                             algorithm_vertex *alg_to_evaluate, adversary_vertex *parent_adv,
                             const advisor &advis) {

    adversary_vertex *upcoming_adv = NULL;
    if (gsheuristic(b, k, &(comp->meas)) == 1) {
        alg_to_evaluate->win = victory::alg;
        alg_to_evaluate->leaf = leaf_type::heuristical;
        return victory::alg;
    }


    if (comp->heuristic_regime) {
        alg_to_evaluate->leaf = leaf_type::heuristical;
    }
    victory r = victory::adv;
    victory below = victory::adv;
    int8_t i = 1;

    while (i <= BINS) {
        // simply skip a step where two bins have the same load
        // any such bins are sequential if we assume loads are sorted (and they should be)
        if (i > 1 && b->loads[i] == b->loads[i - 1]) {
            i++;
            continue;
        }

        if ((b->loads[i] + k < R)) {

            // editing binconf in place -- undoing changes later

            int previously_last_item = b->last_item;
            int from = b->assign_and_rehash(k, i);
            int ol_from = onlineloads_assign(comp->ol, k);
            // initialize the adversary's next vertex in the tree (corresponding to d)

            /* Check vertex cache if this adversarial vertex is already present */
            bool already_generated = false;
            auto it = qdag->adv_by_hash.find(b->hash_with_last());
            if (it == qdag->adv_by_hash.end()) {
                upcoming_adv = qdag->add_adv_vertex(*b);
                qdag->add_alg_outedge(alg_to_evaluate, upcoming_adv, i);
            } else {
                already_generated = true;
                upcoming_adv = it->second;
                qdag->add_alg_outedge(alg_to_evaluate, upcoming_adv, i);
                below = it->second->win;
            }

            if (!already_generated) {
                below = sequencing_adversary<MODE>(b, depth, comp, upcoming_adv, alg_to_evaluate, advis);
                print_if<DEBUG>("SEQ: Alg packs into bin %d, the new configuration is:", i);
                print_binconf_if<DEBUG>(b);
                print_if<DEBUG>("Resulting in: ");
                if (DEBUG) {
                    print(stderr, below);
                }
                print_if<DEBUG>(".\n");
            }

            // return b to original form
            b->unassign_and_rehash(k, from, previously_last_item);

            onlineloads_unassign(comp->ol, k, ol_from);

            if (below == victory::alg) {
                r = below;
                qdag->remove_outedges<minimax::generating>(alg_to_evaluate);
                alg_to_evaluate->win = r;
                return r;
            } else if (below == victory::adv) {
                // nothing needs to be currently done, the edge is already created
            } else if (below == victory::uncertain) {
                // insert upcoming_adv into algorithm's "next" list
                if (r == victory::adv) {
                    r = victory::uncertain;
                }
            }
        } else { // b->loads[i] + k >= R, so a good situation for the adversary
        }
        i++;
    }

    alg_to_evaluate->win = r;


    // If the vertex has degree zero and no descendants, it is a true leaf -- no move for algorithm is allowed.
    if (alg_to_evaluate->win == victory::adv && alg_to_evaluate->out.size() == 0) {
        alg_to_evaluate->leaf = leaf_type::trueleaf;
    }

    return r;
}