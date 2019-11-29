#ifndef STRATEGIES_INSIGHT_HPP
#define STRATEGIES_INSIGHT_HPP

class thread_attr; // forward declaration.

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
    int itemdepth = 0;

    // The basic strategy also stores these numbers for the computations.
    revertible<int> lowest_sendable;
    revertible<int> maximum_feasible;
    revertible<int> lower_bound;
    heuristic_strategy* strat = nullptr;

    victory heuristics(const binconf *b, thread_attr *tat);

    bool heuristic_regime() const
	{
	    return (strat != nullptr);
	}

    // undo_heuristics() should be called only when heuristics() returned true.
    void undo_heuristics()
	{
	    strat = nullptr;
	}

    void computation(const binconf *b, thread_attr *tat);
    void undo_computation();
    std::vector<int> moveset(const binconf *b);
    void adv_move(const binconf *b, int item);
    void undo_adv_move();
    // void alg_move(const binconf *b, int item, int bin);
    // void undo_alg_move();

};

#endif // STRATEGIES_INSIGHT_HPP
