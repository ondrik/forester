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


void SVTracePrinter::printTrace(
			const std::vector<const CodeStorage::Insn*>&   instrs,
			std::ostream&                                  out,
			const char*                                    filename)
{
	out << START;
		
	std::ifstream in(filename, std::ifstream::in);
	struct token* begin = letTokenize(filename);
	struct token* next = begin;
	int lastLine = 0;

	for (const auto instr : instrs)
	{
		const int lineNumber = instr->loc.line;
		if (lineNumber == lastLine)
		{
			continue;
		}

		next = getNext(begin, next, lineNumber);

		std::string line;
		jumpToLine(in, lastLine, lineNumber, line);
		lastLine = lineNumber;
		
		if (line.length() == 0)
		{
			continue;
		}

		if (nodeNumber_ == 1)
		{
			out << "\t" << NODE_START << "\"" << NODE_NAME << nodeNumber_ << "\">\n\t"
				<< NODE_ENTRY << "\n\t" << NODE_END <<"\n";
		}
		else
		{
			out << "\t" << NODE_START << "\"" << NODE_NAME << nodeNumber_ << "/>\n";
		}

		printTokensOfLine(
				out,
				filename,
				findToken(next, lineNumber+1));
		++nodeNumber_;
	}

	out << "\t" << NODE_START << "\"" << NODE_NAME << nodeNumber_ << "\">\n";
    out << "\t" << VIOLATION << DATA_END;
    out << "\t" << NODE_END << "\n";

	out << "\t" << END;
	in.close();
	return;
}


void SVTracePrinter::printTokensOfLine(
			std::ostream&                                 out,
			const char*                                   filename,
			struct token*                                 act)
{
	if (eof_token(act))
	{
		return;
	}

	const int lineNumber = act->pos.line;
	const int startTokenNumber = tokenNumber_;
	std::string code = "";

	for (struct token* i = act; i->pos.line == lineNumber && !eof_token(i); i = i->next)
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

	return;
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
			const int                                   line)
{
	struct token* i = next;
	struct token* j = i;
	for (; !eof_token(i) && i->pos.line != line; j=i, i = i->next);

	return i;
}


struct token* SVTracePrinter::letTokenize(
		const char*                                     filename)
{
	const char *next_path[2] = {
		"",
		NULL
	};
	struct token end;

	int fd = open(filename, O_RDONLY);
	if (fd < 0)
	{
		throw std::runtime_error("Cannot open file \"" + std::string(filename) + "\" for tokenizing\n");
	}
	struct token* begin = tokenize(filename, fd, &end, next_path);
	close(fd);

	return begin;
}


void SVTracePrinter::jumpToLine(
			std::ifstream&                                 in,
			const int                                      actLine,
			const int                                      nextLine,
			std::string&                                   line)
{
	for (int i = actLine; i < nextLine; ++i)
	{
		std::getline(in, line);
	}
}

