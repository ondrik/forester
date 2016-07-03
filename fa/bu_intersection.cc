#include "bu_intersection.hh"

BUIntersection::BUProductResult BUIntersection::bottomUpIntersection(
        const FAE&            fwdFAE,
        const FAE&            bwdFAE)
{
    assert(fwdFAE.getRootCount() == bwdFAE.getRootCount());
    FA_DEBUG_AT(1, "empty input fwd " << fwdFAE);
    FA_DEBUG_AT(1, "empty input bwd " << bwdFAE);

    VATA::AutBase::ProductTranslMap productMap;

    TreeAutVec res;
    for (size_t i = 0; i < fwdFAE.getRootCount(); ++i)
    {
        if (bwdFAE.getRoot(i) == nullptr || fwdFAE.getRoot(i) == nullptr)
        {
            res.push_back(nullptr);
            continue;
        }

        TreeAut isectTA = TreeAut::intersectionBU(
                *(fwdFAE.getRoot(i)),*(bwdFAE.getRoot(i)), &productMap);

        std::shared_ptr<TreeAut> finalIsectTA = std::shared_ptr<TreeAut>(new TreeAut());
        isectTA.uselessAndUnreachableFree(*finalIsectTA);

        std::shared_ptr<TreeAut> renamedfinalTA = std::shared_ptr<TreeAut>(new TreeAut());

        // Now we need to rename states to distinguish again between data and
        // normal state. This was lost because VATA ignores semantics of states
        // numbering
        auto renamingFunction = [&productMap](const size_t productState) -> size_t {
            auto productMapItem = std::find_if(
                    productMap.begin(), productMap.end(), [&productState](
                            std::pair<std::pair<size_t, size_t>, size_t> p) {
                return p.second == productState;
            });

            const std::pair<size_t, size_t>& prodPair = productMapItem->first;

            // When a state is data one, it is same in the lhs and the rhs
            assert((FA::isData(prodPair.first) && FA::isData(prodPair.second)) ||
                           (!FA::isData(prodPair.first) && !FA::isData(prodPair.second)));
            assert((!FA::isData(prodPair.first) && !FA::isData(prodPair.second)) ||
                           (prodPair.first == prodPair.second));

            // When it is a data state returns its original number (taken from lhs),
            // otherwise keep it same
            return !FA::isData(prodPair.first) ? productState : prodPair.first;
        };
        TreeAut::rename(*renamedfinalTA, *finalIsectTA, renamingFunction);
        assert(TreeAut::subseteq(*renamedfinalTA, *finalIsectTA) &&
                       TreeAut::subseteq(*finalIsectTA, *renamedfinalTA));

        FA_DEBUG_AT(1, "result before " << isectTA);
        FA_DEBUG_AT(1, "result after " << *finalIsectTA);
        FA_DEBUG_AT(1, "result named" << *renamedfinalTA);

        res.push_back(renamedfinalTA);
    }

    assert(fwdFAE.getRootCount() == res.size());
    return BUProductResult(res, productMap);
}

bool BUIntersection::isResultNonEmpty(const BUProductResult &result)
{
    for (auto& ta : result.tas_)
	{
		if (ta->getFinalStates().empty())
        {
            return false;
        }
	}

    return true;
}

