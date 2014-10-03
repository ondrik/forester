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
private: // XML marks
	const static std::string START;
	const static std::string END;
	const static std::string DATA_START;
	const static std::string DATA_END;
	const static std::string NODE_START;
	const static std::string NODE_END;
	const static std::string NODE_ENTRY;
	const static std::string NODE_NAME;
	const static std::string VIOLATION;
	const static std::string EDGE_START;
	const static std::string EDGE_END;
	const static std::string EDGE_TARGET;

private: // private members
	int nodeNumber_;
	int tokenNumber_;

public: // public methods

	SVTracePrinter() : nodeNumber_(1), tokenNumber_(0)
	{}


	/*
	 * @brief Method prints trace from @instrs to the @out using @filename to get tokens.
	 * This methods tokenize program in @filename. Then it iterates over
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


	static struct token* getNext(
			struct token*                                  begin,
			struct token*                                  next,
			const int                                      line);


	void printTokensOfLine(
			std::ostream&                                  out,
			const char*                                    filename,
			struct token*                                  act);


	static struct token* findToken(
			struct token*                                  next,
			const int                                      line);


	static void jumpToLine(
			std::ifstream&                                 in,
			const int                                      actLine,
			const int                                      nextLine,
			std::string&                                   line);


};

#endif
