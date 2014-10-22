#ifndef SVTRACE_PRINTER_H
#define SVTRACE_PRINTER_H

#include <vector>
#include <ostream>
#include <fstream>

namespace CodeStorage {
	struct Insn;
}

extern "C" {
	struct token;
}

/**
 * @brief Class provides method for printing trace compatible with SV-Comp format.
 */
class SVTracePrinter
{

private: // private members

	int nodeNumber_;
	int tokenNumber_;
	struct token* next_;

public: // public methods

	SVTracePrinter() : nodeNumber_(1), tokenNumber_(0), next_(nullptr)
	{}


	/*
	 * @brief Method prints trace from @instrs to the @out using @filename to get tokens.
	 *
	 * This methods tokenize program in @filename. Then it iterates over
	 * 
	 * @instrs which contains the instructions of a error trace. It gradually
	 * creates trace graph using tokenized program and print it to @out.
	 * @param[in]  instrs     Instruction included in error trace.
	 * @param[out] out        Output stream where the graph is printed
	 * @param[in]  filename   File with program which has been analyzed
	 */
	void printTrace(
			const std::vector<const CodeStorage::Insn*>&   instrs,
			std::ostream&                                  out,
			const char*                                    filename);


private: // private methods

	struct token* letTokenize(
			const char*                                    filename);


	void printTokens(
			std::ostream&                                  out,
			const char*                                    filename,
			const int                                      to);


	void findToken(
			const int                                      from,
			const int                                      line);
};

#endif
