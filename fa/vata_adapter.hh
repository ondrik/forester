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
 * forester is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with forester.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef VATA_AUT_H
#define VATA_AUT_H

#include "utils.hh"
#include "vata_abstraction.hh"
#include "label.hh"
#include "streams.hh"

// VATA headers
#include "libvata/include/vata/explicit_tree_aut.hh"

class VATAAdapter
{
private:
    typedef VATA::ExplicitTreeAut TreeAut;
    typedef TreeAut::SymbolType SymbolType;

public:   // data types
	///	the type of a tree automaton transition
	typedef TreeAut::Transition Transition;
    typedef TreeAut::Iterator iterator;
    typedef TreeAut::DownAccessor DownAccessor;
    typedef TreeAut::AcceptTrans AcceptTrans;

private: // private data types
	typedef typename std::unordered_map<size_t,
            std::vector<const Transition*>> tdCacheType;

    class CopyAllFunctor : public TreeAut::AbstractCopyF
    {
    public:
        bool operator()(const Transition&)
        {
            return true;
        }
    };

    class CopyNonAcceptingFunctor : public TreeAut::AbstractCopyF
    {
    private:
        const VATAAdapter& ta_;
    public:
        CopyNonAcceptingFunctor(const VATAAdapter& ta) : ta_(ta)
        {}
        bool operator()(const Transition& t)
        {
            if (ta_.isFinalState(t.GetParent()))
            {
                return false;
            }

            return true;
        }
    };

private: // data members
    TreeAut vataAut_;

private: // private methods
	VATAAdapter(TreeAut aut);

public: // public methods
	VATAAdapter();
	VATAAdapter(const VATAAdapter& adapter);
    ~VATAAdapter();

    static VATAAdapter createTAWithSameTransitions(
		const VATAAdapter&         ta);

    static VATAAdapter* allocateTAWithSameTransitions(
		const VATAAdapter&         ta);
    
    static VATAAdapter createTAWithSameFinalStates(
		const VATAAdapter&         ta,
        bool                 copyFinalStates=true);

    static VATAAdapter* allocateTAWithSameFinalStates(
		const VATAAdapter&         ta,
        bool                 copyFinalStates=true);

	iterator begin() const;
	iterator end() const;

    DownAccessor::Iterator begin(size_t rhs) const;
    DownAccessor::Iterator end(size_t rhs) const;
    DownAccessor::Iterator end(
            size_t rhs,
            DownAccessor::Iterator i) const;

    AcceptTrans::Iterator accBegin() const;
	AcceptTrans::Iterator accEnd() const;
	AcceptTrans::Iterator accEnd(
            AcceptTrans::Iterator i) const;

    VATAAdapter& operator=(const VATAAdapter& rhs);

    void addTransition(
		const std::vector<size_t>&          children,
		const SymbolType&                   symbol,
		size_t                              parent);
    void addTransition(const Transition& transition);

    const Transition& getTransition(
        const std::vector<size_t>&          children,
		const SymbolType&                       symbol,
		size_t                              parent);

	static const label_type GetSymbol(const Transition& t);

    void addFinalState(size_t state);
	void addFinalStates(const std::set<size_t>& states);
	void addFinalStates(const std::unordered_set<size_t>& states);

    bool isFinalState(size_t state) const;
	const std::unordered_set<size_t>& getFinalStates() const;
	size_t getFinalState() const;
    const Transition& getAcceptingTransition() const;

    VATAAdapter& unreachableFree(VATAAdapter& dst) const;
	VATAAdapter& uselessAndUnreachableFree(VATAAdapter& dst) const;
    VATAAdapter& minimized(VATAAdapter& dst) const;

	static VATAAdapter& disjointUnion(
		VATAAdapter&                      dst,
		const VATAAdapter&                src,
		bool                              addFinalStates = true);

    /**
     * Return true if there are no transitions in automata
     * @returns True if there are no transitions, otherwise false
     */
    bool areTransitionsEmpty();

	VATAAdapter& copyTransitions(VATAAdapter& dst) const;

	template <class TVisitor>
	void accept(TVisitor& visitor) const
	{
		visitor(*this);
	}

	VATAAdapter& copyNotAcceptingTransitions(
            VATAAdapter& dst,
            const VATAAdapter& ta) const;

    void clear();
	static bool subseteq(const VATAAdapter& a, const VATAAdapter& b);

    VATAAdapter& unfoldAtRoot(
		VATAAdapter&             dst,
		size_t                   newState,
		bool                     registerFinalState = true) const;

	VATAAdapter& unfoldAtRoot(
		VATAAdapter&                                  dst,
		const std::unordered_map<size_t, size_t>&     states,
		bool                                          registerFinalState = true) const;

	void buildStateIndex(Index<size_t>& index) const;
    TreeAut::AcceptTrans getEmptyRootTransitions() const;

    void copyReachableTransitionsFromRoot(
            const VATAAdapter&        src,
            const size_t&    rootState);

    template <class F>
    static VATAAdapter& rename(
            VATAAdapter&                   dst,
            const VATAAdapter&             src,
            F                              funcRename,
            bool                           addFinalStates = true)
    {
        FA_DEBUG_AT(1,"TA rename\n");
        std::vector<size_t> children;
        if (addFinalStates)
        {
            for (auto state : src.getFinalStates())
            {
                dst.addFinalState(funcRename(state));
            }
        }

        for (auto trans : src.vataAut_)
        {
            children.resize(trans.GetChildrenSize());
            for (size_t j = 0; j < trans.GetChildrenSize(); ++j)
            {
                children[j] = funcRename(trans.GetNthChildren(j));
            }

            dst.addTransition(children,
                    trans.GetSymbol(),
                    funcRename(trans.GetParent()));
        }

        return dst;
    }

    // currently erases '1' from the relation
	template <class F>
	void heightAbstraction(
		std::vector<std::vector<bool>>&            result,
		size_t                                     height,
		F                                          f,
		const Index<size_t>&                       stateIndex) const
	{
        FA_DEBUG_AT(1,"TA height abstraction\n");
        VATAAbstraction::heightAbstraction(vataAut_, result, height, f, stateIndex);
	}

	// collapses states according to a given relation
	VATAAdapter& collapsed(
		VATAAdapter&                             dst,
		const std::vector<std::vector<bool>>&    rel,
		const Index<size_t>&                     stateIndex) const;
};

#endif
