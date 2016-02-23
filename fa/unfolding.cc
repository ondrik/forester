#include "unfolding.hh"

namespace
{
    TreeAut TARelabeledToBox(
        FAE&                           fae,
        const TreeAut&                 boxRoot,
        const std::vector<size_t>&     rootIndex)
    {
            TreeAut relabeledTA = fae.createTAWithSameBackend();
            fae.relabelReferences(relabeledTA, boxRoot, rootIndex);

            return relabeledTA;
    }

    TreeAut copyAndRelabelTAToBox(
        FAE&                           fae,
        const TreeAut&                 boxRoot,
        const std::vector<size_t>&     rootIndex)
    {
        TreeAut newTA = fae.createTAWithSameBackend();
//		this->fae.boxMan->adjustLeaves(tmp2, boxRoot);
        fae.unique(newTA, TARelabeledToBox(fae, boxRoot, rootIndex));

        return newTA;
    }

    template <class T>
    std::vector<T> vectorConcat(const std::vector<T>& v1, const std::vector<T>& v2)
    {
        auto res(v1);
        res.insert(res.end(), v2.begin(), v2.end());

        return res;
    }
}


void Unfolding::boxMerge(
		TreeAut&                       dst,
		const TreeAut&                 src,
		const TreeAut&                 boxRoot,
		const Box*                     box,
		const std::vector<size_t>&     rootIndex)
{
	TreeAut relabeledTA = copyAndRelabelTAToBox(this->fae, boxRoot, rootIndex);
    //		this->fae.boxMan->adjustLeaves(relabeledTA, boxRoot);
    // Copy the original and relabeled automaton to destination
    relabeledTA.copyNotAcceptingTransitions(dst, relabeledTA);
    dst.addFinalStates(relabeledTA.getFinalStates());
    src.copyNotAcceptingTransitions(dst, src);

    for (const size_t state : src.getFinalStates())
    {
        for (auto originalTrans = src.begin(state); originalTrans != src.end(state); ++originalTrans)
        {
            std::vector<size_t> boxLhs;
            std::vector<const AbstractBox*> boxLabel;
            getChildrenAndLabelFromBox(box, *originalTrans, boxLhs, boxLabel);

            for (auto relabeledTrans = relabeledTA.accBegin();
                 relabeledTrans != relabeledTA.accEnd(); ++relabeledTrans)
            {
                std::vector<size_t> newLhs =
                        vectorConcat(boxLhs, (*relabeledTrans).GetChildren());
                std::vector<const AbstractBox*> newLabel =
                        vectorConcat(boxLabel, TreeAut::GetSymbol(*relabeledTrans)->getNode());

                FA::reorderBoxes(newLabel, newLhs);

                if (nullptr == this->fae.boxMan->lookupLabel(newLabel).obj_)
                {
                    throw std::runtime_error("Cannot find box -- equivalent to invalid deref");
                }
                assert(nullptr != this->fae.boxMan->lookupLabel(newLabel).obj_);
                dst.addTransition(
                        newLhs,
                        static_cast<VATAAdapter::SymbolType>(
                                this->fae.boxMan->lookupLabel(newLabel)),
                        (*relabeledTrans).GetParent()
                );
            }
        }
    }
}

void Unfolding::getChildrenAndLabelFromBox(
    const Box*                         box,
    const TreeAut::Transition&         transition,
    std::vector<size_t>&               children,
    std::vector<const AbstractBox*>&   label)
{
    size_t childrenOffset = 0;
    if (box)
    {
        bool found = false;
        for (const AbstractBox* aBox : TreeAut::GetSymbol(transition)->getNode())
        {
            if (!aBox->isStructural())
            {
                label.push_back(aBox);
                continue;
            }
            const StructuralBox* b = static_cast<const StructuralBox*>(aBox);
            if (b != static_cast<const StructuralBox*>(box))
            {
                // this box is not interesting
                for (size_t k = 0; k < b->getArity(); ++k, ++childrenOffset)
                    children.push_back(transition.GetNthChildren(childrenOffset));
                label.push_back(b);
                continue;
            }
            childrenOffset += box->getArity();

            if (found)
                assert(false);

            found = true;
        }

        if (!found)
            assert(false);

    } else
    {
        children = transition.GetChildren();
        label = TreeAut::GetSymbol(transition)->getNode();
    }
}

void Unfolding::initRootRefIndex(
		std::vector<size_t>&          index,
		const Box*                    box,
		const TreeAut::Transition&    t)
{
    size_t lhsOffset = 0;

	// First initialize structures before unfolding
    for (const AbstractBox* aBox : TreeAut::GetSymbol(t)->getNode())
    {
        if (static_cast<const AbstractBox*>(box) != aBox)
        {
            lhsOffset += aBox->getArity();
        }
		else
		{
			for (size_t j = 0; j < box->getArity(); ++j)
			{
				const Data& data = this->fae.getData(t.GetNthChildren(lhsOffset + j));

				if (data.isUndef())
					index.push_back(static_cast<size_t>(-1));
				else
					index.push_back(data.d_ref.root);
			}

	        return;
		}
    }
}

void Unfolding::substituteOutputPorts(
		const std::vector<size_t>&    index,
		const size_t                  root,
		const Box*                    box)
{
    auto ta = std::shared_ptr<TreeAut>(this->fae.allocTA());

    this->boxMerge(*ta, *this->fae.getRoot(root), *box->getOutput(), box, index);

    this->fae.setRoot(root, ta);
    this->fae.connectionGraph.invalidate(root);
}

void Unfolding::substituteInputPorts(
		const std::vector<size_t>&    index,
		const Box*                    box)
{
    assert(box->getInputIndex() < index.size());
	const size_t aux = index.at(box->getInputIndex() + 1);

    assert(aux != static_cast<size_t>(-1));
    assert(aux < this->fae.getRootCount());

    TreeAut tmp = this->fae.createTAWithSameBackend();

    this->fae.getRoot(aux)->unfoldAtRoot(tmp, this->fae.freshState());
    this->fae.setRoot(aux, std::shared_ptr<TreeAut>(this->fae.allocTA()));

    this->boxMerge(*this->fae.getRoot(aux), tmp, *box->getInput(), nullptr, index);

    this->fae.connectionGraph.invalidate(aux);
}


void Unfolding::unfoldBox(const size_t root, const Box* box)
{
    //std::cerr << "FA " << this->fae;
    //std::cerr << "Root Number " << root << std::endl;
    assert(root < this->fae.getRootCount());
    //std::cerr << "TA " << *(this->fae.getRoot(root));
    assert(nullptr != this->fae.getRoot(root));
    assert(nullptr != box);

    const TreeAut::Transition& t =
        this->fae.getRoot(root)->getAcceptingTransition();

    std::vector<size_t> index = { root };
	initRootRefIndex(index, box, t);

	substituteOutputPorts(index, root, box);

    if (!box->getInput())
    {
        return;
    }

	substituteInputPorts(index, box);
}

void Unfolding::unfoldBoxes(const size_t root, const std::set<const Box*>& boxes)
{
    for (std::set<const Box*>::const_iterator i = boxes.begin(); i != boxes.end(); ++i)
        this->unfoldBox(root, *i);
}
