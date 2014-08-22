/*
 * Copyright (C) 2010 Jiri Simacek
 *
 * This file is part of predator.
 *
 * predator is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * predator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with predator.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TREE_AUT_H
#define TREE_AUT_H

// Standard library headers
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <cassert>
#include <stdexcept>

// Forester headers
#include "cache.hh"
#include "lts.hh"
#include "utils.hh"
#include "label.hh"

class TA;

typedef uintptr_t T;

class TTBase
{
	friend class TA;

public:   // data types

	typedef Cache<std::vector<size_t>> lhs_cache_type;

protected:// data members

	lhs_cache_type::value_type* lhs_;

private:  // data members

	T label_;
	size_t rhs_;

private:  // methods

	TTBase(const TTBase&);
	TTBase& operator=(const TTBase&);

protected:// methods

	TTBase(
		lhs_cache_type::value_type*       lhs = nullptr,
		const T&                          label = T(),
		size_t                            rhs = 0) :
		lhs_(lhs),
		label_(label),
		rhs_(rhs)
	{ }

public:   // methods

	size_t GetNthChildren(int n) const
    {
        return lhs()[n]; 
    }
	const std::vector<size_t>& GetChildren() const
    {
        return lhs(); 
    }
	const std::vector<size_t>& lhs() const
	{
		assert(nullptr != lhs_);
		return lhs_->first;
	}
    size_t GetChildrenSize() const
    {
		assert(nullptr != lhs_);
		return lhs_->first.size();
    }

	const label_type label() const { return label_type(label_); }
	const label_type GetSymbol() const { return label_type(label_); }

	size_t rhs() const { return rhs_; }
	size_t GetParent() const { return rhs_; }

	bool operator==(const TTBase& rhs) const
	{
		return (label_ == rhs.label_) && (lhs_ == rhs.lhs_) && (rhs_ == rhs.rhs_);
	}

	bool operator<(const TTBase& rhs) const
	{
		// this is completely illegible
		return (rhs_ < rhs.rhs_) || (
			(rhs_ == rhs.rhs_) && ((label_ < rhs.label_) || (
				(label_ == rhs.label_) && ( lhs_ < rhs.lhs_))));
	}

	friend size_t hash_value(const TTBase& t)
	{
		size_t h = boost::hash_value(t.lhs_);
		boost::hash_combine(h, t.label_);
		boost::hash_combine(h, t.rhs_);
		return h;
	}

	friend std::ostream& operator<<(std::ostream& os, const TTBase& t)
	{
		os << t.label_ << '(';
		if (nullptr != t.lhs_)
		{
			if (t.lhs().size() > 0)
			{
				os << t.lhs()[0];
				for (size_t i = 1; i < t.lhs().size(); ++i)
					os << ',' << t.lhs()[i];
			}
		}
		return os << ")->" << t.rhs_;
	}

	/**
	 * @brief  Run a visitor on the instance
	 *
	 * This method is the @p accept method of the Visitor design pattern.
	 *
	 * @param[in]  visitor  The visitor of the type @p TVisitor
	 *
	 * @tparam  TVisitor  The type of the visitor
	 */
	template <class TVisitor>
	void accept(TVisitor& visitor) const
	{
		visitor(*this);
	}

	/**
	 * @brief  Virtual destructor
	 */
	virtual ~TTBase()
	{ }
};

class TT : public TTBase
{
	friend class TA;

private:  // data types

	typename TTBase::lhs_cache_type& lhsCache;

public:   // methods

	TT(
		const TT&           t) :
		TTBase(t.lhs_, t.label(), t.rhs()),
		lhsCache(t.lhsCache)
	{
		this->lhsCache.addRef(TTBase::lhs_);
	}

	TT(
		const std::vector<size_t>&              lhs,
		const T&                                label,
		size_t                                  rhs,
		typename TTBase::lhs_cache_type&     lhsCache) :
		TTBase(lhsCache.lookup(lhs), label, rhs),
		lhsCache(lhsCache)
	{ }

	TT(
		const std::vector<size_t>&              lhs,
		const T&                                label,
		size_t                                  rhs,
		const std::vector<size_t>&              index,
		typename TTBase::lhs_cache_type&     lhsCache) :
		TTBase(nullptr, label, index[rhs]),
		lhsCache(lhsCache)
	{
		std::vector<size_t> tmp(lhs.size());
		for (size_t i = 0; i < lhs.size(); ++i)
			tmp[i] = index[lhs[i]];
		TTBase::lhs_ = this->lhsCache.lookup(tmp);
	}

	TT(
		const TT&                                t,
		typename TTBase::lhs_cache_type&      lhsCache) :
		TTBase(lhsCache.lookup(t.lhs()), t.label(), t.rhs()),
		lhsCache(lhsCache)
	{ }

	TT(
		const TT&                                t,
		const std::vector<size_t>&               index,
		typename TTBase::lhs_cache_type&      lhsCache) :
		TTBase(nullptr, t.label(), index[t.rhs()]),
		lhsCache(lhsCache)
	{
		std::vector<size_t> tmp(t.lhs().size());
		for (size_t i = 0; i < t.lhs().size(); ++i)
			tmp[i] = index[t.lhs()[i]];
		TTBase::lhs_ = this->lhsCache.lookup(tmp);
	}

	~TT()
	{
		this->lhsCache.release(TTBase::lhs_);
	}

	bool llhsLessThan(
		const TT&                                 rhs,
		const std::vector<std::vector<bool>>&     cons,
		const Index<size_t>&                      stateIndex) const
	{
		if (this->label() != rhs.label())
			return false;
		for (size_t i = 0; i < this->lhs().size(); ++i)
		{
			if (!cons[stateIndex[this->lhs()[i]]][stateIndex[rhs.lhs()[i]]])
				return false;
		}
		return true;
	}
};


/**
 * @brief  Tree automaton
 */
class TA
{
	template<class TAC> friend class AntichainExt;
public:   // data types
	///	the type of a tree automaton transition
	typedef TT Transition;

private:
	/// the value type of the cache: a pair of a transition and its ID
	typedef std::pair<const Transition, size_t> TransIDPair;

	/// cache of transitions_
	typedef Cache<Transition> trans_cache_type;
    // check that the types _really_ match
	static_assert(
		std::is_same<typename trans_cache_type::value_type, TransIDPair>::value,
		"Incompatible types!");

	// this is the place where transitions_ are stored
	struct Backend
	{
		typename TTBase::lhs_cache_type lhsCache;
		trans_cache_type transCache;

		Backend() :
			lhsCache{},
			transCache{}
		{ }
	};

	struct CmpF
	{
		bool operator()(
			const TransIDPair*     lhs,
			const TransIDPair*     rhs) const
		{
			return lhs->first < rhs->first;
		}
	};

	typedef std::set<TransIDPair*, CmpF> trans_set_type;

	/**
	 * @brief  Iterator over transitions_
	 */
	class Iterator
	{
		typename trans_set_type::const_iterator _i;
	public:
		Iterator(typename trans_set_type::const_iterator i) : _i(i) {}

		Iterator& operator++() { return ++this->_i, *this; }

		Iterator operator++(int) { return Iterator(this->_i++); }

		Iterator& operator--() { return --this->_i, *this; }

		Iterator operator--(int) { return Iterator(this->_i--); }

		const Transition& operator*() const { return (*this->_i)->first; }

		const Transition* operator->() const { return &(*this->_i)->first; }

		bool operator==(const Iterator& rhs) const { return this->_i == rhs._i; }

		bool operator!=(const Iterator& rhs) const { return this->_i != rhs._i; }
	};

	typedef typename std::unordered_map<size_t,
            std::vector<const Transition*>> td_cache_type;

	class TDIterator
	{
	private:  // data members

		const td_cache_type& cache_;
		std::set<size_t> visited_;
		std::vector<
			std::pair<
				typename std::vector<const Transition*>::const_iterator,
				typename std::vector<const Transition*>::const_iterator
			>
		> stack_;

		void insertLhs(const std::vector<size_t>& lhs)
		{
			for (std::vector<size_t>::const_reverse_iterator i = lhs.rbegin(); i != lhs.rend(); ++i)
			{
				if (visited_.insert(*i).second)
				{
					typename td_cache_type::const_iterator j = cache_.find(*i);
					if (j != cache_.end())
						stack_.push_back(std::make_pair(j->second.begin(), j->second.end()));
				}
			}
		}

	public:

		TDIterator(
			const td_cache_type&         cache,
			const std::vector<size_t>&   stack) :
			cache_(cache),
			visited_(),
			stack_{ }
		{
			this->insertLhs(stack);
		}

		bool isValid() const { return !stack_.empty(); }

		bool next()
		{
			size_t oldSize = stack_.size();
			this->insertLhs((*stack_.back().first)->lhs());
			// if something changed then we have a new "working" item on the top of the stack
			if (stack_.size() != oldSize)
				return true;
			++stack_.back().first;
			do
			{
				// is there something to process ?
				if (stack_.back().first != stack_.back().second)
					return true;
				// discard processed queue
				stack_.pop_back();
			} while (!stack_.empty());
			// nothing else remains
			return false;
		}

		const Transition& operator*() const { return **stack_.back().first; }

		const Transition* operator->() const { return *stack_.back().first; }

	};

	typedef std::unordered_map<size_t, std::vector<const Transition*>> bu_cache_type;
	typedef std::unordered_map<T, std::vector<const Transition*>> lt_cache_type;

	typedef TDIterator td_iterator;

public:
	typedef Iterator iterator;

private: // private constants
    const int cEmptyRootTransIndex = 0;

private:  // data members

	size_t nextState_;
	std::unordered_set<size_t> finalStates_;
	Backend* backend_;
	size_t maxRank_;
	trans_set_type transitions_;

private: // private constructor
	TA(Backend&             backend_);
    /*
    template <class F>
	TA(const TA&         ta,
	    F                    f);
        */

public:
	TA();
    TA(const TA&         ta);

    static TA createTAWithSameTransitions(
		const TA&         ta);

    static TA* allocateTAWithSameTransitions(
		const TA&         ta);
    
    static TA createTAWithSameFinalStates(
		const TA&         ta,
        bool                 copyFinalStates=true);

    static TA* allocateTAWithSameFinalStates(
		const TA&         ta,
        bool                 copyFinalStates=true);
    
    ~TA()
	{
		this->clear();
	}

    std::vector<const Transition*> getEmptyRootTransitions() const;

    void copyReachableTransitionsFromRoot(
            const TA&     src,
            const size_t&    rootState);
    
	typename TA::Iterator begin() const
	{
		return typename TA::Iterator(this->transitions_.begin());
	}

	typename TA::Iterator end() const
	{
		return typename TA::Iterator(this->transitions_.end());
	}

	typename TA::Iterator begin(size_t rhs) const
	{
		return Iterator(this->_lookup(rhs));
	}

	typename TA::Iterator end(size_t rhs) const
	{
		typename TA::Iterator i = this->begin(rhs);
		for (; i != this->end() && i->rhs() == rhs; ++i);
		return Iterator(i);
	}

	typename TA::Iterator end(size_t rhs, typename TA::Iterator i) const
	{
		for (; i != this->end() && i->rhs() == rhs; ++i);
		return Iterator(i);
	}

	typename TA::Iterator accBegin() const
	{
		return this->begin(this->getFinalState());
	}

	typename TA::Iterator accEnd() const
	{
		typename TA::Iterator i = this->accBegin();
		return this->end(this->getFinalState(), i);
	}

	typename TA::Iterator accEnd(typename TA::Iterator i) const
	{
		return this->end(this->getFinalState(), i);
	}

	TA& operator=(const TA& rhs)
	{
		if (&rhs == this)
			return *this;

		this->clear();
		nextState_ = rhs.nextState_;
		this->maxRank_ = rhs.maxRank_;
		this->backend_ = rhs.backend_;
		this->transitions_ = rhs.transitions_;
		finalStates_ = rhs.finalStates_;

		for (TransIDPair* trans : this->transitions_)
		{	// copy transitions_
			this->transCache().addRef(trans);
		}

		return *this;
	}

	void clear()
	{
		this->maxRank_ = 0;
		nextState_ = 0;
		for (TransIDPair* trans : this->transitions_)
		{
			this->transCache().release(trans);
		}
		this->transitions_.clear();
		finalStates_.clear();
	}

	/**
	 * @brief  Build an index of states occuring in the automaton
	 *
	 * This method builds an index of states that really occur in the automaton.
	 *
	 * @param[out]  index  The index that will be built
	 */
	void buildStateIndex(Index<size_t>& index) const;
	
	void addTransition(const Transition& transition);
    void addTransition(
		const std::vector<size_t>&          lhs,
		const T&                            label,
		size_t                              rhs);

    const Transition& getTransition(
        const std::vector<size_t>&          lhs,
		const T&                            label,
		size_t                              rhs);
	
    static const label_type GetSymbol(const Transition& trans);

    bool areTransitionsEmpty()
    {
        return this->transitions_.empty();
    }

	void addFinalState(size_t state)
	{
		finalStates_.insert(state);
	}

    /*
	void addFinalStates(const std::vector<size_t>& states)
	{
		finalStates_.insert(states.begin(), states.end());
	}
    */

	void addFinalStates(const std::unordered_set<size_t>& states)
	{
		finalStates_.insert(states.begin(), states.end());
	}

	bool isFinalState(size_t state) const
	{
		return (finalStates_.find(state) != finalStates_.end());
	}

	const std::unordered_set<size_t>& getFinalStates() const
	{
		return finalStates_;
	}

	size_t getFinalState() const
	{
		assert(finalStates_.size() == 1);
		return *finalStates_.begin();
	}

    const Transition& getAcceptingTransition() const
	{
		// Assertions
		assert(this->accBegin() != this->accEnd());
		assert(++(this->accBegin()) == this->accEnd());
    
		return *(this->accBegin());
	}

/*
	TA::RhsIterator getAcceptingTransitions() const {
		return this->getRhsIterator(this->getFinalState());
	}
*/
	
    // currently erases '1' from the relation
	template <class F>
	void heightAbstraction(
		std::vector<std::vector<bool>>&            result,
		size_t                                     height,
		F                                          f,
		const Index<size_t>&                       stateIndex) const
	{
		td_cache_type cache = this->buildTDCache();

		std::vector<std::vector<bool>> tmp;

		while (height--)
		{
			tmp = result;

			for (Index<size_t>::iterator i = stateIndex.begin(); i != stateIndex.end(); ++i)
			{
				const size_t& state1 = i->second;
				typename td_cache_type::iterator j = cache.insert(
					std::make_pair(i->first, std::vector<const Transition*>())
				).first;
				for (Index<size_t>::iterator k = stateIndex.begin(); k != stateIndex.end(); ++k)
				{
					const size_t& state2 = k->second;
					if ((state1 == state2) || !tmp[state1][state2])
						continue;
					typename td_cache_type::iterator l = cache.insert(
						std::make_pair(k->first, std::vector<const Transition*>())
					).first;
					bool match = true;
					for (const Transition* trans1 : j->second)
					{
						for (const Transition* trans2 : l->second)
						{
							if (!TA::transMatch(trans1, trans2, f, tmp, stateIndex))
							{
								match = false;
								break;
							}
						}
						if (!match)
							break;
					}
					if (!match)
						result[state1][state2] = false;
				}
			}
		}

		for (size_t i = 0; i < result.size(); ++i)
		{
			for (size_t j = 0; j < i; ++j)
			{
				if (!result[i][j])
					result[j][i] = false;
				if (!result[j][i])
					result[i][j] = false;
			}
		}
	}

    /*
	void predicateAbstraction(
		std::vector<std::vector<bool>>&      result,
		const TA&                         predicate,
		const Index<size_t>&                 stateIndex) const
	{
		std::vector<size_t> states;
		this->intersectingStates(states, predicate);
		std::set<size_t> s;
		for (std::vector<size_t>::iterator i = states.begin(); i != states.end(); ++i)
			s.insert(stateIndex[*i]);
		for (size_t i = 0; i < result.size(); ++i)
		{
			if (s.count(i) == 1)
				continue;
			for (size_t j = 0; j < i; ++j)
			{
				result[i][j] = 0;
				result[j][i] = 0;
			}
			for (size_t j = i + 1; j < result.size(); ++j)
			{
				result[i][j] = 0;
				result[j][i] = 0;
			}
		}
	}
    */

	// collapses states according to a given relation
	TA& collapsed(
		TA&                                   dst,
		const std::vector<std::vector<bool>>&    rel,
		const Index<size_t>&                     stateIndex) const
	{
		std::vector<size_t> headIndex;
		utils::relBuildClasses(rel, headIndex);

		std::ostringstream os;
		utils::printCont(os, headIndex);

		// TODO: perhaps improve indexing
		std::vector<size_t> invStateIndex(stateIndex.size());
		for (Index<size_t>::iterator i = stateIndex.begin(); i != stateIndex.end(); ++i)
		{
			invStateIndex[i->second] = i->first;
		}

		for (std::vector<size_t>::iterator i = headIndex.begin(); i != headIndex.end(); ++i)
		{
			*i = invStateIndex[*i];
		}

		for (const size_t& state : finalStates_)
		{
			dst.addFinalState(headIndex[stateIndex[state]]);
		}

		for (const TransIDPair* trans : this->transitions_)
		{
			std::vector<size_t> lhs;
			stateIndex.translate(lhs, trans->first.lhs());
			for (size_t j = 0; j < lhs.size(); ++j)
				lhs[j] = headIndex[lhs[j]];
			dst.addTransition(lhs, trans->first.label(), headIndex[stateIndex[trans->first.rhs()]]);
			std::ostringstream os;
			utils::printCont(os, lhs);
		}
		return dst;
	}
	
	TA& unreachableFree(TA& dst) const
	{
		std::vector<const TransIDPair*> v1(
			transitions_.begin(), this->transitions_.end()), v2;
		std::unordered_set<size_t> states(finalStates_.begin(), finalStates_.end());
		for (size_t finState : finalStates_)
		{
			dst.addFinalState(finState);
		}

		bool changed = true;
		while (changed)
		{
			changed = false;
			for (const TransIDPair* trans : v1)
			{
				if (states.count(trans->first.rhs()))
				{
					dst.addTransition(trans);
					for (size_t state : trans->first.lhs())
					{
						if (states.insert(state).second)
						{
							changed = true;
						}
					}
				} else {
					v2.push_back(trans);
				}
			}
			v1.clear();
			std::swap(v1, v2);
		}
		return dst;
	}

	TA& uselessAndUnreachableFree(TA& dst) const
	{
		std::vector<const TransIDPair*> v1(this->transitions_.begin(), this->transitions_.end()), v2, v3;
		std::unordered_set<size_t> states;
		bool changed = true;
		while (changed)
		{
			changed = false;
			for (typename std::vector<const TransIDPair*>::const_iterator i = v1.begin(); i != v1.end(); ++i)
			{
				bool matches = true;
				for (std::vector<size_t>::const_iterator j = (*i)->first.lhs().begin(); j != (*i)->first.lhs().end(); ++j)
				{
					if (!states.count(*j))
					{
						matches = false;
						break;
					}
				}
				if (matches)
				{
					if (states.insert((*i)->first.rhs()).second)
						changed = true;
					v3.push_back(*i);
				} else
				{
					v2.push_back(*i);
				}
			}
			v1.clear();
			std::swap(v1, v2);
		}
		for (size_t state : finalStates_)
		{
			if (states.count(state))
				dst.addFinalState(state);
		}
		std::swap(v1, v3);
		v2.clear();
		states = std::unordered_set<size_t>(dst.finalStates_.begin(), dst.finalStates_.end());
		changed = true;
		while (changed)
		{
			changed = false;
			for (typename std::vector<const TransIDPair*>::const_iterator i = v1.begin(); i != v1.end(); ++i)
			{
				if (states.count((*i)->first.rhs()))
				{
					dst.addTransition(*i);
					for (std::vector<size_t>::const_iterator j = (*i)->first.lhs().begin(); j != (*i)->first.lhs().end(); ++j)
					{
						if (states.insert(*j).second)
							changed = true;
					}
				} else
				{
					v2.push_back(*i);
				}
			}
			v1.clear();
			std::swap(v1, v2);
		}
		return dst;
	}

	TA& minimizedCombo(TA& dst) const
	{
		Index<size_t> stateIndex;
		this->buildSortedStateIndex(stateIndex);
		typename TA::Backend backend_;
		std::vector<std::vector<bool> > dwn;
		this->downwardSimulation(dwn, stateIndex);
		std::vector<std::vector<bool> > up;
		this->upwardSimulation(up, stateIndex, dwn);
		std::vector<std::vector<bool> > rel;
		TA::combinedSimulation(rel, dwn, up);
		TA tmp(backend_);
		return this->collapsed(tmp, rel, stateIndex).minimized(dst);
	}

	TA& minimized(TA& dst) const
	{
		Index<size_t> stateIndex;
		this->buildSortedStateIndex(stateIndex);
		std::vector<std::vector<bool> > cons(stateIndex.size(), std::vector<bool>(stateIndex.size(), true));
		return this->minimized(dst, cons, stateIndex);
	}

	static bool subseteq(const TA& a, const TA& b);

	/**
	 * @brief  Creates a new TA with renamed states
	 *
	 * This method takes the TA @p src and copies its states and transitions_
	 * (while renaming them on the way) into the TA @p dst, which may or may not
	 * be empty. The renaming is given by the @p funcRename functor. In case @p
	 * addFinalStates is @p true, the final states of @p src will also be set as
	 * final in @p dst.
	 *
	 * @param[out]     dst             The output TA
	 * @param[in]      src             The input TA
	 * @param[in,out]  funcRename      The functor that performs the renaming
	 * @param[in]      addFinalStates  Should the copied states which are final
	 *                                 in @p src be final also in @p dst?
	 *
	 * @returns  The output TA (same as @p dst)
	 */
	template <class F>
	static TA& rename(
		TA&                   dst,
		const TA&             src,
		F                        funcRename,
		bool                     addFinalStates = true)
	{
		return rename(
			dst,
			src,
			funcRename,
			/* predicate over transitions_ to be copied */
			[](const Transition&){ return true; },
			addFinalStates);
	}

	/**
	 * @brief  Copy transitions_ into a destination tree automaton
	 *
	 * Copies all transitions_ into the @p dst tree automaton.
	 *
	 * @param[in,out]  dst  Destination tree automaton
	 *
	 * @returns  Result tree automaton
	 */
	TA& copyTransitions(TA& dst) const
	{
		for (const TransIDPair* trans : this->transitions_)
			dst.addTransition(trans);
		return dst;
	}

	TA& copyNotAcceptingTransitions(TA& dst, const TA& ta) const
	{
        NonAcceptingF f(ta);

		for (const TransIDPair* trans : this->transitions_)
		{
			if (f(&trans->first))
				dst.addTransition(trans);
		}
		return dst;
	}

	static TA& disjointUnion(
		TA&                      dst,
		const TA&                src,
		bool                     addFinalStates = true)
	{
		if (addFinalStates)
		{
			for (size_t state : src.finalStates_)
				dst.addFinalState(state);
		}

		for (const TransIDPair* trans : src.transitions_)
			dst.addTransition(trans);

		return dst;
	}

	TA& unfoldAtRoot(
		TA&                   dst,
		size_t                   newState,
		bool                     registerFinalState = true) const
	{
		if (registerFinalState)
			dst.addFinalState(newState);

		for (const TransIDPair* trans : this->transitions_)
		{
			dst.addTransition(trans);
			if (this->isFinalState(trans->first.rhs()))
				dst.addTransition(trans->first.lhs(), trans->first.label(), newState);
		}

		return dst;
	}

	TA& unfoldAtRoot(
		TA&                                        dst,
		const std::unordered_map<size_t, size_t>&     states,
		bool                                          registerFinalState = true) const
	{
		this->copyTransitions(dst);
		for (size_t state : finalStates_)
		{
			std::unordered_map<size_t, size_t>::const_iterator j = states.find(state);
			assert(j != states.end());
			for (typename trans_set_type::const_iterator k = this->_lookup(state); k != this->transitions_.end() && (*k)->first.rhs() == state; ++k)
				dst.addTransition((*k)->first.lhs(), (*k)->first.label(), j->second);
			if (registerFinalState)
				dst.addFinalState(j->second);
		}
		return dst;
	}

	/**
	 * @brief  Run a visitor on the instance
	 *
	 * This method is the @p accept method of the Visitor design pattern.
	 *
	 * @param[in]  visitor  The visitor of the type @p TVisitor
	 *
	 * @tparam  TVisitor  The type of the visitor
	 */
	template <class TVisitor>
	void accept(TVisitor& visitor) const
	{
		visitor(*this);
	}

/*
	TA& unfoldAtLeaf(TA& dst, size_t selector) const {
		// TODO:
		return dst;
	}
*/

private:
    void copyReachableTransitionsFromRoot(
            const TA&     src,
            td_cache_type    cache,
            const size_t&    rootState);

    const TransIDPair* addTransition(
		const std::vector<size_t>&          lhs,
		const T&                            label,
		size_t                              rhs,
		const std::vector<size_t>&          index);

	const TransIDPair* addTransition(
		const TransIDPair*       transition);

    const TransIDPair* addTransition(
		const TransIDPair*               transition,
		const std::vector<size_t>&       index);

	const TransIDPair* addTransition(
		const Transition&                 transition,
		const std::vector<size_t>&        index);

    const trans_set_type& getTransitions() const
	{
		return this->transitions_;
	}

	typename trans_set_type::const_iterator _lookup(size_t rhs) const
	{
		char buffer[sizeof(std::pair<const Transition, size_t>)];
		std::pair<const Transition, size_t>* tPtr = 
            reinterpret_cast<std::pair<const Transition, size_t>*>(buffer);
		new (reinterpret_cast<TTBase*>(const_cast<Transition*>(&tPtr->first)))
            TTBase(nullptr, T(), rhs);
		typename trans_set_type::const_iterator i = this->transitions_.lower_bound(tPtr);
		(reinterpret_cast<TTBase*>(const_cast<Transition*>(&tPtr->first)))->~TTBase();
		return i;
	}

    typename TA::TDIterator tdStart(const td_cache_type& cache) const
	{
		return typename TA::TDIterator(cache, std::vector<size_t>(
                    finalStates_.begin(), finalStates_.end()));
	}

	typename TA::TDIterator tdStart(
		const td_cache_type&                 cache,
		const std::vector<size_t>&           stack) const
	{
		return typename TA::TDIterator(cache, stack);
	}

    typename Transition::lhs_cache_type& lhsCache() const
	{
		return this->backend_->lhsCache;
	}

	trans_cache_type& transCache() const
	{
		return this->backend_->transCache;
	}

    void buildSortedStateIndex(Index<size_t>& index) const;
	void buildLabelIndex(Index<T>& index) const;
	void buildLhsIndex(Index<const std::vector<size_t>*>& index) const;

	/**
	 * @brief  Creates a top-down cache for transitions_ of the TA
	 *
	 * This method creates a top-down cache of transitions_ of the TA, i.e.
	 * a mapping where for each state @e q there is a list of transitions_ such
	 * that @e q is the parent state of the transition.
	 *
	 * @returns  Top-down cache of transitions_ of the TA
	 */
	td_cache_type buildTDCache() const;
	td_cache_type buildTDCacheWithEmptyRoot() const;
	void buildBUCache(bu_cache_type& cache) const;
	void buildLTCache(lt_cache_type& cache) const;
    static void buildLTCacheExt(
            const TA&                 ta,
	        TA::lt_cache_type&        cache,
            T                            lUndef);

    void downwardTranslation(
		LTS&                                      lts,
		const Index<size_t>&                      stateIndex,
		const Index<T>&                           labelIndex) const;

	void downwardSimulation(
		std::vector<std::vector<bool>>&           rel,
		const Index<size_t>&                      stateIndex) const;

    void upwardTranslation(
		LTS&                                      lts,
		std::vector<std::vector<size_t>>&         part,
		std::vector<std::vector<bool>>&           rel,
		const Index<size_t>&                      stateIndex,
		const Index<T>&                           labelIndex,
		const std::vector<std::vector<bool>>&     sim) const;

	void upwardSimulation(
		std::vector<std::vector<bool>>&           rel,
		const Index<size_t>&                      stateIndex,
		const std::vector<std::vector<bool>>&     param) const;

	static void combinedSimulation(
		std::vector<std::vector<bool>>&           dst,
		const std::vector<std::vector<bool>>&     dwn,
		const std::vector<std::vector<bool>>&     up);

    struct IntersectF
	{
		TA& dst;
		const TA& src1;
		const TA& src2;

		IntersectF(
			TA&                dst,
			const TA&          src1,
			const TA&          src2) :
			dst(dst),
			src1(src1),
			src2(src2)
		{ }

		void operator()(
			const Transition*               t1,
			const Transition*               t2,
			const std::vector<size_t>&      lhs,
			size_t                          rhs)
		{
			if (this->src1.isFinalState(t1->rhs()) && this->src2.isFinalState(t2->rhs()))
				this->dst.addFinalState(rhs);
			this->dst.addTransition(lhs, t1->label(), rhs);
		}
	};


	static size_t intersection(
		TA&                           dst,
		const TA&                     src1,
		const TA&                     src2,
		size_t                           stateOffset = 0)
	{
		lt_cache_type cache1, cache2;
		src1.buildLTCache(cache1);
		src2.buildLTCache(cache2);
		return TA::buProduct(cache1, cache2, TA::IntersectF(dst, src1, src2), stateOffset);
	}

	TA& downwardSieve(
		TA&                                    dst,
		const std::vector<std::vector<bool>>&     cons,
		const Index<size_t>&                      stateIndex) const
	{
		td_cache_type cache = this->buildTDCache();

		for (size_t state : finalStates_)
			dst.addFinalState(state);

		for (typename td_cache_type::iterator i = cache.begin(); i != cache.end(); ++i)
		{
			std::list<const Transition*> tmp;
			for (typename std::vector<const Transition*>::iterator j = i->second.begin(); j != i->second.end(); ++j)
			{
				bool noskip = true;
				for (typename std::list<const Transition*>::iterator k = tmp.begin(); k != tmp.end(); )
				{
					if ((*j)->llhsLessThan(**k, cons, stateIndex))
					{
						noskip = false;
						break;
					}
					if ((*k)->llhsLessThan(**j, cons, stateIndex))
					{
						typename std::list<const Transition*>::iterator l = k++;
						tmp.erase(l);
					} else ++k;
				}
				if (noskip)
					tmp.push_back(*j);
			}
			for (typename std::list<const Transition*>::iterator j = tmp.begin(); j != tmp.end(); ++j)
				dst.addTransition(**j);
		}
		return dst;
	}

	TA& minimized(
		TA&                                   dst,
		const std::vector<std::vector<bool>>&    cons,
		const Index<size_t>&                     stateIndex) const
	{
		typename TA::Backend backend_;
		std::vector<std::vector<bool>> dwn;
		this->downwardSimulation(dwn, stateIndex);
		utils::relAnd(dwn, cons, dwn);
		TA tmp1(backend_), tmp2(backend_), tmp3(backend_);
		return this->collapsed(tmp1, dwn, stateIndex).uselessFree(tmp2).downwardSieve(tmp3, dwn, stateIndex).unreachableFree(dst);
	}


	/**
	 * @brief  Creates a new TA with renamed states
	 *
	 * This method takes the TA @p src and copies its states and transitions_
	 * (while renaming them on the way) into the TA @p dst, which may or may not
	 * be empty. The renaming is given by the @p funcRename functor and the
	 * transitions_ to be copied are given by the @p funcCopyTrans functor. In
	 * case * @p addFinalStates is @p true, the final states of @p src will also
	 * be set as final in @p dst.
	 *
	 * @param[out]     dst             The output TA
	 * @param[in]      src             The input TA
	 * @param[in,out]  funcRename      The functor that performs the renaming
	 * @param[in,out]  funcCopyTrans   The functor serving as the predicate
	 *                                 determining which transitions_ are to be
	 *                                 copied
	 * @param[in]      addFinalStates  Should the copied states which are final
	 *                                 in @p src be final also in @p dst?
	 *
	 * @returns  The output TA (same as @p dst)
	 */
	template <class F, class G>
	static TA& rename(
		TA&                   dst,
		const TA&             src,
		F                        funcRename,
		G                        funcCopyTrans,
		bool                     addFinalStates = true)
	{
		std::vector<size_t> lhs;
		if (addFinalStates)
		{
			for (size_t state : src.finalStates_)
				dst.addFinalState(funcRename(state));
		}

		for (const TransIDPair* transID : src.transitions_)
		{
			assert(nullptr != transID);
			const Transition& trans = transID->first;

			if (funcCopyTrans(trans))
			{	// in case the transition is to be copied
				lhs.resize(trans.lhs().size());
				for (size_t j = 0; j < trans.lhs().size(); ++j)
				{
					lhs[j] = funcRename(trans.lhs()[j]);
				}

				dst.addTransition(lhs, trans.label(), funcRename(trans.rhs()));
			}
		}

		return dst;
	}

    static TA& reduce(
		TA&                       dst,
		const TA&                 src,
		Index<size_t>&               index,
		size_t                       offset = 0,
		bool                         addFinalStates = true)
	{
		std::vector<size_t> lhs;
		if (addFinalStates)
		{
			for (size_t state : src.finalStates_)
			{
				dst.addFinalState(index.translateOTF(state) + offset);
			}
		}

		for (const TransIDPair* trans : src.transitions_)
		{
			lhs.clear();
			index.translateOTF(lhs, trans->first.lhs(), offset);
			dst.addTransition(lhs, trans->first.label(), index.translateOTF(trans->first.rhs()) + offset);
		}
		return dst;
	}

	struct NonAcceptingF
	{
		const TA& ta;
		NonAcceptingF(const TA& ta) : ta(ta) {}
		bool operator()(const Transition* t) { return !ta.isFinalState(t->rhs()); }
	};

	static TA& disjointUnion(
		TA&                      dst,
		const TA&                a,
		const TA&                b)
	{
		for (size_t state : a.finalStates_)
			dst.addFinalState(state);

		for (size_t state : b.finalStates_)
			dst.addFinalState(state);

		for (const TransIDPair* trans : a.transitions_)
			dst.addTransition(trans);

		for (const TransIDPair* trans : b.transitions_)
			dst.addTransition(trans);

		return dst;
	}

    template <class F>
	static size_t buProduct(
		const TA&                              ta1,
		const TA&                              ta2,
        T                                         lUndef,
		F                                         f,
		size_t                                    stateOffset = 0)
	{
		TA::lt_cache_type cache1, cache2;
        TA::buildLTCacheExt(ta1, cache1, lUndef);
		TA::buildLTCacheExt(ta2, cache2, lUndef);

        return TA::buProduct(ta1, ta2, f, stateOffset);
    }

    template <class F>
	static size_t buProduct(
		const lt_cache_type&                      cache1,
		const lt_cache_type&                      cache2,
		F                                         f,
		size_t                                    stateOffset = 0)
	{
		std::unordered_map<std::pair<size_t, size_t>, size_t, boost::hash<std::pair<size_t, size_t>>> product;
		for (typename lt_cache_type::const_iterator i = cache1.begin(); i != cache1.end(); ++i)
		{
			if (!i->second.front()->lhs().empty())
				continue;

			typename lt_cache_type::const_iterator j = cache2.find(i->first);
			if (j == cache2.end())
				continue;

			for (typename std::vector<const Transition*>::const_iterator k = i->second.begin(); k != i->second.end(); ++k)
			{
				for (typename std::vector<const Transition*>::const_iterator l = j->second.begin(); l != j->second.end(); ++l)
				{
					std::pair<std::unordered_map<std::pair<size_t, size_t>, size_t, boost::hash<std::pair<size_t, size_t>>>::iterator, bool> p =
						product.insert(std::make_pair(std::make_pair((*k)->rhs(), (*l)->rhs()), product.size() + stateOffset));
					f(*k, *l, std::vector<size_t>(), p.first->second);
				}
			}
		}
		bool changed = true;
		while (changed)
		{
			changed = false;
			for (typename lt_cache_type::const_iterator i = cache1.begin(); i != cache1.end(); ++i)
			{
				if (i->second.front()->lhs().empty())
					continue;
				typename lt_cache_type::const_iterator j = cache2.find(i->first);
				if (j == cache2.end())
					continue;
				for (typename std::vector<const Transition*>::const_iterator k = i->second.begin(); k != i->second.end(); ++k)
				{
					for (typename std::vector<const Transition*>::const_iterator l = j->second.begin(); l != j->second.end(); ++l)
					{
						assert((*k)->lhs().size() == (*l)->lhs().size());
						std::vector<size_t> lhs;
						for (size_t m = 0; m < (*k)->lhs().size(); ++m)
						{
							std::unordered_map<std::pair<size_t, size_t>, size_t, boost::hash<std::pair<size_t, size_t>>>::iterator n = product.find(
								std::make_pair((*k)->lhs()[m], (*l)->lhs()[m])
							);
							if (n == product.end())
								break;
							lhs.push_back(n->second);
						}
						if (lhs.size() < (*k)->lhs().size())
							continue;
						std::pair<std::unordered_map<std::pair<size_t, size_t>, size_t, boost::hash<std::pair<size_t, size_t>>>::iterator, bool> p =
							product.insert(std::make_pair(std::make_pair((*k)->rhs(), (*l)->rhs()), product.size() + stateOffset));
						f(*k, *l, lhs, p.first->second);
						if (p.second)
							changed = true;
					}
				}
			}
		}

		return product.size();
	}

	class Manager : Cache<TA*>::Listener
	{
		typename TA::Backend& backend_;

		Cache<TA*> taCache;
		std::vector<TA*> taPool;

	protected:

		virtual void drop(typename Cache<TA*>::value_type* x)
		{
			x->first->clear();
			this->taPool.push_back(x->first);
		}

	public:

		Manager(typename TA::Backend& backend_) :
			backend_(backend_),
			taCache{},
			taPool{}
		{
			this->taCache.addListener(this);
		}

		~Manager()
		{
			utils::erase(this->taPool);
			assert(this->taCache.empty());
		}

		const Cache<TA*>& getCache() const
		{
			return this->taCache;
		}

		TA* alloc()
		{
			TA* dst;
			if (!this->taPool.empty())
			{
				dst = this->taPool.back();
				this->taPool.pop_back();
			} else
			{
				dst = new TA(this->backend_);
			}
			return this->taCache.lookup(dst)->first;
		}

		TA* clone(TA* src, bool copyFinalStates = true)
		{
			assert(src);
			assert(src->backend_ == &this->backend_);
			//return this->taCache.lookup(new TA(*src, copyFinalStates))->first;
			return this->taCache.lookup(allocateTAWithSameFinalStates(
                        *src, copyFinalStates))->first;
		}

		TA* addRef(TA* x)
		{
			typename Cache<TA*>::value_type* v = this->taCache.find(x);
			assert(v);
			return this->taCache.addRef(v), x;
		}

		size_t release(TA* x)
		{
			typename Cache<TA*>::value_type* v = this->taCache.find(x);
			assert(v);
			return this->taCache.release(v);
		}

		void clear()
		{
			this->taCache.clear();
			for (TA* ta : this->taPool)
				delete ta;

			this->taPool.clear();
		}

		bool isAlive(TA* x)
		{
			return this->taCache.find(x) != nullptr;
		}

		typename TA::Backend& getBackend()
		{
			return this->backend_;
		}
	};

    const Transition& getTransitionFromPair(TransIDPair* pair)
    {
        return pair->first;
    }

	TransIDPair* internalAdd(const Transition& t)
	{
        TransIDPair* x = this->transCache().lookup(t);
        bool isTransitionNew = insertToTransitions(x);
            
		if (isTransitionNew)
		{
            updateMaxRank(t.lhs().size());
		} else
        { // if a transition is not a new one it is already cached
          // so the actually created cache entry is removed
            this->transCache().release(x);
        }
        
        return x;
	}

    /**
     * Insert given pair to the internal structure
     * for transitions_.
     * @param x Transition pair to be inserted
     * @return True If the new transition has been inserted
     * @return False If the transition has been already presented
     */
    bool insertToTransitions(TransIDPair* x)
    {
        std::pair<typename std::set<TransIDPair*, CmpF>::iterator, bool> insertRes =
            this->transitions_.insert(x);
        return insertRes.second;
    }

    void updateMaxRank(unsigned int rank)
    {
        if (rank > this->maxRank_)
		    this->maxRank_ = rank;
    }
    
    size_t newState()
	{
		return nextState_++;
	}

	void updateStateCounter()
	{
		nextState_ = 0;
		for (const TransIDPair* trans : this->transitions_)
		{
			nextState_ = std::max(
				nextState_,
				1 + std::max(
					trans->first.rhs(),
					*std::max_element(
						trans->first.lhs().begin(),
						trans->first.lhs().end())));
		}
	}

    void removeFinalState(size_t state)
	{
		finalStates_.erase(state);
	}

	void clearFinalStates()
	{
		finalStates_.clear();
	}

	size_t getAcceptingTransitionCount() const
	{
		// Assertions
		assert(this->accBegin() != this->accEnd());

		size_t cnt = 1;

		auto iter = this->accBegin();

		while (++iter != this->accEnd()) ++cnt;

		return cnt;
	}

    struct PredicateF
	{
		std::vector<size_t>& dst;
		const TA& predicate;

		PredicateF(
			std::vector<size_t>&          dst,
			const TA&                  predicate) :
			dst(dst),
			predicate(predicate)
		{ }

		void operator()(
			const Transition*               /* t1 */,
			const Transition*                  t2,
			const std::vector<size_t>&      /* lhs */,
			size_t                          /* rhs */)
		{
			if (predicate.isFinalState(t2->rhs()))
				this->dst.push_back(t2->rhs());
		}
	};

    void intersectingStates(
		std::vector<size_t>&                 dst,
		const TA&                         predicate) const
	{
		lt_cache_type cache1, cache2;
		this->buildLTCache(cache1);
		predicate.buildLTCache(cache2);
		TA::buProduct(cache1, cache2, TA::PredicateF(dst, predicate));
	}

	/**
	 * @brief  Determines whether two transitions_ match
	 *
	 * This function determines whether two transitions_ match (and can therefore
	 * e.g. be merged during abstraction). First, the @p funcMatch functor is used
	 * to determine whether the transitions_ are to be checked at all.
	 */
	template <class F>
	static bool transMatch(
		const Transition*                         t1,
		const Transition*                         t2,
		F                                         funcMatch,
		const std::vector<std::vector<bool>>&     mat,
		const Index<size_t>&                      stateIndex)
	{
		// Preconditions
		assert((nullptr != t1) && (nullptr != t2));

		if (!funcMatch(*t1, *t2))
			return false;

		if (t1->lhs().size() != t2->lhs().size())
			return false;

		for (size_t m = 0; m < t1->lhs().size(); ++m)
		{
			if (!mat[stateIndex[t1->lhs()[m]]][stateIndex[t2->lhs()[m]]])
			{
				return false;
			}
		}

		return true;
	}

    TA& uselessFree(TA& dst) const
	{
		std::vector<const TransIDPair*> v1(this->transitions_.begin(), this->transitions_.end()), v2;
		std::set<size_t> states;
		bool changed = true;
		while (changed)
		{
			changed = false;
			for (const TransIDPair* trans : v1)
			{
				bool matches = true;
				for (const size_t state : trans->first.lhs())
				{
					if (!states.count(state))
					{
						matches = false;
						break;
					}
				}
				if (matches)
				{
					if (states.insert(trans->first.rhs()).second)
						changed = true;
					dst.addTransition(trans);
				} else
				{
					v2.push_back(trans);
				}
			}
			v1.clear();
			std::swap(v1, v2);
		}

		for (const size_t& state : finalStates_)
		{
			if (states.count(state))
				dst.addFinalState(state);
		}

		return dst;
	}

    struct AcceptingF
	{
		const TA& ta;
		AcceptingF(const TA& ta) : ta(ta) {}
		bool operator()(const Transition* t) { return ta.isFinalState(t->rhs()); }
	};

    // makes state numbers contiguous
	TA& reduced(
		TA&                      dst,
		Index<size_t>&              index) const
	{
		return TA::reduce(dst, *this, index);
	}

	static TA& renamedUnion(
		TA&                     dst,
		const TA&               a,
		const TA&               b,
		size_t&                    aSize,
		size_t                     offset = 0)
	{
		Index<size_t> index;
		reduce(dst, a, index, offset);
		aSize = index.size();
		index.clear();
		reduce(dst, b, index, aSize, offset);
		return dst;
	}

	static TA& renamedUnion(
		TA&                    dst,
		const TA&              src,
		size_t                    offset,
		size_t&                   srcSize)
	{
		Index<size_t> index;
		reduce(dst, src, index, offset);
		srcSize = index.size();
		return dst;
	}
    
    /*
    friend std::ostream& operator<<(std::ostream& os, const TA& ta)
    {
        return os;
    }
    */
};

#endif
