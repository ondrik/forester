/*
 * Copyright (C) 2014 Martin Hruska
 *
 * This file is part of forester.
 *
 * forester is free software: you can redistribute it and/or modify
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

// VATA headers
#include "libvata/include/vata/explicit_tree_aut.hh"

class VATAAdapter
{
public:   // data types
	///	the type of a tree automaton transition
	typedef VATA::ExplicitTreeAut::Transition Transition;
    typedef VATA::ExplicitTreeAut::Iterator iterator;

private:
    typedef VATA::ExplicitTreeAut::SymbolType Symbol;
    typedef VATA::ExplicitTreeAut TreeAut;

private: // private data members
    TreeAut vataAut_;

private: // private methods
	VATAAdapter(TreeAut aut);

public: // public methods
	VATAAdapter();
	VATAAdapter(const VATAAdapter& adapter);

    static VATAAdapter createVATAAdapterWithSameTransitions(
		const VATAAdapter&         ta);

    static VATAAdapter* allocateVATAAdapterWithSameTransitions(
		const VATAAdapter&         ta);
    
    static VATAAdapter createVATAAdapterWithSameFinalStates(
		const VATAAdapter&         ta,
        bool                 copyFinalStates=true);

    static VATAAdapter* allocateVATAAdapterWithSameFinalStates(
		const VATAAdapter&         ta,
        bool                 copyFinalStates=true);
    
    ~VATAAdapter();

    void copyReachableTransitionsFromRoot(
            const VATAAdapter&        src,
            const size_t&    rootState);
    
	iterator begin() const;
	iterator end() const;

    VATAAdapter& operator=(const VATAAdapter& rhs);
	void addTransition(const Transition& transition);
    void addTransition(
		const std::vector<size_t>&          children,
		const Symbol&                       symbol,
		size_t                              parent);

    const Transition getTransition(
        const std::vector<size_t>&          children,
		const Symbol&                       symbol,
		size_t                              parent);

    void addFinalState(size_t state);
	void addFinalStates(const std::set<size_t>& states);

    bool isFinalState(size_t state) const;
	const std::unordered_set<size_t>& getFinalStates() const;
	size_t getFinalState() const;
    const Transition& getAcceptingTransition() const;

    VATAAdapter& unreachableFree(VATAAdapter& dst) const;
	VATAAdapter& uselessAndUnreachableFree(VATAAdapter& dst) const;

	static VATAAdapter& disjointUnion(
		VATAAdapter&                      dst,
		const VATAAdapter&                src,
		bool                              addFinalStates = true);

    VATAAdapter& minimized(VATAAdapter& dst) const;

    /*
    std::vector<const Transition*> getEmptyRootTransitions() const;

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

	void buildStateIndex(Index<size_t>& index) const;

    bool areTransitionsEmpty()
    {
        return this->transitions_.empty();
    }

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

	static bool subseteq(const TA& a, const TA& b);

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
			// predicate over transitions_ to be copied
			[](const Transition&){ return true; },
			addFinalStates);
	}

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

	template <class TVisitor>
	void accept(TVisitor& visitor) const
	{
		visitor(*this);
	}
    */
};
