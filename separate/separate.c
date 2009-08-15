#include "code_listener.h"

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// sparse headers
#include <sparse/expression.h>
#include <sparse/flow.h>
#include <sparse/linearize.h>
#include <sparse/parse.h>
#include <sparse/scope.h>
#include <sparse/storage.h>
#include <sparse/symbol.h>
#include <sparse/token.h>

// safe to remove (it's here for debugging purposes only)
#include <signal.h>
#define TRAP raise(SIGTRAP)

#define DO_EXPAND_SYMBOL            1
#define DO_PER_EP_UNSAA             1
#define DO_PER_EP_SET_UP_STORAGE    1
#define SHOW_PSEUDO_INSNS           0

#define WARN_UNHANDLED(what) \
    fprintf(stderr, "--- %s: %d: warning: '%s' not handled\n", \
            __FUNCTION__, __LINE__, (what))

#define WARN_UNHANDLED_SYM(sym) \
    WARN_UNHANDLED(show_ident(sym->ident))

#define WARN_VA(...) do {\
    fprintf(stderr, "--- %s: %d: warning: ", \
            __FUNCTION__, __LINE__); \
    fprintf (stderr, __VA_ARGS__); \
    fprintf (stderr, "\n"); \
} while (0)

#define CASE_UNHANDLED(what) \
    case what: WARN_UNHANDLED(#what); break;

// FIXME: hard-coded for now
static const unsigned KNOWN_PTR_SIZE = 32;

static bool is_pseudo(pseudo_t pseudo)
{
    return pseudo
        && pseudo != VOID;
}

static void free_cl_operand_data(struct cl_operand *op)
{
    free((char *) op->name);
    free((char *) op->offset);
    switch (op->type) {
        case CL_OPERAND_STRING:
            free((char *) op->value.text);
            break;

        default:
            break;
    }
}

static char* strdup_if_not_null(const char *str) {
    if (str)
        return strdup(str);
    else
        return NULL;
}

static void pseudo_to_cl_operand(struct instruction *insn, pseudo_t pseudo,
                                 struct cl_operand *op)
{
    // to not call free(3) on dangling pointer afterwards
    op->name = NULL;
    op->offset = NULL;

    // may be overridden afterwards
    op->deref = false;

    if (!is_pseudo(pseudo)) {
        op->type = CL_OPERAND_VOID;
        return;
    }

    switch(pseudo->type) {
        case PSEUDO_SYM: {
            struct symbol *sym = pseudo->sym;
            struct expression *expr;

            if (sym->bb_target) {
                WARN_UNHANDLED("sym->bb_target");
                op->type = CL_OPERAND_VOID;
                return;
            }
            if (sym->ident) {
                op->type            = CL_OPERAND_VAR;
                op->name            = strdup(show_ident(sym->ident));
                break;
            }
            expr = sym->initializer;
            if (expr) {
                switch (expr->type) {
#if 0
                    case EXPR_VALUE:
                        printf("<symbol value: %lld>", expr->value);
                        break;
#endif
                    case EXPR_STRING:
                        // FIXME: not dup'ed now (subtle)
                        op->type = CL_OPERAND_STRING;
                        op->value.text = strdup_if_not_null(
                                show_string(expr->string));
                        return;

                    default:
                        TRAP;
                }
            }
            break;
        }

        case PSEUDO_REG:
            op->type            = CL_OPERAND_REG;
            op->name            = strdup(show_ident(pseudo->ident));
            op->value.reg_id    = pseudo->nr;
            break;

        case PSEUDO_VAL: {
            long long value = pseudo->value;

            op->type            = CL_OPERAND_INT;
            op->value.num_int   = value;
            return;
        }

        case PSEUDO_ARG:
            op->type            = CL_OPERAND_ARG;
            op->value.arg_pos   = pseudo->nr;
            break;

        case PSEUDO_PHI:
            WARN_UNHANDLED("PSEUDO_PHI");
            break;

        default:
            TRAP;
    }

    if (insn->type) {
        const struct ident *id = insn->type->ident;
        const char *id_string = show_ident(id);
        if (id
                /* FIXME: deref? */
                && 0 != strcmp("__ptr", id_string))
        {
            op->offset = strdup(id_string);
        }
    }
}

static void handle_insn_call(struct instruction *insn,
                             struct cl_code_listener *cl)
{
    struct cl_operand dst, fnc;
    struct pseudo *arg;
    int cnt = 0;

    // open call
    pseudo_to_cl_operand(insn, insn->target, &dst);
    pseudo_to_cl_operand(insn, insn->func, &fnc);
    cl->insn_call_open(cl, insn->pos.line, &dst, &fnc);
    free_cl_operand_data(&dst);
    free_cl_operand_data(&fnc);

    // go through arguments
    FOR_EACH_PTR(insn->arguments, arg) {
        struct cl_operand src;
        pseudo_to_cl_operand(insn, arg, &src);

        cl->insn_call_arg(cl, ++cnt, &src);
        free_cl_operand_data(&src);
    } END_FOR_EACH_PTR(arg);

    // close call
    cl->insn_call_close(cl);
}

static void handle_insn_br(struct instruction *insn,
                           struct cl_code_listener *cl)
{
    char *bb_name_true = NULL;
    char *bb_name_false = NULL;
    struct cl_operand op;

    if (asprintf(&bb_name_true, "%p", insn->bb_true) < 0)
        die("asprintf failed");

    if (!is_pseudo(insn->cond)) {
        cl->insn_jmp(cl, insn->pos.line, bb_name_true);
        free(bb_name_true);
        return;
    }

    if (asprintf(&bb_name_false, "%p", insn->bb_false) < 0)
        die("asprintf failed");

    pseudo_to_cl_operand(insn, insn->cond, &op);
    cl->insn_cond(cl, insn->pos.line, &op, bb_name_true, bb_name_false);
    free_cl_operand_data(&op);

    free(bb_name_true);
    free(bb_name_false);
}

static void handle_insn_ret(struct instruction *insn,
                            struct cl_code_listener *cl)
{
    struct cl_operand op;
    pseudo_to_cl_operand(insn, insn->src, &op);
    cl->insn_ret(cl, insn->pos.line, &op);
    free_cl_operand_data(&op);
}

static void insn_assignment_base(struct instruction                 *insn,
                                 struct cl_code_listener            *cl,
                                 pseudo_t     lhs,        pseudo_t  rhs,
                                 bool         lhs_deref,  bool      rhs_deref)
{
    struct cl_operand op_lhs;
    struct cl_operand op_rhs;

    pseudo_to_cl_operand(insn, lhs, &op_lhs);
    pseudo_to_cl_operand(insn, rhs, &op_rhs);

    op_lhs.deref = lhs_deref;
    op_rhs.deref = rhs_deref;

    cl->insn_unop(cl, insn->pos.line, CL_UNOP_ASSIGN, &op_lhs, &op_rhs);

    free_cl_operand_data(&op_lhs);
    free_cl_operand_data(&op_rhs);
}

static void handle_insn_store(struct instruction *insn,
                              struct cl_code_listener *cl)

{
    insn_assignment_base(insn, cl,
            insn->symbol, insn->target,
            true        , false);
}
static void handle_insn_load(struct instruction *insn,
                             struct cl_code_listener *cl)
{
    insn_assignment_base(insn, cl,
            insn->target, insn->symbol,
            false       , true);
}
static void handle_insn_copy(struct instruction *insn,
                             struct cl_code_listener *cl)
{
    insn_assignment_base(insn, cl,
            insn->target, insn->src,
            false       , false);
}

static void handle_insn_add(struct instruction *insn,
                            struct cl_code_listener *cl)
{
    struct cl_operand dst, src1, src2;
    pseudo_to_cl_operand(insn, insn->target, &dst);
    pseudo_to_cl_operand(insn, insn->src1, &src1);
    pseudo_to_cl_operand(insn, insn->src2, &src2);

    cl->insn_binop(cl, insn->pos.line, CL_BINOP_ADD, &dst, &src1, &src2);

    free_cl_operand_data(&dst);
    free_cl_operand_data(&src1);
    free_cl_operand_data(&src2);
}

static void handle_insn_set_eq(struct instruction *insn,
                               struct cl_code_listener *cl)
{
    WARN_UNHANDLED("==");
}

static void handle_insn(struct instruction *insn, struct cl_code_listener *cl)
{
    switch (insn->opcode) {
        CASE_UNHANDLED(OP_BADOP)

        /* Entry */
        case OP_ENTRY:
            // ignore for now
            break;

        /* Terminator */
        case OP_RET /*= OP_TERMINATOR*/:
            handle_insn_ret(insn, cl);
            break;

        case OP_BR:
            handle_insn_br(insn, cl);
            break;

        CASE_UNHANDLED(OP_SWITCH)
        CASE_UNHANDLED(OP_INVOKE)
        CASE_UNHANDLED(OP_COMPUTEDGOTO)
        CASE_UNHANDLED(OP_TERMINATOR_END /*= OP_UNWIND*/)

        /* Binary */
        case OP_ADD /*= OP_BINARY*/:
            handle_insn_add(insn, cl);
            break;

        CASE_UNHANDLED(OP_SUB)
        CASE_UNHANDLED(OP_MULU)
        CASE_UNHANDLED(OP_MULS)
        CASE_UNHANDLED(OP_DIVU)
        CASE_UNHANDLED(OP_DIVS)
        CASE_UNHANDLED(OP_MODU)
        CASE_UNHANDLED(OP_MODS)
        CASE_UNHANDLED(OP_SHL)
        CASE_UNHANDLED(OP_LSR)
        CASE_UNHANDLED(OP_ASR)

        /* Logical */
        CASE_UNHANDLED(OP_AND)
        CASE_UNHANDLED(OP_OR)
        CASE_UNHANDLED(OP_XOR)
        CASE_UNHANDLED(OP_AND_BOOL)
        CASE_UNHANDLED(OP_BINARY_END /*= OP_OR_BOOL*/)

        /* Binary comparison */
        case OP_SET_EQ /*= OP_BINCMP*/:
            handle_insn_set_eq(insn, cl);
            break;

        CASE_UNHANDLED(OP_SET_NE)
        CASE_UNHANDLED(OP_SET_LE)
        CASE_UNHANDLED(OP_SET_GE)
        CASE_UNHANDLED(OP_SET_LT)
        CASE_UNHANDLED(OP_SET_GT)
        CASE_UNHANDLED(OP_SET_B)
        CASE_UNHANDLED(OP_SET_A)
        CASE_UNHANDLED(OP_SET_BE)
        CASE_UNHANDLED(OP_BINCMP_END /*= OP_SET_AE*/)

        /* Uni */
        CASE_UNHANDLED(OP_NOT)
        CASE_UNHANDLED(OP_NEG)

        /* Select - three input values */
        CASE_UNHANDLED(OP_SEL)

        /* Memory */
        CASE_UNHANDLED(OP_MALLOC)
        CASE_UNHANDLED(OP_FREE)
        CASE_UNHANDLED(OP_ALLOCA)
        case OP_LOAD:
            handle_insn_load(insn, cl);
            break;

        case OP_STORE:
            handle_insn_store(insn, cl);
            break;

        CASE_UNHANDLED(OP_SETVAL)
        CASE_UNHANDLED(OP_SYMADDR)
        CASE_UNHANDLED(OP_GET_ELEMENT_PTR)

        /* Other */
        case OP_PHI:
        case OP_PHISOURCE:
            // FIXME: this might be a SPARSE bug if DO_PER_EP_UNSAA is set
            fprintf(stderr, "%s", show_instruction(insn));
            break;

        case OP_CAST:
        case OP_SCAST:
        case OP_FPCAST:
        case OP_PTRCAST:
            // TODO: separate handler?
            handle_insn_copy(insn, cl);
            break;

        CASE_UNHANDLED(OP_INLINED_CALL)
        case OP_CALL:
            handle_insn_call(insn, cl);
            break;

        CASE_UNHANDLED(OP_VANEXT)
        CASE_UNHANDLED(OP_VAARG)
        CASE_UNHANDLED(OP_SLICE)
        case OP_SNOP:
            //handle_insn_store(insn);
            fprintf(stderr, "%s", show_instruction(insn));
            break;

        case OP_LNOP:
            //handle_insn_load(insn);
            fprintf(stderr, "%s", show_instruction(insn));
            break;

        CASE_UNHANDLED(OP_NOP)
        CASE_UNHANDLED(OP_DEATHNOTE)
        CASE_UNHANDLED(OP_ASM)

        /* Sparse tagging (line numbers, context, whatever) */
        CASE_UNHANDLED(OP_CONTEXT)
        CASE_UNHANDLED(OP_RANGE)

        /* Needed to translate SSA back to normal form */
        case OP_COPY:
            handle_insn_copy(insn, cl);
            break;
    }
}

static bool is_insn_interesting(struct instruction *insn)
{
    unsigned size = insn->size;
    if (size && KNOWN_PTR_SIZE != size) {
        WARN_VA("ignored instruction with operand of size %d", insn->size);
        return false;
    }

    switch (insn->opcode) {
        case OP_ENTRY:
            return false;

        default:
            return true;
    }
}

static void handle_bb_insn(struct instruction *insn,
                           struct cl_code_listener *cl)
{
    if (!insn)
        return;

    if (!insn->bb) {
#if SHOW_PSEUDO_INSNS
        WARN_VA("ignoring pseudo: %s", show_instruction(insn));
#endif
        return;
    }

    if (!is_insn_interesting(insn))
        return;

    handle_insn(insn, cl);
}

static void handle_bb(struct basic_block *bb, struct cl_code_listener *cl)
{
    struct instruction *insn;
    char *bb_name;

    if (!bb)
        return;

    if (asprintf(&bb_name, "%p", bb) < 0)
        die("asprintf failed");

    cl->bb_open(cl, bb_name);
    free(bb_name);

    FOR_EACH_PTR(bb->insns, insn) {
        handle_bb_insn(insn, cl);
    } END_FOR_EACH_PTR(insn);
}

static void handle_fnc_ep(struct entrypoint *ep, struct cl_code_listener *cl)
{
    struct instruction *entry = ep->entry;
    struct basic_block *bb;
    char *entry_name;

    if (!bb)
        return;

    // jump to entry basic block
    if (asprintf(&entry_name, "%p", entry->bb) < 0)
        die("asprintf failed");
    cl->insn_jmp(cl, entry->pos.line, entry_name);
    free(entry_name);

    // go through basic blocks
    FOR_EACH_PTR(ep->bbs, bb) {
        if (!bb)
            continue;

        if (bb->parents || bb->children || bb->insns || 1 < verbose)
            handle_bb(bb, cl);

    } END_FOR_EACH_PTR(bb);
}

static void handle_fnc_body(struct symbol *sym, struct cl_code_listener *cl)
{
    struct entrypoint *ep = linearize_symbol(sym);
    if (!ep)
        TRAP;

#if DO_PER_EP_UNSAA
    unssa(ep);
#endif

#if DO_PER_EP_SET_UP_STORAGE
    set_up_storage(ep);
#endif

    handle_fnc_ep(ep, cl);

#if DO_PER_EP_SET_UP_STORAGE
    free_storage();
#endif
}

static void handle_fnc_def(struct symbol *sym, struct cl_code_listener *cl)
{
    struct symbol *base_type = sym->ctype.base_type;
    struct symbol *arg;
    int argc = 0;

    cl->fnc_open(cl, sym->pos.line, show_ident(sym->ident),
            (sym->scope==file_scope)
            ? CL_SCOPE_GLOBAL
            : CL_SCOPE_STATIC);

    // dump argument list
    FOR_EACH_PTR(base_type->arguments, arg) {
        cl->fnc_arg_decl(cl, ++argc, show_ident(arg->ident));
    } END_FOR_EACH_PTR(arg);

    // handle fnc body
    handle_fnc_body(sym, cl);
    cl->fnc_close(cl);
}

static void handle_sym_fn(struct symbol *sym, struct cl_code_listener *cl)
{
    struct symbol *base_type = sym->ctype.base_type;
    struct statement *stmt = base_type->stmt;

    if (stmt) {
        // function definition
        handle_fnc_def(sym, cl);
        return;
    }

    WARN_UNHANDLED_SYM(sym);
}

static void handle_top_level_sym(struct symbol *sym,
                                 struct cl_code_listener *cl)
{
    struct symbol *base_type;

    if (!sym)
        return;

    base_type = sym->ctype.base_type;
    if (!base_type)
        return;

    switch (base_type->type) {
        CASE_UNHANDLED(SYM_UNINITIALIZED)
        CASE_UNHANDLED(SYM_PREPROCESSOR)
        CASE_UNHANDLED(SYM_BASETYPE)
        CASE_UNHANDLED(SYM_NODE)
        CASE_UNHANDLED(SYM_PTR)
        CASE_UNHANDLED(SYM_ARRAY)
        CASE_UNHANDLED(SYM_STRUCT)
        CASE_UNHANDLED(SYM_UNION)
        CASE_UNHANDLED(SYM_ENUM)
        CASE_UNHANDLED(SYM_TYPEDEF)
        CASE_UNHANDLED(SYM_TYPEOF)
        CASE_UNHANDLED(SYM_MEMBER)
        CASE_UNHANDLED(SYM_BITFIELD)
        CASE_UNHANDLED(SYM_LABEL)
        CASE_UNHANDLED(SYM_RESTRICT)
        CASE_UNHANDLED(SYM_FOULED)
        CASE_UNHANDLED(SYM_KEYWORD)
        CASE_UNHANDLED(SYM_BAD)

        case SYM_FN:
            handle_sym_fn(sym, cl);
            break;
    }

    if (sym->initializer)
        WARN_UNHANDLED("sym->initializer");
}

static void clean_up_symbols(struct symbol_list *list,
                             struct cl_code_listener *cl)
{
    struct symbol *sym;

    FOR_EACH_PTR(list, sym) {
#if DO_EXPAND_SYMBOL
        expand_symbol(sym);
#endif
        handle_top_level_sym(sym, cl);
    } END_FOR_EACH_PTR(sym);
}

int main(int argc, char **argv)
{
    char *file;
    struct string_list *filelist = NULL;
    struct cl_code_listener *cl;

#if 1
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
#endif

    cl_global_init_defaults(NULL, true);
    cl = cl_code_listener_create("pp", STDOUT_FILENO, false);
    if (!cl)
        // error message already emitted
        return EXIT_FAILURE;

    cl->file_open(cl, "<built-in>");
    clean_up_symbols(sparse_initialize(argc, argv, &filelist), cl);
    cl->file_close(cl);

    FOR_EACH_PTR_NOTAG(filelist, file) {
        fprintf(stderr, "%s: about to process '%s'...\n", argv[0], file);

        cl->file_open(cl, file);
        clean_up_symbols(sparse(file), cl);
        cl->file_close(cl);
    } END_FOR_EACH_PTR_NOTAG(file);

    cl->destroy(cl);
    cl_global_cleanup();

    return 0;
}
