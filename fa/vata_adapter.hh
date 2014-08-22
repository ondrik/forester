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
    // iterator over accepting transitions
    typedef TreeAut::AcceptTrans AcceptTrans;

private: // private data types
    /**
     * @brief Functor copies all transitions
     *
     * Functor used for copying of the transition from one
     * automaton to another. Functor always returns true so
     * all transitions are copied.
     * */
    class CopyAllFunctor : public TreeAut::AbstractCopyF
    {
    public:
        bool operator()(const Transition&)
        {
            return true;
        }
    };

    /**
     * @brief functor copies only not accepting transitions
     *
     * Functor confirm copying only of the transitions
     * which are not accepting
     */
    class CopyNonAcceptingFunctor : public TreeAut::AbstractCopyF
    {
    private:
        const VATAAdapter& ta_;
    public:
        CopyNonAcceptingFunctor(const VATAAdapter& ta) : ta_(ta)
        {}
        bool operator()(const Transition& t)
        {
            return !ta_.isFinalState(t.GetParent());
        }
    };

private: // private constants
    const int cEmptyRootTransIndex = 0; // index of root transition

private: // data members
    TreeAut vataAut_;

private: // private methods
	VATAAdapter(TreeAut aut);

public: // public methods
	VATAAdapter();
	VATAAdapter(const VATAAdapter& adapter);
    ~VATAAdapter();

    /**
     * @brief Function returns a new TA with same transitions as the given one
     *
     * Function returns a new VATAAdapter which has same transitions
     * as that one in @ta
     *
     * @param[in] ta Tree Automata which transitions should be copied
     */
    static VATAAdapter createTAWithSameTransitions(
		const VATAAdapter&         ta);

    /**
     * @brief Function allocates a new TA with same transitions as the given one
     *
     * Function returns a pointer to a newly allocated VATAAdapter
     * which has same transitions as that one in @ta
     *
     * @param[in] ta Tree Automata which transitions should be copied
     */
    static VATAAdapter* allocateTAWithSameTransitions(
		const VATAAdapter&         ta);
    
    /**
     * @brief Function could create TA with same transitions and final states
     *
     * Function returns a new TA with the same transitions as the TA @ta
     * and also with the same final states if @copyFinalStates is true
     * 
     * @param[in] ta Tree Automata which transitions
     * and final states should be copied
     * @param[in] copyFinalStates Determines if final states should be copied
     */
    static VATAAdapter createTAWithSameFinalStates(
		const VATAAdapter&         ta,
        bool                       copyFinalStates=true);

    /**
     * @brief Function could allocate TA with same transitions and final states
     *
     * Function returns a pointer to a newly allocated TA with
     * the same transitions as the TA @ta and also with the same final states
     * if @copyFinalStates is true
     * 
     * @param[in] ta Tree Automata which transitions
     * and final states should be copied
     * @param[in] copyFinalStates Determines if final states should be copied
     */
    static VATAAdapter* allocateTAWithSameFinalStates(
		const VATAAdapter&         ta,
        bool                 copyFinalStates=true);

    /**
     * @brief Returns iterator over transitions
     *
     * Returns iterator pointing to the begin of the
     * transitions of @vataAut_
     *
     * @return Iterator over transitions of @vataAut_
     */
	iterator begin() const;

    /**
     * @brief Returns iterator pointing to the end of transitions
     *
     * @return End of transitions of @vataAut
     */
	iterator end() const;

    /**
     * @brief Returns iterator over transitions with parent @rhs
     *
     * Return iterator over a set of the transitions having @rhs
     * state as the parent
     *
     * @paremeter[in] rhs State which is parent of transitions
     * @return Iterator over trasitions with @rhs as parent
     * */
    DownAccessor::Iterator begin(size_t parent) const;
    DownAccessor::Iterator end(size_t parent) const;
    DownAccessor::Iterator end(
            size_t                       parent,
            DownAccessor::Iterator       i) const;

    /**
     * @brief Returns iterator over a set of the accepting transitions
     *
     * @return Iterator over a set of the accpeting transitions of @vataAut_
     */
    AcceptTrans::Iterator accBegin() const;
	AcceptTrans::Iterator accEnd() const;
	AcceptTrans::Iterator accEnd(
            AcceptTrans::Iterator i) const;

    /**
     * @brief Copies TreeAut of @rhs to this @vataAut_
     *
     * Copies TreeAut of @rhs to this @vataAut_. It works
     * like a classical copy operator.
     * 
     * @return Reference to this object
     */
    VATAAdapter& operator=(const VATAAdapter& rhs);

    /**
     * @brief Adds a new transition to @vataAut_
     *
     * Method adds a new transition with the given parameters
     * to the underlying tree automaton @vataAut_
     * @param[in] children A vector of the children states of the new transition
     * @param[in] symbol A symbol of the new transition
     * @param[in] parent A parent state of the new transition
     */
    void addTransition(
		const std::vector<size_t>&          children,
		const SymbolType&                   symbol,
		size_t                              parent);
    /**
     * @brief Adds a new @transition to @vataAut_
     *
     * Method adds a new @transition to the underlying tree automaton @vataAut_
     *
     * @param[in] trasition A new transition to be add to tree automaton
     * @param[in] symbol A symbol of the new transition
     * @param[in] parent A parent state of the new transition
     */
    void addTransition(const Transition& transition);

    /**
     * @brief Returns transition with the same parameters
     *
     * Returns transition with the same @children vector of states,
     * with the same @symbol and @parent state.
     * @precondition is that such a transition exists
     *
     * @param[in] children A vector of the children states of the wanted transition
     * @param[in] symbol A symbol of the wanted transition
     * @param[in] parent A parent state of the wanted transition
     * @return Transition with the same parts
     */
    const Transition getTransition(
        const std::vector<size_t>&          children,
		const SymbolType&                   symbol,
		size_t                              parent);

    /**
     * @brief Returns label_type structure for a given transition
     *
     * It returns label_type which is created from symbol of @t
     * by cast from uintptr_t type to label_type
     *
     * @return label_type structure describing symbol of @t
     */
	static const label_type GetSymbol(const Transition& t);

    /**
     * @brief Add one final state to the set of final states of @treeAut_
     *
     * @param[in] state State which is being added to the final states set
     */
    void addFinalState(size_t state);
    
    /**
     * @brief Add a set of states to the set of final states of @treeAut_
     *
     * Adds a set of states to the set of fibal states of @treeAut_.
     * This method has parameter type compatible with VATA method.
     *
     * @param[in] states States to be set as the final ones
     */
	void addFinalStates(const std::set<size_t>& states);
    
    /**
     * @brief Add a set of states to the set of final states of @treeAut_
     *
     * Adds a set of states to the set of fibal states of @treeAut_.
     * This method hasn't parameter type compatible with VATA method.
     *
     * @param[in] states States to be set as the final ones
     */
	void addFinalStates(const std::unordered_set<size_t>& states);

    /**
     * @brief Check whether a given state is final one
     *
     * @return Returns true if a given state is final one
     */
    bool isFinalState(size_t state) const;
    
    /**
     * @brief Returns a set of the final states of @vataAut_
     *
     * @return A set of final states
     */
	const std::unordered_set<size_t>& getFinalStates() const;
    
    /**
     * @brief Returns one final state
     *
     * Method returns one final state. A precondition
     * of the methods is that there is just one final state in
     * @vataAut_
     *
     * @return The final state
     */
	size_t getFinalState() const;

    /**
     * @brief Returns one accepting transition.
     *
     * Method returns one accepting transition.
     * A precondition is that there exists just one accepting transition.
     */
    const Transition getAcceptingTransition() const;

    /**
     * @brief Remove unreachable states from @vataAut_ and saves resulting TA to @dst.
     *
     * Removes all unreachable states from @vataAut_ and result of this operation
     * saves as underlying TA of @dst and returns @dst
     *
     * @param[out] dst Output automata withou unreachable states
     * @return TA equivalent to @vataAut_ without unreachable states
     */
    VATAAdapter& unreachableFree(VATAAdapter& dst) const;

    /**
     * @brief Remove useless states from @vataAut_ and saves resulting TA to @dst.
     *
     * Removes all useless states from @vataAut_ and result of this operation
     * saves as underlying TA of @dst and returns @dst
     *
     * @param[out] dst Output automata withou useless states
     * @return TA equivalent to @vataAut_ without useless states
     */
	VATAAdapter& uselessAndUnreachableFree(VATAAdapter& dst) const;
    
    /**
     * @brief Remove unreachable and useless states from @vataAut_ and saves resulting TA to @dst.
     *
     * Removes all unreachable and useless states from @vataAut_ and result of this operation
     * saves as underlying TA of @dst and returns @dst
     *
     * @param[out] dst Output automata withou unreachable states
     * @return TA equivalent to @vataAut_ without unreachable states
     */
    VATAAdapter& minimized(VATAAdapter& dst) const;

    /**
     * @brief Union of two automata with disjoint states sets
     *
     * Method creates union of two automata which have disjoint
     * set of states. Disjoint of states is guranteed by user
     * of the method
     *
     * @param[in/out] dst Result automaton and also one of the operands of the union
     * @param[in] src States of this automaton will be copied
     * @param[in] addFinalStates Determines whether final states will be also copied
     */
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
        FA_DEBUG_AT(1,"TA accept\n");
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
        if (addFinalStates)
        {
            for (auto state : src.getFinalStates())
            {
                dst.addFinalState(funcRename(state));
            }
        }

        for (const Transition trans : src.vataAut_)
        {
            std::vector<size_t> children;
            for (size_t j : trans.GetChildren())
            {
                children.push_back(funcRename(j));
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

    friend std::ostream& operator<<(std::ostream& os, const VATAAdapter& ta);
};

#endif
