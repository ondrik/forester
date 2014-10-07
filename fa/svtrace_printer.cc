#include "svtrace_printer.hh"

// code listener headers
#include <cl/storage.hh>

// std headers
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <exception>

extern "C"
{
	#include "sparse/token.h"
}


// XML initialization
const std::string SVTracePrinter::START = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n\
<graphml xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns=\"http://graphml.graphdrawing.org/xmlns\">\n\
    <key attr.name=\"assumption\" attr.type=\"string\" for=\"edge\" id=\"assumption\"/>\n\
    <key attr.name=\"sourcecode\" attr.type=\"string\" for=\"edge\" id=\"sourcecode\"/>\n\
    <key attr.name=\"sourcecodeLanguage\" attr.type=\"string\" for=\"graph\" id=\"sourcecodelang\"/>\n\
    <key attr.name=\"tokenSet\" attr.type=\"string\" for=\"edge\" id=\"tokens\"/>\n\
    <key attr.name=\"originTokenSet\" attr.type=\"string\" for=\"edge\" id=\"origintokens\"/>\n\
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

const std::string SVTracePrinter::END = "</graph>\n</graphml>";
const std::string SVTracePrinter::DATA_START = "<data key=";
const std::string SVTracePrinter::DATA_END = "</data>\n";
const std::string SVTracePrinter::NODE_START = "<node id=";
const std::string SVTracePrinter::NODE_END = "</node>";
const std::string SVTracePrinter::NODE_ENTRY = "<data key=\"entry\">true</data>";
const std::string SVTracePrinter::NODE_NAME = "A";
const std::string SVTracePrinter::VIOLATION = "<data key=\"violation\">true";
const std::string SVTracePrinter::EDGE_START = "<edge source=";
const std::string SVTracePrinter::EDGE_END = "</edge>";
const std::string SVTracePrinter::EDGE_TARGET = " target=";


// private constants
const int SVTracePrinter::TO_EOL = -1;


void SVTracePrinter::printTrace(
			const std::vector<const CodeStorage::Insn*>&   instrs,
			std::ostream&                                  out,
			const char*                                    filename)
{
	out << START;
		
	struct token* begin = letTokenize(filename);
	struct token* next = begin;

	for (size_t i = 0; i < instrs.size(); ++i)
	{
		const auto instr = instrs.at(i);
		const int lineNumber = instr->loc.line;

		next = getNext(begin, next, lineNumber);
		
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

		size_t to = TO_EOL;
		if (i+1 < instrs.size() && instr->loc.line == instrs.at(i+1)->loc.line)
		{
			to = instrs.at(i+1)->loc.column-1;
		}

		printTokens(
				out,
				filename,
				findToken(next, instr->loc.column-1, lineNumber),
				to);

		++nodeNumber_;
	}

	out << "\t" << NODE_START << "\"" << NODE_NAME << nodeNumber_ << "\">\n";
    out << "\t" << VIOLATION << DATA_END;
    out << "\t" << NODE_END << "\n";

	out << "\t" << END;
}

void SVTracePrinter::printTokens(
			std::ostream&                                 out,
			const char*                                   filename,
			struct token*                                 act,
			const int                                     to)
{
	if (eof_token(act) || act == NULL)
	{
		throw std::invalid_argument("SVTracePrinter: act is invalid token");
	}

	const int lineNumber = act->pos.line;
	const int startTokenNumber = tokenNumber_;
	std::string code = "";

	struct token* i;
	for (
			i = act;
			i->pos.line == lineNumber && !allRead(i->pos.pos, to) && !eof_token(i);
			i = i->next)
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
	
	if (startTokenNumber == tokenNumber_)
	{
		out << indent << DATA_START << "\"origintokens\">" <<
			tokenNumber_ << DATA_END;
	}
	else
	{
		out << indent << DATA_START << "\"origintokens\">" << startTokenNumber
			<< "," << tokenNumber_ << DATA_END;
	}

    out << indent << DATA_START << "\"originline\">"<< lineNumber << DATA_END;
	out << "\t" << EDGE_END << "\n";
}

struct token* SVTracePrinter::getNext(
			struct token*                               begin,
			struct token*                               next,
			const int                                   line)
{
	return ((line < next->pos.line) ? begin : next);
}


struct token* SVTracePrinter::findToken(
			struct token*                               next,
			const int                                   from,
			const int                                   line)
{
	struct token* i = next;
	bool first = false;
	for (; !eof_token(i) && i->pos.line != line; i = i->next) first = true; // jump to line
	if (first)
	{
		return i;
	}

	// jump to token
	for (; !eof_token(i) && i->pos.line == line && i->pos.pos < from; i = i->next);

	if (!(i->pos.line == line && i->pos.pos >= from))
	{
		return NULL;
	}
	else
	{
		return i;
	}
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


bool SVTracePrinter::allRead(
			const int                                      current,
			const int                                      to)
{
	return current > to && to != TO_EOL;
}

bool SVTracePrinter::instrsEq(
			const CodeStorage::Insn*                       instr1,
			const CodeStorage::Insn*                       instr2)
{
	return instr1->loc.line == instr2->loc.line &&
		instr1->loc.column >= instr2->loc.column &&
		instr1->code == instr2->code;
}
