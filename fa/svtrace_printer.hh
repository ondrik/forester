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

class SVTracePrinter
{
private:
	const static std::string START;
	const static std::string END;
	const static std::string DATA_START;
	const static std::string DATA_END;
	const static std::string NODE_START;
	const static std::string NODE_END;
	const static std::string ENTRY;
	const static std::string VIOLATION;
	const static std::string EDGE_START;
	const static std::string EDGE_END;
	const static std::string EDGE_TARGET;
	int nodeNumber_;
	int tokenNumber_;

public: // public methods

	SVTracePrinter() : nodeNumber_(1), tokenNumber_(0)
	{}


	void printTrace(
			const std::vector<const CodeStorage::Insn*>&   instrs,
			std::ostream&                                  out,
			const char*                                    filename);


private: // private methods
	struct token* letTokenize(
			const char*                                    filename,
			struct token*                                  end);


	static struct token* getNext(
			struct token*                                  begin,
			struct token*                                  next,
			const int                                      line);


	void printTokensOfLine(
			std::ostream&                                  out,
			const char*                                    filename,
			const std::string&                             line,
			struct token*                                  act,
			const struct token*                            end);


	std::string getToken(
			const struct token*                            token,
			const std::string&                             line);


	static struct token* findToken(
			struct token*                                  next,
			const struct token*                            end,
			const int                                      line);

	static void jumpToLine(
			std::ifstream&                                 in,
			const int                                      actLine,
			const int                                      nextLine,
			std::string&                                   line);


};

#endif
