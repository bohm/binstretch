#ifndef STRATEGIES_INSIGHT_HPP
#define STRATEGIES_INSIGHT_HPP

// A very simple stack that should be faster than std::stack.
template <class T> class revertible
{
private:
    std::vector<T> stack;
    int pos = -1;
    int capacity = 50;
    void double_capacity();
public:
    revertible();
    T& value();
    void push(T new_item);
    void pop();
};

template <class T> revertible<T>::revertible()
{
    stack.reserve(50);
}

template <class T> void revertible<T>::double_capacity()
{
    capacity *= 2;
    stack.reserve(capacity);
}

template <class T> T& revertible<T>::value()
{
    assert(pos >= 0);
    return stack[pos];
}

template <class T> void revertible<T>::push(T new_item)
{
    if (pos+1 >= capacity)
    {
	double_capacity();
    }
    stack[++pos] = new_item;
}

template <class T> void revertible<T>::pop()
{
    pos--;
}

// A boolean switch that is simpler than a stack of bools.
class depth_switch
{
private:
    bool value = false;
    int depth_when_flipped = -1;
public:
    void on(int depth)
	{
	    value = true;
	    depth_when_flipped = depth;
	}

    // undo() returns true if a reset happened.
    bool undo(int depth)
	{
	    if (value && depth_when_flipped == depth)
	    {
		value = false;
		depth_when_flipped = -1;
		return true;
	    }
	    return false;
	}

    void reset()
	{
	    value = false;
	    depth_when_flipped = -1;
	}
};



class algorithmic_strategy_basic
{
public:
    bool following_algorithm = false;
    void alg_move(const binconf *b, int item, int bin);
    void adv_move(const binconf *b, int item);

    void undo_alg_move();
    void undo_adv_move();
};

// Any "insight" that the strategy needs to proceed.
// Updated iteratively after every move of either player.
class adversary_strategy_basic
{
public:
    // Almost all strategies monitor depth.
    revertible<int> depth;

    // The basic strategy also stores these numbers for the computations.
    revertible<int> lowest_sendable;
    revertible<int> maximum_feasible;
    revertible<int> lower_bound;
 
    heuristic_strategy* strat = nullptr;

    // Heuristics are applied before cache checking and before
    // any computation. Returns "true" if a heuristic strategy is seleted.
    bool heuristics(const binconf *b);

    // undo_heuristics() should be called only when heuristics() returned true.
    void undo_heuristics()
	{
	    strat = nullptr;
	}

    // In this phase, the strategy is free to compute parameters that are
    // required for the selection of items -- based on the previous values
    // as well as new computation.
    
    void computation(const binconf *b, thread_attr *tat)
	{
	    int computed_maxfeas = MAXIMUM_FEASIBLE(b, depth.value(), lower_bound.value(), maximum_feasible.value(), tat);
	    maximum_feasible.push(computed_maxfeas);
	}

    // undo_computation() should be called every time before leaving the adversary() function.
    void undo_computation()
	{
	    maximum_feasible.pop();
	}
    
    // This computes the moveset of the adversary strategy based on the
    // computation before. This function should not update the internal
    // state.
    
    moveset(const binconf *b)
	{
	}


    // Update the internal state after a move is done by the adversary.
    
    void adv_move(const binconf *b, int item)
	{
	    lower_bound.push(lowest_sendable(item));
	}
    void undo_adv_move()
	{
	    lower_bound.pop();
	}
    
    // And possibly also when there is a move by the algorithm.
    void alg_move(const binconf *b, int item, int bin);
    void undo_alg_move();

};


#endif // STRATEGIES_INSIGHT_HPP