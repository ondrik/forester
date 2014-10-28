#ifndef SVTRACE_LITE_H
#define SVTRACE_LITE_H

#include <vector>
#include <ostream>
#include <fstream>

namespace CodeStorage {
	struct Insn;
}

/**
 * @brief Class provides method for printing trace compatible with SV-Comp format.
 */
class SVTraceLite
{

private: // private members

	int nodeNumber_;

public: // public methods

	SVTraceLite() : nodeNumber_(1)
	{}


	/*
	 * @brief Method prints trace from @instrs to the @out using.
	 *
	 * @instrs which contains the instructions of a error trace. It gradually
	 * creates trace graph and print it to @out.
	 * @param[in]  instrs     Instruction included in error trace.
	 * @param[out] out        Output stream where the graph is printed
	 */
	void printTrace(
			const std::vector<const CodeStorage::Insn*>&   instrs,
			std::ostream&                                  out);
};

#endif
