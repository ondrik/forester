#ifndef SVTRACE_PRINTER_H
#define SVTRACE_PRINTER_H

#include <vector>
#include <ostream>
#include <istream>

namespace CodeStorage {
	struct Insn;
}

extern "C" {
	struct token;
}

class SVTracePrinter
{
private:
	const static std::string START;
	const static std::string END;
	int nodeNumber_;
	int tokenNumber_;

public: // public methods

	SVTracePrinter() : nodeNumber_(1), tokenNumber_(0)
	{}


	void printTrace(
			const std::vector<CodeStorage::Insn*>&   instrs,
			std::ostream&                            out,
			std::istream&                            in);


private: // private methods
	struct token* tokenize();


	static struct token* getNext(
			struct token*                            begin,
			struct token*                            next,
			const struct CodeStorage::Insn*          instr);


	void printNode(
			std::ostream&                            out,
			const std::string&                       line,
			struct token*                            act,
			const struct token*                      end);

	std::string getToken(
			const struct token*                      token,
			const std::string&                       line);

	static struct token* findToken(
			struct token*                            next,
			const struct token*                      end,
			const struct CodeStorage::Insn*          instr);


};

#endif
