#include "svtrace_printer.hh"

// code listener headers
#include <cl/storage.hh>

// std headers
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <exception>
#include <assert.h>

extern "C"
{
	#include "sparse/token.h"
}

namespace
{

	// XML initialization
	const std::string START       = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n\
	<graphml xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns=\"http://graphml.graphdrawing.org/xmlns\">\n\
		<key attr.name=\"assumption\" attr.type=\"string\" for=\"edge\" id=\"assumption\"/>\n\
		<key attr.name=\"sourcecode\" attr.type=\"string\" for=\"edge\" id=\"sourcecode\"/>\n\
		<key attr.name=\"sourcecodeLanguage\" attr.type=\"string\" for=\"graph\" id=\"sourcecodelang\"/>\n\
		<key attr.name=\"tokenSet\" attr.type=\"string\" for=\"edge\" id=\"tokens\"/>\n\
		<key attr.name=\"negativeCase\" attr.type=\"string\" for=\"edge\" id=\"negated\">\n\
			<default>false</default>\n\
		</key>\n\
		<key attr.name=\"lineNumberInOrigin\" attr.type=\"int\" for=\"edge\" id=\"originline\"/>\n\
		<key attr.name=\"originFileName\" attr.type=\"string\" for=\"edge\" id=\"originfile\">\n\
			<default>\"&lt;command-line&gt;\"</default>\n\
		</key>\n\
		<key attr.name=\"nodeType\" attr.type=\"string\" for=\"node\" id=\"nodetype\">\n\
			<default>path</default>\n\
		</key>\n\
		<key attr.name=\"isFrontierNode\" attr.type=\"boolean\" for=\"node\" id=\"frontier\">\n\
			<default>false</default>\n\
		</key>\n\
		<key attr.name=\"isViolationNode\" attr.type=\"boolean\" for=\"node\" id=\"violation\">\n\
			<default>false</default>\n\
		</key>\n\
		<key attr.name=\"isEntryNode\" attr.type=\"boolean\" for=\"node\" id=\"entry\">\n\
			<default>false</default>\n\
		</key>\n\
		<key attr.name=\"isSinkNode\" attr.type=\"boolean\" for=\"node\" id=\"sink\">\n\
			<default>false</default>\n\
		</key>\n\
		<key attr.name=\"enterFunction\" attr.type=\"string\" for=\"edge\" id=\"enterFunction\"/>\n\
		<key attr.name=\"returnFromFunction\" attr.type=\"string\" for=\"edge\" id=\"returnFrom\"/>\n\
		<graph edgedefault=\"directed\">\n\
			<data key=\"sourcecodelang\">C</data>\n";

	const std::string END         = "</graph>\n</graphml>";
	const std::string DATA_START  = "<data key=";
	const std::string DATA_END    = "</data>\n";
	const std::string NODE_START  = "<node id=";
	const std::string NODE_END    = "</node>";
	const std::string NODE_ENTRY  = "<data key=\"entry\">true</data>";
	const std::string NODE_NAME   = "A";
	const std::string VIOLATION   = "<data key=\"violation\">true";
	const std::string EDGE_START  = "<edge source=";
	const std::string EDGE_END    = "</edge>";
	const std::string EDGE_TARGET = " target=";


	// private constants
	const int TO_EOL = -1;


	/*
	struct token* getNext(
			struct token*                               begin,
			struct token*                               next,
			const int                                   line)
	{
		return ((line < next->pos.line) ? begin : next);
	}
	*/


	bool allRead(
			const int                                      current,
			const int                                      to)
	{
		return current > to && to != TO_EOL;
	}

	bool instrsEq(
			const CodeStorage::Insn*                       instr1,
			const CodeStorage::Insn*                       instr2)
	{
		return instr1->loc.line == instr2->loc.line &&
			instr1->loc.column >= instr2->loc.column &&
			instr1->code == instr2->code;
	}


	void printStart(
			std::ostream&                                  out)
	{
		out << START;
	}


	void printEnd(
			std::ostream&                                  out,
			int                                            endNode)
	{
		out << "\t" << NODE_START << "\"" << NODE_NAME << endNode << "\">\n";
		out << "\t" << VIOLATION << DATA_END;
		out << "\t" << NODE_END << "\n";
		out << "\t" << END;
	}


	int findCondEnd(struct token* act)
	{
		for (; std::string(show_token(act)) != ")"; act = act->next);
			
		return act->pos.pos+1;
	}


	size_t getEndColumn(
			const std::vector<const CodeStorage::Insn*>&   instrs,
			const size_t                                   pos,
			struct token*                                  actToken)
	{
		size_t to = TO_EOL;
		
		if (instrs.at(pos)->code == cl_insn_e::CL_INSN_COND)
		{
			to = findCondEnd(actToken);
		}
		else if (pos+1 < instrs.size() &&
				instrs.at(pos)->loc.line == instrs.at(pos+1)->loc.line)
		{
			to = instrs.at(pos+1)->loc.column-1;
		}
		else
		{
			struct token* ti;
			const int line = instrs.at(pos)->loc.line;
			for (ti = actToken; ti->pos.line == line && !eof_token(ti) && std::string(show_token(ti)) != ";"; ti = ti->next);
			to = ti->pos.pos+1;
		}

		return to;
	}
}


void SVTracePrinter::printTrace(
			const std::vector<const CodeStorage::Insn*>&   instrs,
			std::ostream&                                  out,
			const char*                                    filename)
{
	printStart(out);
		
	struct token* begin = letTokenize(filename);
	next_ = begin;

	for (size_t i = 0; i < instrs.size(); ++i)
	{
		const auto instr = instrs.at(i);
		const int lineNumber = instr->loc.line;

		//next = getNext(begin, next, lineNumber);
		std::cerr << "Instr code " << instr->code << " " << instr->loc.line << " " << instr->loc.column << '\n';
		std::cerr << "Instrs position " << i << " of " << instrs.size() << '\n';
		
		if (i+1 < instrs.size() && instrsEq(instr, instrs.at(i+1)))
		{
			continue;
		}

		if (nodeNumber_ == 1)
		{ // TODO this should be refactored
			out << "\t" << NODE_START << "\"" << NODE_NAME << nodeNumber_ << "\">\n\t"
				<< NODE_ENTRY << "\n\t" << NODE_END <<"\n";
		}
		else
		{
			out << "\t" << NODE_START << "\"" << NODE_NAME << nodeNumber_ << "/>\n";
		}

		findToken(instr->loc.column-1, lineNumber);

		size_t to = getEndColumn(instrs, i , next_);
		std::cerr << "Next round " << show_token(next_) << " to " << to << '\n';

		printTokens(
				out,
				filename,
				to);

		++nodeNumber_;
	}

	printEnd(out, nodeNumber_);
}

void SVTracePrinter::printTokens(
			std::ostream&                                 out,
			const char*                                   filename,
			const int                                     to)
{
	if (eof_token(next_) || next_ == NULL)
	{
		throw std::invalid_argument("SVTracePrinter: next_ is invalid token");
	}
	std::cerr << "Printing " << show_token(next_) << '\n';

	const int lineNumber = next_->pos.line;
	const int startTokenNumber = tokenNumber_;
	std::string code = "";

	struct token* i;
	struct token* last;
	for (
			i = next_;
			i->pos.line == lineNumber && !allRead(i->pos.pos, to) && !eof_token(i);
			last = i, i = i->next)
	{
		++tokenNumber_;
		code += std::string(show_token(i)) + '\n';
	}
	code += "\n";

	// Printing XML
	const std::string indent("\t\t\t");
	out << "\t\t" << EDGE_START << "\"" << NODE_NAME << nodeNumber_ << "\"" <<
		EDGE_TARGET << "\""<<  NODE_NAME << nodeNumber_+1 << "\">\n";
	out << indent << DATA_START << "\"sourcecode\">" << code << "\t\t\t" << DATA_END;

	if (startTokenNumber == tokenNumber_)
	{
		out << indent << DATA_START <<"\"tokens\">" << tokenNumber_ << DATA_END;
	}
	else
	{
		out << indent << DATA_START << "\"tokens\">" << startTokenNumber << "," <<
			tokenNumber_ << DATA_END;
	}

	out << indent << DATA_START << "\"originfile\">\""<< filename << "\"" << DATA_END;
    out << indent << DATA_START << "\"originline\">"<< lineNumber << DATA_END;
	out << "\t" << EDGE_END << "\n";

	if (i->pos.line != lineNumber)
	{
		next_ = last;
	}
	else
	{
		next_ = i;
	}
}


void SVTracePrinter::findToken(
			const int                                   from,
			const int                                   line)
{
	struct token* i = next_;
	const int startLine = i->pos.line;

	std::cerr << "Start finding " << startLine << " " << line << " || " << i->pos.pos << " " << from  << '\n';
	// jump to line
	for (; !eof_token(i) && i->pos.line != line; i = i->next)
	{
		++tokenNumber_;
	}

	std::cerr << "Over lines to " << show_token(i) << '\n';
	if (startLine != line)
	{
		next_ = i;
		std::cerr << "Found " << show_token(next_) << '\n';
		return;
	}

	// jump to token
	for (; !eof_token(i) && i->pos.line == line && i->pos.pos < from; i = i->next)
	{
		std::cerr << "Jump over " << i->pos.pos << " " << from << " " << show_token(i) << '\n';
		++tokenNumber_;
	}

	if (!(i->pos.line == line && i->pos.pos >= from))
	{
		next_ = NULL;
	}
	else
	{
		next_ = i;
	}
	std::cerr << "Over tokens to " << show_token(next_) << '\n';
}


struct token* SVTracePrinter::letTokenize(
		const char*                                     filename)
{
	const char *includepath[4] = {
		"",
		"/usr/include",
		"/usr/local/include",
		NULL
	};
	
	int fd = open(filename, O_RDONLY);
	if (fd < 0)
	{
		throw std::runtime_error("Cannot open file \"" + std::string(filename) + "\" for tokenizing\n");
	}
	struct token* begin = tokenize(filename, fd, NULL, includepath);
	close(fd);

	return preprocess(begin);
}
