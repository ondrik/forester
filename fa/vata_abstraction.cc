#include "vata_abstraction.hh"

void VATAAbstraction::completeSymmetricIndex(
        std::vector<std::vector<bool>>& result)
{
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
