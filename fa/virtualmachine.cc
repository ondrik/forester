/*
 * Copyright (C) 2012 Jiri Simacek
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

// Forester headers
#include "virtualmachine.hh"


void VirtualMachine::transitionLookup(
	const Transition&              transition,
	size_t                         base,
	const std::vector<size_t>&     offsets,
	Data&                          data) const
{
	data = Data::createStruct();

	// for every offset, add an item
	for (size_t off : offsets)
	{
		const NodeLabel::NodeItem& ni = TreeAut::GetSymbol(transition)->nodeLookup(off + base);
		// Assertions
		assert(VirtualMachine::isSelectorWithOffset(ni.aBox, off + base));

		const Data* tmp = nullptr;
		if (!fae_.isData(transition.GetNthChildren(ni.offset), tmp))
		{
			throw std::runtime_error("transitionLookup(): destination is not a leaf!");
		}
		data.d_struct->push_back(Data::item_info(off, *tmp));
		VirtualMachine::displToData(VirtualMachine::readSelector(ni.aBox),
			data.d_struct->back().second);
	}
}


void VirtualMachine::transitionLookup(
	const Transition&           transition,
	size_t                      offset,
	Data&                       data) const
{
	// retrieve the item at given offset
	const NodeLabel::NodeItem& ni = TreeAut::GetSymbol(transition)->nodeLookup(offset);
	// Assertions
	assert(VirtualMachine::isSelectorWithOffset(ni.aBox, offset));

	const Data* tmp = nullptr;
	if (!fae_.isData(transition.GetNthChildren(ni.offset), tmp))
	{
		throw std::runtime_error("transitionLookup(): destination is not a leaf!");
	}

	data = *tmp;
	VirtualMachine::displToData(VirtualMachine::readSelector(ni.aBox), data);
}


void VirtualMachine::transitionModify(
	TreeAut&                            dst,
	const Transition&                   transition,
	size_t                              offset,
	const Data&                         in,
	Data&                               out)
{
	struct SingleData : public OutDataFunctor
	{
		virtual void save(Data& out, const size_t sel, const Data& temp)
		{
			out = temp;
		}


		virtual Data& get(Data& out)
		{
			return out;
		}
	};

	SingleData functor;
	/// In with key zero pushed because zero is neutral to addition
	/// so it will be possible to use general function for transition modification
	std::vector<std::pair<size_t, Data>> tempVector = {std::pair<size_t, Data>(0, in)};
	transitionModifyInternal(
			dst,
			transition,
			offset,
			tempVector,
			out,
			functor);
}


void VirtualMachine::transitionModify(
	TreeAut&                                        dst,
	const Transition&                               transition,
	size_t                                          base,
	const std::vector<std::pair<size_t, Data>>&     in,
	Data&                                           out)
{
	struct MultiData : public OutDataFunctor
	{
		virtual void save(Data& out, const size_t sel, const Data& temp)
		{
			out.d_struct->push_back(Data::item_info(sel, temp));
		}


		virtual Data& get(Data& out)
		{
			return out.d_struct->back().second;
		}
	};

	MultiData functor;
	out = Data::createStruct();
	transitionModifyInternal(
			dst,
			transition,
			base,
			in,
			out,
			functor);
}


const NodeLabel::NodeItem& VirtualMachine::getNodeItem(
		const Transition&                               transition,
		const size_t                                    base,
		const std::pair<size_t, Data>&                  oldSelector) const
{
	// Retrieve the item with the given offset
	const NodeLabel::NodeItem& ni = TreeAut::GetSymbol(transition)->
			nodeLookup(oldSelector.first + base);
	// Assertions
	assert(VirtualMachine::isSelectorWithOffset(ni.aBox, oldSelector.first + base));

	return ni;
}


const Data* VirtualMachine::getData(
		const Transition&                               transition,
		const NodeLabel::NodeItem&                      ni)
{
	const Data* tmp = nullptr;
	if (!fae_.isData(transition.GetNthChildren(ni.offset), tmp))
	{
		throw std::runtime_error("transitionModify(): destination is not a leaf!");
	}

	return tmp;
}


VirtualMachine::DataSelector VirtualMachine::createDataSelector(
		const NodeLabel::NodeItem&                      ni,
		Data&                                           out,
		OutDataFunctor&                                 outFunc,
		const std::pair<size_t, Data>&                  oldSelector,
		const Data*                                     data)
{
	outFunc.save(out, oldSelector.first, *data);
	SelData s = VirtualMachine::readSelector(ni.aBox);
	VirtualMachine::displToData(s, outFunc.get(out));
	Data d = oldSelector.second;
	VirtualMachine::displToSel(s, d);

	return DataSelector(d,s);
}


void VirtualMachine::addSelector(
		TreeAut&                                        dst,
		const DataSelector&                             dataSelector,
		const NodeLabel::NodeItem&                      ni,
		std::vector<size_t>&                            lhs,
		std::vector<const AbstractBox*>&                label)
{
	lhs[ni.offset] = fae_.addData(dst, dataSelector.data);
	label[ni.index] = fae_.boxMan->getSelector(dataSelector.selector);
}


void VirtualMachine::pushIfStruct(
		const std::pair<size_t, Data>&           sel,
		std::vector<std::pair<size_t, Data>>&    inStructStack)
{
	if (sel.second.isStruct())
	{
		for (auto& structItem : *sel.second.d_struct)
		{
			inStructStack.push_back(std::pair<size_t, Data>(sel.first + structItem.first, structItem.second));
		}
	}
}


void VirtualMachine::transitionModifyInternal(
		TreeAut&                                        dst,
		const Transition&                               transition,
		const size_t                                    base,
		const std::vector<std::pair<size_t, Data>>&     in,
		Data&                                           out,
		OutDataFunctor&                                 outFunc)
{
	// Create a new final state
	const size_t state = fae_.freshState();
	dst.addFinalState(state);

	std::vector<size_t> lhs = transition.GetChildren();

	// Get the label
	std::vector<const AbstractBox*> label =
        TreeAut::GetSymbol(transition)->getNode();

	out = Data::createStruct();

	std::vector<std::pair<size_t, Data>> structSelectorStack;

	for (const std::pair<size_t, Data>& sel : in)
	{
		pushIfStruct(sel, structSelectorStack);
		const NodeLabel::NodeItem& ni = getNodeItem(transition, base, sel);
		DataSelector dataSelector =
			createDataSelector(ni, out, outFunc, sel, getData(transition, ni));
		addSelector(dst, dataSelector, ni, lhs, label);
	}

	while (!structSelectorStack.empty())
	{ // loop for the inner structure elements
		const std::pair<size_t, Data> sel = structSelectorStack.front();
		structSelectorStack.erase(structSelectorStack.begin());

		pushIfStruct(sel, structSelectorStack);
		const NodeLabel::NodeItem& ni = getNodeItem(transition, base, sel);
		DataSelector dataSelector =
			createDataSelector(ni, out, outFunc, sel, getData(transition, ni));
		addSelector(dst, dataSelector, ni, lhs, label);
	}

	FAE::reorderBoxes(label, lhs);
	dst.addTransition(lhs, fae_.boxMan->lookupLabel(label), state);
}


size_t VirtualMachine::nodeCreate(
	const std::vector<SelData>&          nodeInfo,
	const TypeBox*                       typeInfo)
{
	// Assertions
	assert(nullptr != fae_.boxMan);

	// create a new tree automaton
	size_t root = fae_.getRootCount();
	TreeAut* ta = fae_.allocTA();
	size_t f = fae_.freshState();
	ta->addFinalState(f);

	const std::vector<SelData>* ptrNodeInfo = nullptr;

	// build the label
	std::vector<const AbstractBox*> label;
	if (typeInfo)
	{	// if there is a some box
		label.push_back(typeInfo);

		ptrNodeInfo = fae_.boxMan->LookupTypeDesc(typeInfo, nodeInfo);
	}

	for (const SelData& sel : nodeInfo)
	{	// push selector
		label.push_back(fae_.boxMan->getSelector(sel));
	}

	// build the tuple
	std::vector<size_t> lhs(nodeInfo.size(),
		fae_.addData(*ta, Data::createUndef()));

	// reorder
	FAE::reorderBoxes(label, lhs);

	// fill the rest
	const label_type boxLabel = fae_.boxMan->lookupLabel(label, ptrNodeInfo);
	ta->addTransition(lhs, boxLabel, f);

	// add the tree automaton into the forest automaton
	fae_.appendRoot(ta);
	fae_.connectionGraph.newRoot();
	return root;
}


void VirtualMachine::nodeDelete(size_t root)
{
	// Assertions
	assert(root < fae_.getRootCount());
	assert(nullptr != fae_.getRoot(root));

	// update content of variables referencing the tree automaton
	fae_.SetVarsToUndefForRoot(root);

	// erase node
	fae_.setRoot(root, nullptr);

	// make all references to this rootpoint dangling
	for (size_t i = 0; i < fae_.getRootCount(); ++i)
	{
		if (root == i)
		{	// for the 'root'
			fae_.connectionGraph.invalidate(i);
		}

		if (!fae_.getRoot((i)))
			continue;

		fae_.setRoot(i, std::shared_ptr<TreeAut>(
			fae_.invalidateReference(fae_.getRoot(i).get(), root)));
		fae_.connectionGraph.invalidate(i);
	}
}


void VirtualMachine::reinsertModifiedTA(
	const size_t            root,
	TreeAut&                ta)
{
	// Note that this operations also adds an old root state from the TA fae_[root]
	// but it is not set as a root by this function so it will be removed by
	// operation eliminating unreachable states (function unreachableFree).
	fae_.getRoot(root)->copyTransitions(ta);
	assert(fae_.getRoot(root)->getAcceptingTransition().GetParent() !=
			ta.getAcceptingTransition().GetParent());
	TreeAut* tmp = fae_.allocTA();
	ta.unreachableFree(*tmp);
	fae_.setRoot(root, std::shared_ptr<TreeAut>(tmp));
	fae_.connectionGraph.invalidate(root);

}

void VirtualMachine::nodeModify(
	size_t                      root,
	size_t                      offset,
	const Data&                 in,
	Data&                       out)
{
	// Assertions
	assert(root < fae_.getRootCount());
	assert(nullptr != fae_.getRoot(root));

	TreeAut ta = fae_.createTAWithSameBackend();
	this->transitionModify(
		ta,
		fae_.getRoot(root)->getAcceptingTransition(),
		offset,
		in,
		out);
	reinsertModifiedTA(root, ta);
}


void VirtualMachine::nodeModifyMultiple(
	size_t                      root,
	size_t                      offset,
	const Data&                 in,
	Data&                       out)
{
	// Assertions
	assert(root < fae_.getRootCount());
	assert(nullptr != fae_.getRoot(root));
	assert(in.isStruct());

	TreeAut ta = fae_.createTAWithSameBackend();
	this->transitionModify(ta, fae_.getRoot(root)->getAcceptingTransition(),
		offset, *in.d_struct, out);
	reinsertModifiedTA(root, ta);
}

SelData VirtualMachine::getSelector(
	const size_t                root,
	const size_t                offset) const
{
	const Transition& trans = fae_.getRoot(root)->getAcceptingTransition();
	const NodeLabel::NodeItem& ni = TreeAut::GetSymbol(trans)->nodeLookup(offset);
	return VirtualMachine::readSelector(ni.aBox);
}

void VirtualMachine::selectorModify(
	const size_t                root,
	const size_t                offset,
	const SelData&              newSel)
{
	// Assertions
	assert(root < fae_.getRootCount());
	assert(nullptr != fae_.getRoot(root));

	TreeAut ta = fae_.createTAWithSameBackend();

	const Transition& trans = fae_.getRoot(root)->getAcceptingTransition();
	std::vector<size_t> lhs = trans.GetChildren();
	std::vector<const AbstractBox*> label = TreeAut::GetSymbol(trans)->getNode();

	const NodeLabel::NodeItem& ni = TreeAut::GetSymbol(trans)->nodeLookup(offset);
	label[ni.index] = fae_.boxMan->getSelector(newSel);

	const size_t newRoot = fae_.freshState();
	ta.addFinalState(newRoot);
	ta.addTransition(lhs, fae_.boxMan->lookupLabel(label), newRoot);

	reinsertModifiedTA(root, ta);
}

void VirtualMachine::selectorDisplModify(
	const size_t                root,
	const size_t                offset,
	const size_t                newDispl)
{
	SelData sel = getSelector(root, offset);
	sel.displ = newDispl;
	selectorModify(root, offset, sel);
}

void VirtualMachine::getNearbyReferences(
	size_t                       root,
	std::set<size_t>&            out) const
{
	// Assertions
	assert(root < fae_.getRootCount());
	assert(nullptr != fae_.getRoot(root));

	const Transition& t = fae_.getRoot(root)->getAcceptingTransition();
	for (size_t state : t.GetChildren())
	{
		const Data* data = nullptr;
		if (fae_.isData(state, data) && data->isRef())
			out.insert(data->d_ref.root);
	}
}

void VirtualMachine::nodeCopy(
	size_t                          dstRoot,
	const VirtualMachine&           srcVM,
	size_t                          srcRoot)
{
	// Assertions
	assert(dstRoot < fae_.getRootCount());
	assert(srcRoot < srcVM.fae_.getRootCount());
	assert(nullptr == fae_.getRoot(dstRoot));
	assert(nullptr != srcVM.fae_.getRoot(dstRoot));

	// copy the TA
	TreeAut* tmp = fae_.allocTA();
	*tmp = *srcVM.fae_.getRoot(srcRoot);
	fae_.setRoot(dstRoot, std::shared_ptr<TreeAut>(tmp));
}
