#pragma once

// A virtual class that sets the methods required for an adversarial strategy.
// Essentially an interface.
template<minimax MODE> adversarial_strategy
{

public:
std::pair<victory, adversarial_strategy < MODE> *>

heuristics(const binconf *b, computation <MODE> *comp) = 0;

virtual void calcs(const binconf *b, computation <MODE> *comp) = 0;

virtual void undo_calcs() = 0;

virtual std::vector<int> moveset(const binconf *b) = 0;

virtual void adv_move(const binconf *b, int item) = 0;

virtual void undo_adv_move() = 0;

// The following properties are needed only if strategies are heuristical (and thus
// can be loaded or printed).
heuristic type = heuristic::none;

virtual std::string print(const binconf *b) = 0;

virtual std::vector<int> contents() = 0;

};
