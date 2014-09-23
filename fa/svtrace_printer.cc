#include "svtrace_printer.hh"

#include <cl/storage.hh>
extern "C"
{
	#include "sparse/token.h"
}

// XML initialization
const std::string SVTracePrinter::START = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\
<graphml xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns=\"http://graphml.graphdrawing.org/xmlns\">\
    <key attr.name=\"assumption\" attr.type=\"string\" for=\"edge\" id=\"assumption\"/>\
    <key attr.name=\"sourcecode\" attr.type=\"string\" for=\"edge\" id=\"sourcecode\"/>\
    <key attr.name=\"sourcecodeLanguage\" attr.type=\"string\" for=\"graph\" id=\"sourcecodelang\"/>\
    <key attr.name=\"tokenSet\" attr.type=\"string\" for=\"edge\" id=\"tokens\"/>\
    <key attr.name=\"originTokenSet\" attr.type=\"string\" for=\"edge\" id=\"origintokens\"/>\
    <key attr.name=\"negativeCase\" attr.type=\"string\" for=\"edge\" id=\"negated\">\
        <default>false</default>\
    </key>\
    <key attr.name=\"lineNumberInOrigin\" attr.type=\"int\" for=\"edge\" id=\"originline\"/>\
    <key attr.name=\"originFileName\" attr.type=\"string\" for=\"edge\" id=\"originfile\">\
        <default>\"&lt;command-line&gt;\"</default>\
    </key>\
    <key attr.name=\"nodeType\" attr.type=\"string\" for=\"node\" id=\"nodetype\">\
        <default>path</default>\
    </key>\
    <key attr.name=\"isFrontierNode\" attr.type=\"boolean\" for=\"node\" id=\"frontier\">\
        <default>false</default>\
    </key>\
    <key attr.name=\"isViolationNode\" attr.type=\"boolean\" for=\"node\" id=\"violation\">\
        <default>false</default>\
    </key>\
    <key attr.name=\"isEntryNode\" attr.type=\"boolean\" for=\"node\" id=\"entry\">\
        <default>false</default>\
    </key>\
    <key attr.name=\"isSinkNode\" attr.type=\"boolean\" for=\"node\" id=\"sink\">\
        <default>false</default>\
    </key>\
    <key attr.name=\"enterFunction\" attr.type=\"string\" for=\"edge\" id=\"enterFunction\"/>\
    <key attr.name=\"returnFromFunction\" attr.type=\"string\" for=\"edge\" id=\"returnFrom\"/>\
	<graph edgedefault=\"directed\">\
        <data key=\"sourcecodelang\">C</data>\
        <node id=\"A1\">\
            <data key=\"entry\">true</data>\
        </node>";


const std::string SVTracePrinter::END = "</graph>\
	</graphml>";

void SVTracePrinter::printTrace(
			const std::vector<CodeStorage::Insn*>&   instrs,
			std::ostream&                            out,
			std::istream&                            in)
{
	out << START;
	struct token* begin = tokenize();
	struct token end = *begin; // TODO
	struct token* next = begin;
	
	for (const auto instr : instrs)
	{
		const int lineNumber = instr->loc.line;
		next = getNext(begin, next, lineNumber);
		std::string line;
		std::getline(in, line);
		printNode(out, line, findToken(next, &end, lineNumber), &end);
		++nodeNumber_;
	}

	out << END;
	return;
}


void SVTracePrinter::printNode(
			std::ostream&                            out,
			const std::string&                       line,
			struct token*                            act,
			const struct token*                      end)
{
	if (act == end)
	{
		return;
	}

	const int lineNumber = act->pos.line;
	const int startTokenNumber = tokenNumber_;
	std::string code = "";
	struct token* last = act;

	for (struct token* i = act->next; i->pos.line == lineNumber && i != end; i = i->next)
	{
		++tokenNumber_;
		code += getToken(i, line);
		// TODO test correct tokenizing here!
	}

	++nodeNumber_;
	return;
}


struct token* SVTracePrinter::getNext(
			struct token*                     begin,
			struct token*                     next,
			const int                         line)
{
	return ((line < next->pos.line) ? begin : next);
}


struct token* SVTracePrinter::findToken(
			struct token*                            next,
			const struct token*                      end,
			const int                                line)
{
	struct token* i = next;
	for (; i != end || i->pos.line != line; i = i->next);

	return i;
}


struct token* SVTracePrinter::tokenize()
{
	struct token* tok = new token();
	return NULL;
}

std::string SVTracePrinter::getToken(
		const struct token*                      token,
		const std::string&                       line)
{
	const int end = (token->pos.line != token->next->pos.line) ? line.length() : token->next->pos.pos;
	std::string res = "";

	for (int i = token->pos.pos; i < end && line[i-1] != '\n'; ++i)
	{
		res += line[i-1];
	}
	res += "\0";

	return res;
}
