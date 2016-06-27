#ifndef BU_INTERSECTION_H
#define BU_INTERSECTION_H

#include <vector>
#include <memory>

#include "forestaut.hh"
#include "forestautext.hh"
#include "streams.hh"
#include "vata_adapter.hh"

class BUIntersection
{
private:
    using TreeAut = VATAAdapter;

public:
    using TreeAutVec = std::vector<std::shared_ptr<const TreeAut>>;

    struct BUProductResult
    {
        TreeAutVec tas_;
        VATA::AutBase::ProductTranslMap productMap_;

        BUProductResult(
                TreeAutVec& tav,
                VATA::AutBase::ProductTranslMap productMap) :
            tas_(tav), productMap_(productMap)
        { }
    };


    static BUProductResult bottomUpIntersection(
            const FAE&            fwdFAE,
            const FAE&            bwdFAE);
};
#endif