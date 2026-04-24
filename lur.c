#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <dirent.h>

#define LUR_VERSION "lur 1.0"
#define LUR_VERSION_MAJ 1
#define LUR_VERSION_MIN 0
#define LUR_VERSION_REV 0

#define LUR_DEBUG_ASSERTS 1
#define LUR_DEBUG_PRINT_CODE 0
#define LUR_DEBUG_PRINT_DATA 0
#define LUR_DEBUG_PRINT_STACK 0
#define LUR_DEBUG_PRINT_STDLIB 1
#define LUR_DEBUG_PRINT_TOKENS 0
#define LUR_DEBUG_PRINT_ALLOCS 0
#define LUR_DEBUG_PRINT_MEM_STATS 0
#define LUR_DEBUG_DISABLE_GC 0

#define lur_printf printf

#define INIT_ARRAY_CAP 8
#define MAP_MAX_LOAD 0.75

#define MAX_CODE SIZE_MAX
#define MAX_DATA UINT16_MAX
#define MAX_LINES SIZE_MAX
#define MAX_CFRAMES UINT16_MAX
#define MAX_STACK UINT16_MAX
#define MAX_JUMP UINT16_MAX
#define MAX_ARGS UINT8_MAX
#define MAX_VREFS UINT8_MAX
#define MAX_LIST_LIT_ITEMS UINT16_MAX
#define MAX_MAP_LIT_ITEMS UINT16_MAX
#define MAX_ERR_MSG 2048

#define ERR_OUT_OF_MEMORY \
	"out of memory"
#define ERR_LIMIT(name, limit) \
	"%s limit reached (%d)", (name), (limit)
#define ERR_READ_FAILED(path) \
	"failed to read '%.*s'", (path)->len, (path)->buffer
#define ERR_WRITE_FAILED(path) \
	"failed to write to '%.*s'", (path)->len, (path)->buffer
#define ERR_OPEN_FAILED(path) \
	"failed to open '%.*s'", (path)->len, (path)->buffer
#define ERR_UNKNOWN_CHAR(c) \
	"unknown character '%c' (ASCII #%d)", (c), (c)
#define ERR_EXPECTED_EXPR(got) \
	"expected expression, got '%.*s'", \
		(got)->len, (got)->buffer
#define ERR_UNTERMINATED_TEXT \
	"unterminated text"
#define ERR_TYPECHECK(got, wanted) \
	"got type %s but expected %s", \
		type_name(got), type_name(wanted)
#define ERR_UNDEFINED(name, ctx) \
	"undefined variable: '%.*s'", \
		value_to_text(name, ctx)->len, \
		value_to_text(name, ctx)->buffer
#define ERR_INDEX(key) \
	"value at index '%.*s' not found", \
		value_to_text(key, ctx)->len, \
		value_to_text(key, ctx)->buffer
#define ERR_NOT_CALLABLE(value, ctx) \
	"value '%.* s' is not callable", \
		value_to_text(value, ctx)->len, \
		value_to_text(value, ctx)->buffer
#define ERR_ARGC(fname, got, wanted) \
	"%.*s got %d argument%s but expected %d", \
		(fname)->len, \
		(fname)->buffer, \
		(got), ((got) == 1) ? "" : "s", \
		(wanted)
#define ERR_INVALID_ESCAPE(ch) \
	"invalid escape sequence: '\\%c'", (ch)
#define ERR_DIV_BY_ZERO \
	"division by zero"
#define ERR_REMAINDER_OF_DIV_BY_ZERO \
	"remainder of division by zero"
#define ERR_NOT_HASHABLE(t) \
	"value of type %s is not hashable", type_name(t)
#define ERR_ASSERTION \
	"assertion failed"

#if LUR_DEBUG_ASSERTS
#define unreachable() \
	lur_printf("unreachable code entered on #%d", \
		__LINE__), \
	exit(EXIT_FAILURE)
#define assert(cond) \
	if (!(cond)) \
		lur_printf("assertion failed on #%d", __LINE__), \
		exit(EXIT_FAILURE)
#else
#define unreachable()
#define assert(cond) (void)(cond)
#endif

typedef enum {
	OP_NOP,
	OP_DATA,
	OP_NEWLIST,
	OP_NEWMAP,
	OP_NEWFREF,
	OP_NEWVREF,
	OP_POP,
	OP_DUP,
	OP_MOVE,
	OP_SWAP,
	OP_GETLOC,
	OP_SETLOC,
	OP_GETFIELD,
	OP_SETFIELD,
	OP_GETGLOB,
	OP_SETGLOB,
	OP_ADDGLOB,
	OP_GETVREF,
	OP_SETVREF,
	OP_EQ,
	OP_NE,
	OP_NOT,
	OP_LT,
	OP_LTE,
	OP_GT,
	OP_GTE,
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_POW,
	OP_DIV,
	OP_REM,
	OP_CON,
	OP_INV,
	OP_JMP,
	OP_IF,
	OP_IFNOT,
	OP_CALL,
	OP_RET,
	OP_HALT,
} opcode_t;

typedef struct {
	const char* name;
	int args;
} opinfo_t;

static opinfo_t OPINFO[] = {
	[OP_NOP] = {"nop", 0},
	[OP_DATA] = {"data", 2},
	[OP_NEWLIST] = {"newlist", 2},
	[OP_NEWMAP] = {"newmap", 2},
	[OP_NEWFREF] = {"newfref", 2},
	[OP_NEWVREF] = {"newvref", 0},
	[OP_POP] = {"pop", 0},
	[OP_DUP] = {"dup", 0},
	[OP_MOVE] = {"move", 2},
	[OP_SWAP] = {"swap", 0},
	[OP_GETLOC] = {"getloc", 2},
	[OP_SETLOC] = {"setloc", 2},
	[OP_GETFIELD] = {"getfield", 2},
	[OP_SETFIELD] = {"setfield", 2},
	[OP_GETGLOB] = {"getglob", 2},
	[OP_SETGLOB] = {"setglob", 2},
	[OP_ADDGLOB] = {"addglob", 2},
	[OP_GETVREF] = {"getvref", 2},
	[OP_SETVREF] = {"setvref", 2},
	[OP_EQ] = {"eq", 0},
	[OP_NE] = {"ne", 0},
	[OP_NOT] = {"not", 0},
	[OP_LT] = {"lt", 0},
	[OP_LTE] = {"lte", 0},
	[OP_GT] = {"gt", 0},
	[OP_GTE] = {"gte", 0},
	[OP_ADD] = {"add", 0},
	[OP_SUB] = {"sub", 0},
	[OP_MUL] = {"mul", 0},
	[OP_POW] = {"pow", 0},
	[OP_DIV] = {"div", 0},
	[OP_REM] = {"rem", 0},
	[OP_CON] = {"con", 0},
	[OP_INV] = {"inv", 0},
	[OP_JMP] = {"jmp", 2},
	[OP_IF] = {"if", 2},
	[OP_IFNOT] = {"ifnot", 2},
	[OP_CALL] = {"call", 1},
	[OP_RET] = {"ret", 0},
	[OP_HALT] = {"halt", 0},
};

typedef enum {
	TYPE_NULL,
	TYPE_BOOL,
	TYPE_NUMBER,
	TYPE_TEXT,
	TYPE_LIST,
	TYPE_MAP,
	TYPE_FUNC,
	TYPE_FREF,
	TYPE_VREF,
} type_t;

typedef struct obj_t obj_t;

typedef struct {
	uint8_t tag;
	union {
		bool q;
		double num;
		obj_t* obj;
	} data;
} value_t;

typedef struct obj_t {
	uint8_t tag;
	obj_t* next;
	bool marked;
} obj_t;

typedef struct {
	obj_t obj;
	uint8_t* buffer;
	size_t len;
} text_t;

typedef struct {
	obj_t obj;
	value_t* items;
	size_t len;
} list_t;

typedef struct {
	value_t key;
	value_t value;
} map_entry_t;

typedef struct {
	obj_t obj;
	map_entry_t* entries;
	size_t len;
	size_t cap;
} map_t;

typedef struct lur_t lur_t;
typedef value_t (*syscall_fn_t)(value_t*, lur_t*);

typedef struct {
	obj_t obj;
	uint8_t* ops;
	size_t nops;
	value_t* data;
	size_t ndata;
	int* lines;
	size_t nlines;
	size_t nvrefs;
	const text_t* name;
	const text_t* src;
	uint8_t argc;
	syscall_fn_t syscall;
} func_t;

typedef struct {
	obj_t obj;
	const func_t* func;
	struct vref_t** vrefs;
	size_t nvrefs;
} fref_t;

typedef struct vref_t {
	obj_t obj;
	size_t index;
	value_t closed;
	struct vref_t* next;
} vref_t;

#define make_null() \
	(value_t){TYPE_NULL, {.num = 0}}
#define make_bool(value) \
	(value_t){TYPE_BOOL, {.q = (value)}}
#define make_number(value) \
	(value_t){TYPE_NUMBER, {.num = (value)}}
#define make_text(value) \
	(value_t){TYPE_TEXT, {.obj = (obj_t*)(value)}}
#define make_list(value) \
	(value_t){TYPE_LIST, {.obj = (obj_t*)(value)}}
#define make_map(value) \
	(value_t){TYPE_MAP, {.obj = (obj_t*)(value)}}
#define make_function(value) \
	(value_t){TYPE_FUNC, {.obj = (obj_t*)(value)}}
#define make_fref(value) \
	(value_t){TYPE_FREF, {.obj = (obj_t*)(value)}}
#define make_vref(value) \
	(value_t){TYPE_VREF, {.obj = (obj_t*)(value)}}

#define get_bool(value) \
	((value).data.q)
#define get_number(value) \
	((value).data.num)
#define get_text(value) \
	((text_t*)((value).data.obj))
#define get_list(value) \
	((list_t*)((value).data.obj))
#define get_map(value) \
	((map_t*)((value).data.obj))
#define get_func(value) \
	((func_t*)((value).data.obj))
#define get_fref(value) \
	((fref_t*)((value).data.obj))
#define get_vref(value) \
	((vref_t*)((value).data.obj))

typedef struct {
	const func_t* func;
	const fref_t* fref;
	uint8_t* ip;
	size_t slots;
	bool returns;
} cframe_t;

typedef struct {
	cframe_t* calls;
	size_t ncalls;
	cframe_t* fp;
	
	value_t* stack;
	size_t max_stack;
	value_t* sp;
	
	vref_t* open_vrefs;
	map_t* globals;
	lur_t* ctx;
} vm_t;

typedef enum {
	T_NAME,
	T_NULL,
	T_TRUE,
	T_FALSE,
	T_NUMBER,
	T_NUMBER_HEX,
	T_NUMBER_BIN,
	T_TEXT,
	T_EQ,
	T_EQ_GT,
	T_BANG,
	T_BANG_EQ,
	T_LT,
	T_LT_EQ,
	T_LT_MINUS,
	T_GT,
	T_GT_EQ,
	T_PLUS,
	T_PLUS_EQ,
	T_MINUS,
	T_MINUS_EQ,
	T_MINUS_GT,
	T_STAR,
	T_STAR_EQ,
	T_CARET,
	T_CARET_EQ,
	T_SLASH,
	T_SLASH_EQ,
	T_PERCENT,
	T_PERCENT_EQ,
	T_LPAREN,
	T_RPAREN,
	T_LSQUARE,
	T_RSQUARE,
	T_LCURLY,
	T_RCURLY,
	T_AMPER,
	T_AMPER_EQ,
	T_PIPE,
	T_DOT,
	T_COMMA,
	T_COLON,
	T_SEMICOLON,
	T_LET,
	T_DO,
	T_END,
	T_FUN,
	T_RETURN,
	T_IF,
	T_THEN,
	T_ELSE,
	T_AND,
	T_OR,
	T_NOT,
	T_EOF,
} token_tag_t;

typedef struct {
	uint8_t tag;
	const text_t* lex;
	int line;
} token_t;

typedef struct ast_node_t ast_node_t;

typedef enum {
	AST_VALUE,
	AST_LIST,
	AST_MAP,
	AST_BIND,
	AST_LOAD,
	AST_STORE,
	AST_UNARY,
	AST_BINARY,
	AST_BLOCK,
	AST_FUN,
	AST_CALL,
	AST_RETURN,
	AST_DOT,
	AST_ARROW,
	AST_BRANCH,
} ast_tag_t;

typedef struct {
	value_t data;
} ast_value_t;

typedef struct {
	ast_node_t** items;
	size_t len;
} ast_list_t;

typedef struct {
	ast_node_t** keys;
	ast_node_t** values;
	size_t len;
} ast_map_t;

typedef struct {
	const text_t* name;
	ast_node_t* value;
} ast_bind_t;

typedef struct {
	const text_t* name;
} ast_load_t;

typedef struct {
	token_tag_t tag;
	const text_t* name;
	ast_node_t* value;
} ast_store_t;

typedef struct {
	ast_node_t* rhs;
	uint8_t opcode;
} ast_unary_t;

typedef struct {
	ast_node_t* lhs;
	ast_node_t* rhs;
	int opcode;
} ast_binary_t;

typedef struct {
	ast_node_t** items;
	size_t nitems;
} ast_block_t;

typedef struct {
	const text_t* name;
	bool is_lambda;
	const text_t** params;
	uint8_t nparams;
	ast_node_t* body;
} ast_fun_t;

typedef struct {
	ast_node_t* func;
	uint8_t argc;
	ast_node_t** args;
} ast_call_t;

typedef struct {
	ast_node_t* value;
} ast_return_t;

typedef struct {
	const text_t* name;
	ast_node_t* lhs;
	ast_node_t* value;
	bool is_assignment;
	token_tag_t tag;
} ast_dot_t;

typedef struct {
	ast_node_t* instance;
	const text_t* method_name;
	uint8_t argc;
	ast_node_t** args;
} ast_arrow_t;

typedef struct {
	ast_node_t* cond;
	ast_node_t* a;
	ast_node_t* b;
} ast_branch_t;

typedef struct ast_node_t {
	uint8_t tag;
	union {
		ast_value_t value;
		ast_list_t list;
		ast_map_t map;
		ast_bind_t bind;
		ast_load_t load;
		ast_store_t store;
		ast_unary_t unary;
		ast_binary_t binary;
		ast_block_t block;
		ast_fun_t fun;
		ast_call_t call;
		ast_return_t ret;
		ast_dot_t dot;
		ast_arrow_t arrow;
		ast_branch_t branch;
	} data;
	int line;
} ast_node_t;

typedef struct {
	const char* pos;
	int line;
	lur_t* ctx;
} scanner_t;

typedef enum {
	PREC_NONE,
	PREC_ASSIGN,
	PREC_OR,
	PREC_AND,
	PREC_EQ,
	PREC_CMP,
	PREC_TERM,
	PREC_FACTOR,
	PREC_UNARY,
	PREC_POW,
	PREC_DOT,
	PREC_CALL,
} precedence_t;

typedef struct parser_t parser_t;

typedef ast_node_t* (*parse_fn_t)(parser_t*);

typedef struct {
	parse_fn_t prefix;
	parse_fn_t infix;
	precedence_t prec;
} parse_rule_t;

static ast_node_t* ps_name(parser_t*);
static ast_node_t* ps_null(parser_t*);
static ast_node_t* ps_boolean(parser_t*);
static ast_node_t* ps_number(parser_t*);
static ast_node_t* ps_text(parser_t*);
static ast_node_t* ps_list(parser_t*);
static ast_node_t* ps_map(parser_t*);
static ast_node_t* ps_bind(parser_t*);
static ast_node_t* ps_unary(parser_t*);
static ast_node_t* ps_binary(parser_t*);
static ast_node_t* ps_grouping(parser_t*);
static ast_node_t* ps_block(parser_t*);
static ast_node_t* ps_lambda(parser_t*);
static ast_node_t* ps_call(parser_t*);
static ast_node_t* ps_return(parser_t*);
static ast_node_t* ps_branch(parser_t*);
static ast_node_t* ps_dot(parser_t*);
static ast_node_t* ps_arrow(parser_t*);

static parse_rule_t RULES[] = {
	[T_NAME] = 
		{ps_name, NULL, PREC_NONE},
	[T_NULL] =
		{ps_null, NULL, PREC_NONE},
	[T_TRUE] =
		{ps_boolean, NULL, PREC_NONE},
	[T_FALSE] =
		{ps_boolean, NULL, PREC_NONE},
	[T_NUMBER] = 
		{ps_number, NULL, PREC_NONE},
	[T_NUMBER_HEX] = 
		{ps_number, NULL, PREC_NONE},
	[T_NUMBER_BIN] = 
		{ps_number, NULL, PREC_NONE},
	[T_TEXT] =
		{ps_text, NULL, PREC_NONE},
	[T_EQ] =
		{NULL, ps_binary, PREC_EQ},
	[T_EQ_GT] =
		{NULL, NULL, PREC_NONE},
	[T_BANG] =
		{NULL, NULL, PREC_NONE},
	[T_BANG_EQ] =
		{NULL, ps_binary, PREC_EQ},
	[T_LT] =
		{NULL, ps_binary, PREC_CMP},
	[T_LT_EQ] =
		{NULL, ps_binary, PREC_CMP},
	[T_LT_MINUS] =
		{NULL, NULL, PREC_ASSIGN},
	[T_GT] =
		{NULL, ps_binary, PREC_CMP},
	[T_GT_EQ] =
		{NULL, ps_binary, PREC_CMP},
	[T_PLUS] =
		{NULL, ps_binary, PREC_TERM},
	[T_PLUS_EQ] =
		{NULL, NULL, PREC_NONE},
	[T_MINUS] =
		{ps_unary, ps_binary, PREC_TERM},
	[T_MINUS_EQ] =
		{NULL, NULL, PREC_NONE},
	[T_MINUS_GT] =
		{NULL, ps_arrow, PREC_DOT},
	[T_STAR] =
		{NULL, ps_binary, PREC_FACTOR},
	[T_STAR_EQ] =
		{NULL, NULL, PREC_NONE},
	[T_CARET] =
		{NULL, ps_binary, PREC_POW},
	[T_CARET_EQ] =
		{NULL, NULL, PREC_NONE},
	[T_SLASH] =
		{NULL, ps_binary, PREC_FACTOR},
	[T_SLASH_EQ] =
		{NULL, NULL, PREC_NONE},
	[T_PERCENT] =
		{NULL, ps_binary, PREC_FACTOR},
	[T_PERCENT_EQ] =
		{NULL, NULL, PREC_NONE},
	[T_LPAREN] =
		{ps_grouping, NULL, PREC_NONE},
	[T_RPAREN] =
		{NULL, NULL, PREC_NONE},
	[T_LSQUARE] =
		{ps_list, NULL, PREC_NONE},
	[T_RSQUARE] =
		{NULL, NULL, PREC_NONE},
	[T_LCURLY] =
		{ps_map, NULL, PREC_NONE},
	[T_RCURLY] =
		{NULL, NULL, PREC_NONE},
	[T_AMPER] =
		{NULL, ps_binary, PREC_TERM},
	[T_AMPER_EQ] =
		{NULL, NULL, PREC_NONE},
	[T_PIPE] =
		{NULL, NULL, PREC_NONE},
	[T_DOT] =
		{NULL, ps_dot, PREC_DOT},
	[T_COMMA] =
		{NULL, NULL, PREC_NONE},
	[T_COLON] =
		{NULL, ps_call, PREC_CALL},
	[T_SEMICOLON] =
		{NULL, NULL, PREC_NONE},
	[T_LET] =
		{ps_bind, NULL, PREC_NONE},
	[T_DO] =
		{ps_block, NULL, PREC_NONE},
	[T_END] =
		{NULL, NULL, PREC_NONE},
	[T_FUN] =
		{ps_lambda, NULL, PREC_NONE},
	[T_RETURN] =
		{ps_return, NULL, PREC_NONE},
	[T_IF] =
		{ps_branch, NULL, PREC_NONE},
	[T_THEN] =
		{NULL, NULL, PREC_NONE},
	[T_ELSE] =
		{NULL, NULL, PREC_NONE},
	[T_AND] =
		{NULL, ps_binary, PREC_AND},
	[T_OR] =
		{NULL, ps_binary, PREC_OR},
	[T_NOT] =
		{ps_unary, NULL, PREC_NONE},
	[T_EOF] =
		{NULL, NULL, PREC_NONE},
};

typedef struct parser_t {
	scanner_t scanner;
	token_t cur;
	token_t prev;
	ast_node_t* prefix;
	lur_t* ctx;
} parser_t;

typedef struct {
	const text_t* name;
	int depth;
	bool is_captured;
} sym_t;

typedef struct {
	size_t index;
	bool is_local;
} cl_vref_t;

typedef struct comp_t {
	parser_t parser;
	func_t* func;
	int depth;
	sym_t* syms;
	size_t nsyms;
	cl_vref_t vrefs[MAX_VREFS];
	struct comp_t* parent;
	ast_node_t* cur;
	lur_t* ctx;
} comp_t;

typedef struct {
	ptrdiff_t bytes;
	size_t total;
	obj_t* objs;
	int gc_pause;
	obj_t** marked;
	size_t nmarked;
	size_t gc_cleaned;
	size_t gc_cycles;
} mem_t;

typedef struct lur_t {
	comp_t cl;
	vm_t vm;
	mem_t mem;
	bool running;
	bool interpreter;
	jmp_buf errjmp;
	map_t* std_map;
	const char* std_map_name;
} lur_t;

static unsigned nextpow2(unsigned n) {
	if (n > 0 && n < INIT_ARRAY_CAP)
		return INIT_ARRAY_CAP;
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	return n + 1;
}

static void text_print(const text_t*);
static void print_stack_trace(const lur_t*);

static void error(lur_t* ctx, const char* msg, ...) {
	assert(ctx && msg);
	
	char buffer[MAX_ERR_MSG];
	va_list args;
	va_start(args, msg);
	vsnprintf(buffer, MAX_ERR_MSG, msg, args);
	va_end(args);
	
	const func_t* func;
	if (ctx->running) func = ctx->vm.fp->func;
	else func = ctx->cl.func;
	
	int line;
	if (ctx->running) line = func->lines[
		ctx->vm.fp->ip - func->ops];
	else line = ctx->cl.parser.prev.line;
	
	lur_printf("[");
	text_print(func->name);
	lur_printf(":%d] ", line);
	lur_printf("error: %s\n", buffer);
	
	if (ctx->running) {
		print_stack_trace(ctx);
		ctx->running = false;
		ctx->vm.ncalls--;
		ctx->vm.fp = &ctx->vm.calls[ctx->vm.ncalls - 1];
	}
	
	longjmp(ctx->errjmp, 1);
}

static void print_stack_trace(const lur_t* ctx) {
	const cframe_t* cur = ctx->vm.fp;
	const cframe_t* end = ctx->vm.calls;
	if (cur == end)
		return;
	
	lur_printf("[ stack trace: ]\n");
	int index = 0;
	do {
		lur_printf("%d: ", index);
		text_print(cur->func->name);
		lur_printf("\n");
		cur--;
		index++;
	} while (cur > end);
}

static void gc_collect(lur_t*);
static void gc_pause(lur_t*);
static void gc_resume(lur_t*);

static void* mem_resize(
	lur_t* ctx, void* p, size_t os, size_t ns,
	int64_t line, const char* func)
{
	if (os == ns) return p;
	
	#if LUR_DEBUG_PRINT_ALLOCS
	if (ctx && p != ctx)
		lur_printf("[mem: %+d - %s:%llu - %p]\n",
			ns - os, func, line, p);
	#endif
		
	if (ctx) {
		ctx->mem.bytes += ns - os;
		ctx->mem.total += ns;
		if (ns > os)
			gc_collect(ctx);
	}
	
	if (ns == 0) {
		free(p);
		return NULL;
	}
	
	void* np = realloc(p, ns);
	if (!np)
		error(ctx, ERR_OUT_OF_MEMORY);
	return np;
}

#define mem_alloc(ctx, size) \
	mem_resize(ctx, NULL, 0, (size), __LINE__, __func__)
#define mem_free(ctx, p, size) \
	mem_resize(ctx, (p), (size), 0, __LINE__, __func__)
#define arr_alloc(ctx, p, t, on, nn) \
	((p) = mem_resize( \
		ctx, p, sizeof(t) * (on), sizeof(t) * (nn), \
			__LINE__, __func__))
#define arr_free(ctx, p, t, n) \
	((p) = mem_resize(ctx, p, sizeof(t) * (n), 0, \
		__LINE__, __func__))

static obj_t* obj_new(size_t size, type_t tag, lur_t* ctx) {
	assert(size > 0 && ctx);
	obj_t* obj = mem_alloc(ctx, size);
	obj->tag = tag;
	obj->next = ctx->mem.objs;
	ctx->mem.objs = obj;
	return obj;
}

static void text_free(text_t*, lur_t*);
static void list_free(list_t*, lur_t*);
static void map_free(map_t*, lur_t*);
static void func_free(func_t*, lur_t*);
static void fref_free(fref_t*, lur_t*);
static void vref_free(vref_t*, lur_t*);

static void obj_free(obj_t* obj, lur_t* ctx) {
	assert(obj && ctx);
	switch (obj->tag) {
	case TYPE_TEXT: text_free((text_t*)obj, ctx); break;
	case TYPE_LIST: list_free((list_t*)obj, ctx); break;
	case TYPE_MAP: map_free((map_t*)obj, ctx); break;
	case TYPE_FUNC: func_free((func_t*)obj, ctx); break;
	case TYPE_FREF: fref_free((fref_t*)obj, ctx); break;
	case TYPE_VREF: vref_free((vref_t*)obj, ctx); break;
	default: unreachable();
	}
}

static text_t* text_new(
	const uint8_t* buffer, size_t len, lur_t* ctx)
{
	assert(ctx);
	text_t* text = (text_t*)obj_new(
		sizeof(text_t), TYPE_TEXT, ctx);
	
	text->buffer = NULL;
	arr_alloc(ctx, text->buffer, uint8_t, 0, len + 1);

	if (buffer)
		snprintf((char*)text->buffer, len + 1, "%s", buffer);
	
	text->buffer[len] = '\0';
	text->len = len;
	return text;
}

#define text_lit(chars, ctx) \
	text_new((const uint8_t*)(chars), strlen(chars), ctx)
	
#define text_copy(text, ctx) \
	text_new((text)->buffer, (text)->len, ctx)
	
static void text_free(text_t* text, lur_t* ctx) {
	assert(text && ctx);
	arr_free(ctx, text->buffer, uint8_t, text->len + 1);
	mem_free(ctx, text, sizeof(text_t));
}

static bool text_eq(const text_t* a, const text_t* b) {
	assert(a && b);
	if (a == b) return true;
	if (a->len != b->len) return false;
	return strncmp(
		(const char*)a->buffer,
		(const char*)b->buffer,
		a->len) == 0;
}

static text_t* text_cmp(const text_t* a, const text_t* b) {
	assert(a && b);
	for (int i = 0; i < (a->len < b->len) ? a->len : b->len; i++) {
		uint8_t ac = a->buffer[i];
		uint8_t bc = b->buffer[i];
		if (ac < bc) return (text_t*)a;
		if (ac > bc) return (text_t*) b;
	}
	
	return (a->len < b->len) ? (text_t*)a : (text_t*)b;
}

static text_t* text_concat(
	const text_t* a, const text_t* b, lur_t* ctx)
{
	assert(b && ctx);
	if (!a || a->len == 0) return text_copy(b, ctx);
	
	text_t* result = text_new(NULL, a->len + b->len, ctx);
	snprintf((char*)result->buffer,
		a->len + 1, "%s", a->buffer);
	snprintf((char*)result->buffer + a->len,
		b->len + 1, "%s", b->buffer);
	return result;
}

static void text_push(text_t* text, uint8_t ch, lur_t* ctx) {
	assert(text && ctx);
	arr_alloc(ctx, text->buffer, uint8_t,
		text->len + 1, text->len + 2);
	text->buffer[text->len++] = ch;
	text->buffer[text->len] = '\0';
}

static text_t* text_fmt(lur_t* ctx, const char* msg, ...) {
	assert(ctx && msg);
	
	va_list args;
	va_start(args, msg);
	size_t len = vsnprintf(NULL, 0, msg, args);
	text_t* text = text_new(NULL, len, ctx);
	vsnprintf((char*)text->buffer, len + 1, msg, args);
	va_end(args);
	return text;
}

static text_t* text_escape(const text_t* input, lur_t* ctx)
{
	assert(input && ctx);
	gc_pause(ctx);
	text_t* output = text_new(NULL, 0, ctx);
	for (size_t i = 0; i < input->len; i++) {
		if (input->buffer[i] != '\\') {
			text_push(output, input->buffer[i], ctx);
			continue;
		}
		
		i++;
			
		uint8_t ch = '\0';
		switch (input->buffer[i]) {
		case '\\': ch = '\\'; break;
		case '"': ch = '"'; break;
		case 't': ch = '\t'; break;
		case 'n': ch = '\n'; break;
		case 'r': ch = '\r'; break;
		case '0': ch = '\0'; break;
		case 'x': {
			i++;
			uint8_t* start = input->buffer + i;
			uint8_t* end = start + 2;
			ch = strtoul((const char*)start, (char**)&end, 16);
			i++;
			break;
		}
		default: error(ctx, ERR_INVALID_ESCAPE(
			input->buffer[i]));
		}
			
		text_push(output, ch, ctx);
	}
	
	gc_resume(ctx);
	return output;
}

static text_t* text_reverse(const text_t* text, lur_t* ctx) {
	assert(text && ctx);
	gc_pause(ctx);
	text_t* rev = text_new(NULL, 0, ctx);
	if (text->len == 0) return rev;
	for (int64_t i = text->len - 1; i >= 0; i--)
		text_push(rev, text->buffer[i], ctx);
	gc_resume(ctx);
	return rev;
}

#define min2(a, b) (a < b) ? (a) : (b)
#define min3(a, b, c) min2(a, min2(b, c))

static int32_t text_edit_distance(
	const text_t* a, const text_t* b)
{
	assert(a && b);
	if (strcmp((const char*)a->buffer,
		(const char*)b->buffer) == 0) return 0;
	if (a->len == 0) return b->len;
	if (b->len == 0) return a->len;
    
	int* v0 = malloc((b->len + 1) * sizeof(int));
	int* v1 = malloc((b->len + 1) * sizeof(int));
    
    for (int i = 0; i < b->len + 1; i++)
        v0[i] = i;

    for (int i = 0; i <  a->len; i++) {
        v1[0] = i + 1;

    	for (int j = 0; j <  b->len; j++) {
			int cost = (a->buffer[i] == b->buffer[j]) ? 0 : 1;
			v1[j + 1] = min3(
				v1[j] + 1, v0[j + 1] + 1, v0[j] + cost);
        }
        
		for (int j = 0; j <  b->len + 1; j++)
        	v0[j] = v1[j];
    }
    
	int distance = v1[b->len];
	free(v0);
	free(v1);
	return distance;
}

#undef min2
#undef min3

static void text_print(const text_t* text) {
	assert(text);
	lur_printf("%.*s", text->len, text->buffer);
}

static list_t* list_new(lur_t* ctx) {
	assert(ctx);
	list_t* list = (list_t*)obj_new(
		sizeof(list_t), TYPE_LIST, ctx);
	list->items = NULL;
	list->len = 0;
	return list;
}

static void list_push(list_t*, value_t, lur_t*);

static list_t* list_copy(const list_t* src, lur_t* ctx) {
	assert(src && ctx);
	gc_pause(ctx);
	
	list_t* dst = list_new(ctx);
	for (size_t i = 0; i < src->len; i++)
		list_push(dst, src->items[i], ctx);
		
	gc_resume(ctx);
	return dst;
}

static void list_free(list_t* list, lur_t* ctx) {
	assert(list && ctx);
	arr_free(ctx, list->items, value_t, nextpow2(list->len));
	mem_free(ctx, list, sizeof(list));
}

static text_t* value_to_text(value_t, lur_t*);

static size_t list_convert_index(
	const list_t* list, double index, lur_t* ctx)
{
	if (index < 0)
		index = list->len + index;
	if (index >= list->len)
		error(ctx, ERR_INDEX(make_number(index)));
	return index;
}

static bool value_eq(value_t, value_t);

static bool list_eq(const list_t* a, const list_t* b) {
	assert(a && b);
	if (a == b) return true;
	if (a->len != b->len) return false;
	for (size_t i = 0; i < a->len; i++)
		if (!value_eq(a->items[i], b->items[i]))
			return false;
	return true;
}

static void list_push(list_t* list, value_t value, lur_t* ctx) {
	assert(list && ctx);
	arr_alloc(ctx, list->items, value_t,
		nextpow2(list->len), nextpow2(list->len + 1));
	list->items[list->len++] = value;
}

static value_t list_pop(list_t* list, lur_t* ctx) {
	assert(list && ctx);
	if (list->len == 0)
		error(ctx, ERR_INDEX(make_number(0)));
	
	value_t value = list->items[list->len - 1];
	arr_alloc(ctx, list->items, value_t,
		nextpow2(list->len), nextpow2(--list->len));
	return value;
}

static void list_insert(
	list_t* list, size_t index, value_t value, lur_t* ctx)
{
	assert(list && ctx);
	if (list->len == 0) {
		list_push(list, value, ctx);
		return;
	}
	
	arr_alloc(ctx, list->items, value_t,
		nextpow2(list->len), nextpow2(list->len++));
	for (int64_t i = list->len - 1; i > index; i--)
		list->items[i] = list->items[i - 1];
	
	list->items[index] = value;
}

static void list_del(
	list_t* list, size_t index, lur_t* ctx)
{
	assert(list && ctx);
	if (list->len == 0) return;
	for (int64_t i = index; i < list->len - 1; i++)
		list->items[i] = list->items[i + 1];
	
	arr_alloc(ctx, list->items, value_t,
		nextpow2(list->len), nextpow2(--list->len));
}

static list_t* list_concat(
	const list_t* a, const list_t* b, lur_t* ctx)
{
	assert(a && b && ctx);
	gc_pause(ctx);
	list_t* result = list_new(ctx);
	for (size_t i = 0; i < a->len; i++)
		list_push(result, a->items[i], ctx);
	for (size_t i = 0; i < b->len; i++)
		list_push(result, b->items[i], ctx);
	gc_resume(ctx);
	return result;
}

static list_t* list_repeat(
	const list_t* target, size_t times, lur_t* ctx)
{
	assert(target && target > 0 && ctx);
	gc_pause(ctx);
	
	list_t* list = list_copy(target, ctx);
	for (size_t i = 0; i < times - 1; i++)
		list = list_concat(list, target, ctx);
		
	gc_resume(ctx);
	return list;
}

static void list_swap(list_t* list, size_t a, size_t b) {
	assert(list && a < list->len && b < list->len);
	value_t temp = list->items[a];
	list->items[a] = list->items[b];
	list->items[b] = temp;
}

static bool list_contains(list_t* list, value_t item) {
	assert(list);
	for (size_t i = 0; i < list->len; i++) {
		if (value_eq(list->items[i], item))
			return true;
	}
	return false;
}

static const char* type_name(type_t type);
static value_t value_math(value_t, value_t, int, lur_t*);

static void list_sort_impl(
	list_t* list, size_t n, lur_t* ctx)
{
	assert(list && ctx);
	size_t swapped = 0;
	for (size_t i = 0; i < n - 1; i++) {
		value_t a = list->items[i];
		value_t b = list->items[i + 1];
		
		bool swap = false;
		if (a.tag == TYPE_NUMBER) {
			if (b.tag != TYPE_NUMBER) \
				error(ctx, ERR_TYPECHECK(
					b.tag, TYPE_NUMBER));
			
			swap = get_bool(value_math(a, b, OP_GT, ctx));
		} else if (a.tag == TYPE_TEXT) {
			if (b.tag != TYPE_TEXT) \
				error(ctx, ERR_TYPECHECK(b.tag, TYPE_TEXT));
			
			swap = text_cmp(get_text(a), get_text(b)) ==
				get_text(b);
		}
		
		if (swap) {
			list_swap(list, i, i + 1);
			swapped++;
		}
	}
	
	if (swapped != 0) list_sort_impl(list, n - 1, ctx);
}

static list_t* list_sort(const list_t* input, lur_t* ctx) {
	assert(input && ctx);
	list_t* sorted = list_copy(input, ctx);
	if (sorted->len <= 1) return sorted;
	list_sort_impl(sorted, sorted->len, ctx);
	return sorted;
}

static list_t* list_reverse(const list_t* list, lur_t* ctx) {
	assert(list);
	gc_pause(ctx);
	list_t* rev = list_new(ctx);
	if (list->len == 0) return rev;
	for (int64_t i = list->len - 1; i >= 0; i--)
		list_push(rev, list->items[i], ctx);
	gc_resume(ctx);
	return rev;
}

static text_t* value_to_text(value_t, lur_t*);

static text_t* list_join(const list_t* list, lur_t* ctx) {
	assert(list && ctx);
	gc_pause(ctx);
	text_t* text = text_new(NULL, 0, ctx);
	for (size_t i = 0; i < list->len; i++)
		text = text_concat(text, value_to_text(
			list->items[i], ctx), ctx);
	gc_resume(ctx);
	return text;
}

static list_t* list_flatten(list_t* input, lur_t* ctx) {
	assert(input && ctx);
	list_t* output = list_new(ctx);
	for (size_t i = 0; i < input->len; i++) {
		value_t item = input->items[i];
		if (item.tag == TYPE_LIST)
			output = list_concat(output,
				list_flatten(get_list(item), ctx), ctx);
		else
			list_push(output, item, ctx);
	}
	return output;
}

static map_t* map_new(lur_t* ctx) {
	assert(ctx);
	map_t* map = (map_t*)obj_new(
		sizeof(map_t), TYPE_MAP, ctx);
	map->entries = NULL;
	map->len = 0;
	map->cap = 0;
	return map;
}

static void map_free(map_t* map, lur_t* ctx) {
	assert(map && ctx);
	arr_free(ctx, map->entries, map_entry_t, map->cap);
	mem_free(ctx, map, sizeof(map));
}

static bool map_eq(const map_t* a, const map_t* b) {
	assert(a && b);
	if (a == b) return true;
	if (a->len != b->len) return false;
	if (a->cap != b->cap) return false;
	for (size_t i = 0; i < a->cap; i++) {
		const map_entry_t* a_entry = &a->entries[i];
		const map_entry_t* b_entry = &b->entries[i];
		if (a_entry->key.tag == TYPE_NULL) continue;
		if (b_entry->key.tag == TYPE_NULL) continue;
		
		if (!value_eq(a_entry->key, b_entry->key))
			return false;
			
		if (!value_eq(a_entry->value, b_entry->value))
			return false;
	}
	return true;
}

static uint64_t value_hash(value_t, lur_t*);

static map_entry_t* map_find_entry(
	map_entry_t* entries, size_t cap, value_t key,
	lur_t* ctx)
{
	assert(entries && ctx);
	uint64_t hash = value_hash(key, ctx);
	uint64_t index = hash & (cap - 1);
	for (;;) {
		const map_entry_t* entry = &entries[index];
		if (entry->key.tag == TYPE_NULL ||
			value_eq(entry->key, key))
			return (map_entry_t*)entry;
		index = (index + 1) & (cap - 1);
	}
}

static void map_set_cap(
	map_t* map, size_t cap, lur_t* ctx)
{
	assert(map && ctx);
	map_entry_t* entries = mem_alloc(
		ctx, sizeof(map_entry_t) * cap);
	
	for (size_t i = 0; i < cap; i++) {
		entries[i].key = make_null();
		entries[i].value = make_null();
	}
	
	for (size_t i = 0; i < map->cap; i++) {
		const map_entry_t* src = &map->entries[i];
		if (src->key.tag == TYPE_NULL) continue;
		
		map_entry_t* dest = map_find_entry(
			entries, cap, src->key, ctx);
		dest->key = src->key;
		dest->value = src->value;
	}
	
	arr_free(ctx, map->entries, map_entry_t, map->cap);
	map->entries = entries;
	map->cap = cap;
}

static bool map_set(
	map_t* map, value_t key, value_t value, lur_t* ctx)
{
	assert(map && ctx);
	if (map->len + 1 > map->cap * MAP_MAX_LOAD) {
		size_t cap = nextpow2(map->cap + 1);
		map_set_cap(map, cap, ctx);
	}
	
	map_entry_t* entry = map_find_entry(
		map->entries, map->cap, key, ctx);
	bool is_new = entry->key.tag == TYPE_NULL;
	if (is_new) map->len++;
	
	entry->key = key;
	entry->value = value;
	return is_new;
}

bool map_get(
	const map_t* map,
	value_t key,
	value_t* value,
	lur_t* ctx)
{
	assert(map && ctx);
	if (map->len == 0) return false;
	
	map_entry_t* entry = map_find_entry(
		map->entries, map->cap, key, ctx);
	if (entry->key.tag == TYPE_NULL) return false;
	
	if (value)
		*value = entry->value;
	return true;
}

static void map_extend(
	map_t* dst, const map_t* src, lur_t* ctx)
{
	assert(src && dst && ctx);
	for (size_t i = 0; i < src->cap; i++) {
		const map_entry_t* entry = &src->entries[i];
		if (entry->key.tag == TYPE_NULL) continue;
		map_set(dst, entry->key, entry->value, ctx);
	}
}

static map_t* map_reverse(
	const map_t* src, lur_t* ctx)
{
	assert(src && ctx);
	gc_pause(ctx);
	
	map_t* dst = map_new(ctx);
	for (size_t i = 0; i < src->cap; i++) {
		const map_entry_t* entry = &src->entries[i];
		if (entry->key.tag == TYPE_NULL) continue;
		map_set(dst, entry->value, entry->key, ctx);
	}
	
	gc_resume(ctx);
	return dst;
}

static func_t* func_new(lur_t* ctx) {
	assert(ctx);
	func_t* func = (func_t*)obj_new(
		sizeof(func_t), TYPE_FUNC, ctx);
	func->ops = NULL;
	func->nops = 0;
	func->data = NULL;
	func->ndata = 0;
	func->lines = NULL;
	func->nlines = 0;
	func->nvrefs = 0;
	func->name = NULL;
	func->src = NULL;
	func->argc = 0;
	func->syscall = NULL;
	return func;
}

static void func_free(func_t* func, lur_t* ctx) {
	assert(func && ctx);
	arr_free(ctx, func->ops, uint8_t,
		nextpow2(func->nops));
	arr_free(ctx, func->data, value_t,
		nextpow2(func->ndata));
	arr_free(ctx, func->lines, int,
		nextpow2(func->nlines));
	mem_free(ctx, func, sizeof(func_t));
}

static void func_write(
	func_t* func, uint8_t byte, int line, lur_t* ctx)
{
	assert(func && ctx);
	if (func->nops == MAX_CODE)
		error(ctx, ERR_LIMIT("opcodes", MAX_CODE));
	
	arr_alloc(ctx, func->ops, uint8_t,
		nextpow2(func->nops),
		nextpow2(func->nops + 1));
	func->ops[func->nops++] = byte;
	
	if (func->nops == MAX_LINES)
		error(ctx, ERR_LIMIT("lines", MAX_LINES));
	
	arr_alloc(ctx, func->lines, int,
		nextpow2(func->nlines),
		nextpow2(func->nlines + 1));
	func->lines[func->nlines++] = line;
}

static size_t func_write_value(
	func_t* func, value_t value, lur_t* ctx)
{
	assert(func && ctx);
	for (size_t i = 0; i < func->ndata; i++)
		if (value_eq(func->data[i], value))
			return i;
	
	if (func->ndata == MAX_DATA)
		error(ctx, ERR_LIMIT("data", MAX_DATA));
	
	arr_alloc(ctx, func->data, value_t,
		nextpow2(func->ndata),
		nextpow2(func->ndata + 1));
	func->data[func->ndata++] = value;
	return func->ndata - 1;
}

static fref_t* fref_new(const func_t* func, lur_t* ctx) {
	assert(func && ctx);
	fref_t* fref = (fref_t*)obj_new(
		sizeof(fref_t), TYPE_FREF, ctx);
	fref->func = func;
	fref->vrefs = NULL;
	arr_alloc(ctx, fref->vrefs, vref_t*, 0, func->nvrefs);
	for (size_t i = 0; i < func->nvrefs; i++)
		fref->vrefs[i] = NULL;
	fref->nvrefs = func->nvrefs;
	return fref;
}

static void fref_free(fref_t* fref, lur_t* ctx) {
	assert(fref && ctx);
	arr_free(ctx, fref->vrefs, vref_t*, fref->nvrefs);
	mem_free(ctx, fref, sizeof(fref_t));
}

static vref_t* vref_new(int index, lur_t* ctx) {
	assert(ctx);
	vref_t* vref = (vref_t*)obj_new(
		sizeof(vref_t), TYPE_VREF, ctx);
	vref->index = index;
	vref->closed = make_null();
	vref->next = NULL;
	return vref;
}

static void vref_free(vref_t* vref, lur_t* ctx) {
	assert(vref && ctx);
	mem_free(ctx, vref, sizeof(vref_t));
}

static const char* type_name(type_t type) {
	switch (type) {
	case TYPE_NULL: return "Null";
	case TYPE_BOOL: return "Bool";
	case TYPE_NUMBER: return "Number";
	case TYPE_TEXT: return "Text";
	case TYPE_LIST: return "List";
	case TYPE_MAP: return "Map";
	case TYPE_FUNC: return "Function";
	case TYPE_FREF: return "Function";
	case TYPE_VREF: return "Vref";
	default: unreachable();
	}
	
	return NULL;
}

static bool type_is_obj(type_t type) {
	switch (type) {
	case TYPE_NULL:
	case TYPE_BOOL:
	case TYPE_NUMBER: return false;
	case TYPE_TEXT:
	case TYPE_LIST:
	case TYPE_MAP:
	case TYPE_FUNC:
	case TYPE_FREF:
	case TYPE_VREF: return true;
	default: unreachable();
	}
	
	return false;
}

static text_t* value_to_text_ex(
	value_t value, bool quote_text, lur_t* ctx)
{
	assert(ctx);
	text_t* result = NULL;
	gc_pause(ctx);
	
	switch (value.tag) {
	case TYPE_NULL:
		result = text_lit("null", ctx);
		break;
	case TYPE_BOOL:
		result = text_lit(
			(get_bool(value)) ? "true" : "false", ctx);
		break;
	case TYPE_NUMBER:
		result = text_fmt(ctx, "%.12g", get_number(value));
		break;
	case TYPE_TEXT: {
		if (quote_text) {
			result = text_lit("\"", ctx);
			result = text_concat(result, get_text(value), ctx);
			result = text_concat(result, text_lit("\"", ctx), ctx);
		} else result = get_text(value);
		break;
	}
	case TYPE_LIST: {
		const list_t* list = get_list(value);
		result = text_lit("[", ctx);
		size_t printed = 0;
		for (size_t i = 0; i < list->len; i++) {
			if (value_eq(list->items[i], value)) {
				printed++;
				continue;
			}
			
			result = text_concat(result, value_to_text_ex(
				list->items[i], true, ctx), ctx);
			
			if (printed < list->len - 1)
				result = text_concat(result, text_lit(", ", ctx), ctx);
			printed++;
		}
		result = text_concat(result, text_lit("]", ctx), ctx);
		break;
	}
	case TYPE_MAP: {
		const map_t* map = get_map(value);
		result = text_lit("{", ctx);
		size_t printed = 0;
		for (size_t i = 0; i < map->cap; i++) {
			const map_entry_t* entry = &map->entries[i];
			if (entry->key.tag == TYPE_NULL) continue;
			if (value_eq(value, entry->value)) {
				printed++;
				continue;
			}
			
			result = text_concat(result, value_to_text_ex(
				entry->key, true, ctx), ctx);
			result = text_concat(result, text_lit(" => ", ctx), ctx);
			result = text_concat(result, value_to_text_ex(
				entry->value, true, ctx), ctx);
			
			if (printed < map->len - 1)
				result = text_concat(result, text_lit(", ", ctx), ctx);
			printed++;
		}
		result = text_concat(result, text_lit("}", ctx), ctx);
		break;
	}
	case TYPE_FUNC: {
		const func_t* func = get_func(value);
		result = text_fmt(ctx, "%.*s",
			func->name->len, func->name->buffer);
		break;
	}
	case TYPE_FREF: {
		const fref_t* fref = get_fref(value);
		result = text_fmt(ctx, "%.*s",
			fref->func->name->len,
			fref->func->name->buffer);
		break;
	}
	case TYPE_VREF: {
		result = text_lit("<vref>", ctx);
		break;
	}
	default: unreachable();
	}
	
	gc_resume(ctx);
	return result;
}

static text_t* value_to_text(value_t value, lur_t* ctx) {
	assert(ctx);
	return value_to_text_ex(value, false, ctx);
}

static void value_print_ex(
	value_t value, bool quote_text, lur_t* ctx)
{
	assert(ctx);
	text_print(value_to_text_ex(value, quote_text, ctx));
}

static void value_print(value_t value, lur_t* ctx) {
	assert(ctx);
	text_print(value_to_text(value, ctx));
}

static bool value_eq(value_t a, value_t b) {
	if (a.tag != b.tag) return false;
	switch (a.tag) {
	case TYPE_NULL: return true;
	case TYPE_BOOL: return get_bool(a) == get_bool(b);
	case TYPE_NUMBER:
		return get_number(a) == get_number(b);
	case TYPE_TEXT:
		return text_eq(get_text(a), get_text(b));
	case TYPE_LIST: return list_eq(get_list(a), get_list(b));
	case TYPE_MAP:
		return map_eq(get_map(a), get_map(b));
	case TYPE_FUNC: return get_func(a) == get_func(b);
	case TYPE_FREF: return get_fref(a) == get_fref(b);
	case TYPE_VREF: return get_vref(a) == get_vref(b);
	default: unreachable();
	}
	return false;
}

#define typecheck(value, t) \
	if ((value).tag != (t)) \
		error(ctx, ERR_TYPECHECK((value).tag, (t)))
		
static value_t vector_math(
	value_t a, value_t b, int op, lur_t* ctx)
{
	gc_pause(ctx);
			
	if (b.tag == TYPE_LIST) {
		const list_t* v1 = get_list(a);
		const list_t* v2 = get_list(b);
		list_t* out = list_new(ctx);
		size_t shortest = (v1->len < v2->len) ?
			v1->len : v2->len;
				
		for (size_t i = 0; i < shortest; i++) {
			typecheck(v1->items[i], TYPE_NUMBER);
			typecheck(v2->items[i], TYPE_NUMBER);
					
			list_push(out, value_math(
				v1->items[i], v2->items[i], op, ctx),
				ctx);
		}
				
		gc_resume(ctx);
		return make_list(out);
	}
			
	typecheck(b, TYPE_NUMBER);
	const list_t* list = get_list(a);
	list_t* out = list_new(ctx);
			
	for (size_t i = 0; i < list->len; i++) {
		typecheck(list->items[i], TYPE_NUMBER);
		list_push(out, value_math(list->items[i], b, op, ctx),
			ctx);
	}
			
	gc_resume(ctx);
	return make_list(out);
}

static value_t value_math(
	value_t a, value_t b, int op, lur_t* ctx)
{
	assert(ctx);
	switch (op) {
	case OP_EQ: {
		return make_bool(value_eq(a, b));
	}
	case OP_NE: {
		return make_bool(!value_eq(a, b));
	}
	case OP_NOT: {
		typecheck(a, TYPE_BOOL);
		return make_bool(!get_bool(a));
	}
	case OP_LT: {
		typecheck(a, TYPE_NUMBER);
		typecheck(b, TYPE_NUMBER);
		return make_bool(get_number(a) < get_number(b));
	}
	case OP_LTE: {
		typecheck(a, TYPE_NUMBER);
		typecheck(b, TYPE_NUMBER);
		return make_bool(get_number(a) <= get_number(b));
	}
	case OP_GT: {
		typecheck(a, TYPE_NUMBER);
		typecheck(b, TYPE_NUMBER);
		return make_bool(get_number(a) > get_number(b));
	}
	case OP_GTE: {
		typecheck(a, TYPE_NUMBER);
		typecheck(b, TYPE_NUMBER);
		return make_bool(get_number(a) >= get_number(b));
	}
	case OP_ADD: {
		if (a.tag == TYPE_LIST || b.tag == TYPE_LIST)
			return vector_math(a, b, op, ctx);
		
		typecheck(a, TYPE_NUMBER);
		typecheck(b, TYPE_NUMBER);
		return make_number(
			get_number(a) + get_number(b));
	}
	case OP_SUB: {
		if (a.tag == TYPE_LIST || b.tag == TYPE_LIST)
			return vector_math(a, b, op, ctx);
		
		typecheck(a, TYPE_NUMBER);
		typecheck(b, TYPE_NUMBER);
		return make_number(
			get_number(a) - get_number(b));
	}
	case OP_MUL: {
		if (a.tag == TYPE_LIST || b.tag == TYPE_LIST)
			return vector_math(a, b, op, ctx);
		
		typecheck(a, TYPE_NUMBER);
		typecheck(b, TYPE_NUMBER);
		return make_number(
			get_number(a) * get_number(b));
	}
	case OP_POW: {
		if (a.tag == TYPE_LIST || b.tag == TYPE_LIST)
			return vector_math(a, b, op, ctx);
		
		typecheck(a, TYPE_NUMBER);
		typecheck(b, TYPE_NUMBER);
		return make_number(
			powf(get_number(a), get_number(b)));
	}
	case OP_DIV: {
		if (a.tag == TYPE_LIST || b.tag == TYPE_LIST)
			return vector_math(a, b, op, ctx);
		
		typecheck(a, TYPE_NUMBER);
		typecheck(b, TYPE_NUMBER);
		if (get_number(b) == 0.0)
			error(ctx, ERR_DIV_BY_ZERO);
		return make_number(
			get_number(a) / get_number(b));
	}
	case OP_REM: {
		if (a.tag == TYPE_LIST || b.tag == TYPE_LIST)
			return vector_math(a, b, op, ctx);
		
		typecheck(a, TYPE_NUMBER);
		typecheck(b, TYPE_NUMBER);
		if (get_number(b) == 0.0)
			error(ctx, ERR_REMAINDER_OF_DIV_BY_ZERO);
		double result = fmod(
			get_number(a), get_number(b));
		if (result < 0.0)
			result += fabs(get_number(b));
		return make_number(result);
	}
	case OP_CON: {
		if (a.tag == TYPE_LIST) {
			gc_pause(ctx);
			list_t* list = get_list(a);
			if (b.tag == TYPE_LIST)
				list = list_concat(list, get_list(b), ctx);
			else
				list_push(list, b, ctx);
			gc_resume(ctx);
			return make_list(list);
		} 
			
		if (b.tag == TYPE_LIST) {
			gc_pause(ctx);
			list_t* list = get_list(b);
			list_insert(list, 0, a, ctx);
			gc_resume(ctx);
			return make_list(list);
		}
		
		if (a.tag == TYPE_MAP && b.tag == TYPE_MAP) {
			gc_pause(ctx);
			map_t* map = map_new(ctx);
			map_extend(map, get_map(a), ctx);
			map_extend(map, get_map(b), ctx);
			gc_resume(ctx);
			return make_map(map);
		}
			
		gc_pause(ctx);
		text_t* text = text_concat(
			get_text(a),
			value_to_text(b, ctx),
			ctx);
		gc_resume(ctx);
		return make_text(text);
	}
	case OP_INV: {
		if (a.tag == TYPE_TEXT)
			return make_text(text_reverse(get_text(a), ctx));
		if (a.tag == TYPE_LIST)
			return make_list(list_reverse(get_list(a), ctx));
		if (a.tag == TYPE_MAP)
			return make_map(map_reverse(get_map(a), ctx));
		typecheck(a, TYPE_NUMBER);
		return make_number(-get_number(a));
	}
	default: unreachable();
	}
	
	return make_null();
}

#undef typecheck

static uint64_t value_hash(value_t value, lur_t* ctx) {
	assert(ctx);
	switch (value.tag) {
	case TYPE_BOOL: return get_bool(value);
	case TYPE_NUMBER: return get_number(value);
	case TYPE_TEXT: {
		const text_t* text = get_text(value);
		uint64_t hash = 2166136261u;
		for (int i = 0; i < text->len; i++) {
			hash ^= text->buffer[i];
    		hash *= 16777619;
		}
		return hash;
	}
	case TYPE_FUNC: {
		const func_t* func = get_func(value);
		return value_hash(make_text(func->name), ctx);
	}
	case TYPE_FREF: {
		const fref_t* fref = get_fref(value);
		return value_hash(make_function(fref->func), ctx);
	}
	default: error(ctx, ERR_NOT_HASHABLE(value.tag));
	}
	
	return 0;
}

static void gc_mark_obj(obj_t* obj, lur_t* ctx) {
	assert(ctx);
	if (!obj || obj->marked) return;
	obj->marked = true;
	ctx->mem.marked = realloc(ctx->mem.marked,
		sizeof(obj_t*) * ctx->mem.nmarked + 1);
	if (!ctx->mem.marked)
		error(ctx, ERR_OUT_OF_MEMORY);
	ctx->mem.marked[ctx->mem.nmarked++] = obj;
}

static void gc_mark_value(value_t value, lur_t* ctx) {
	assert(ctx);
	if (type_is_obj(value.tag))
		gc_mark_obj(value.data.obj, ctx);
}

static void gc_deep_mark_obj(obj_t* obj, lur_t* ctx) {
	assert(obj && ctx);
	switch (obj->tag) {
		case TYPE_LIST: {
			list_t* list = (list_t*)obj;
			for (size_t i = 0; i < list->len; i++)
				gc_mark_value(list->items[i], ctx);
			break;
		}
		case TYPE_MAP: {
			map_t* map = (map_t*)obj;
			for (size_t i = 0; i < map->cap; i++) {
				map_entry_t* entry = &map->entries[i];
				if (entry->key.tag == TYPE_NULL) continue;
				gc_mark_value(entry->key, ctx);
				gc_mark_value(entry->value, ctx);
			}
			break;
		}
		case TYPE_FUNC: {
			func_t* func = (func_t*)obj;
			gc_mark_obj((obj_t*)func->name, ctx);
			for (size_t i = 0; i < func->ndata; i++)
				gc_mark_value(func->data[i], ctx);
			break;
		}
		case TYPE_FREF: {
			fref_t* fref = (fref_t*)obj;
			gc_mark_obj((obj_t*)fref->func, ctx);
			for (size_t i = 0; i < fref->nvrefs; i++)
				gc_mark_obj((obj_t*)fref->vrefs[i], ctx);
			break;
		}
		case TYPE_VREF: {
			vref_t* vref = (vref_t*)obj;
			gc_mark_value(vref->closed, ctx);
			break;
		}
		default: break;
	}
}

static void gc_mark(lur_t*);
static void gc_trace_refs(lur_t*);
static void gc_sweep(lur_t*);

static void gc_collect(lur_t* ctx) {
	assert(ctx);
	
	#if LUR_DEBUG_DISABLE_GC
	return;
	#endif
	
	if (!ctx->running) return;
	if (ctx->mem.gc_pause > 0) return;
	
	gc_mark(ctx);
	gc_trace_refs(ctx);
	gc_sweep(ctx);
	ctx->mem.gc_cycles++;
}

static void gc_mark(lur_t* ctx) {
	assert(ctx);
	
	vm_t* vm = &ctx->vm;
	for (cframe_t* frame = vm->calls; 
		frame <= vm->fp;
		frame++)
		gc_mark_obj((obj_t*)frame->fref, ctx);
	
	for (value_t* v = vm->stack; v < vm->sp; v++)
		gc_mark_value(*v, ctx);
		
	for (vref_t* vref = vm->open_vrefs;
		vref != NULL;
		vref = vref->next)
		gc_mark_obj((obj_t*)vref, ctx);
	
	gc_mark_obj((obj_t*)vm->globals, ctx);
	for (size_t i = 0; i < vm->globals->cap; i++) {
		map_entry_t* entry = &vm->globals->entries[i];
		gc_mark_value(entry->key, ctx);
		gc_mark_value(entry->value, ctx);
	}
}

static void gc_trace_refs(lur_t* ctx) {
	assert(ctx);
	
	while (ctx->mem.nmarked > 0) {
		obj_t* object = ctx->mem.marked[
			--ctx->mem.nmarked];
		gc_deep_mark_obj(object, ctx);
	}
}

static void gc_sweep(lur_t* ctx) {
	assert(ctx);
	
	obj_t* prev = NULL;
	obj_t* obj = ctx->mem.objs;
	
	while (obj) {
		if (obj->marked) {
			obj->marked = false;
			prev = obj;
			obj = obj->next;
			continue;
		}
		
		obj_t* unreached = obj;
		obj = obj->next;
		if (prev) prev->next = obj;
		else ctx->mem.objs = obj;
		obj_free(unreached, ctx);
		
		ctx->mem.gc_cleaned++;
	}
}

static void gc_pause(lur_t* ctx) {
	assert(ctx);
	gc_collect(ctx);
	ctx->mem.gc_pause++;
}

static void gc_resume(lur_t* ctx) {
	assert(ctx && ctx->mem.gc_pause >= 1);
	ctx->mem.gc_pause--;
}

static void vm_init(vm_t* vm, lur_t* ctx) {
	assert(ctx);
	gc_pause(ctx);
	
	vm->calls = NULL;
	arr_alloc(ctx, vm->calls, cframe_t, 0, MAX_CFRAMES);
	vm->ncalls = 0;
	vm->fp = NULL;
	
	vm->stack = NULL;
	arr_alloc(ctx, vm->stack, value_t, 0, MAX_STACK);
	vm->max_stack = MAX_STACK;
	vm->sp = vm->stack;
	
	vm->open_vrefs = NULL;
	vm->globals = map_new(ctx);
	vm->ctx = ctx;
	gc_resume(ctx);
}

static void vm_free(vm_t* vm, lur_t* ctx) {
	assert(vm && ctx);
	arr_free(ctx, vm->calls, cframe_t, MAX_CFRAMES);
	arr_free(ctx, vm->stack, value_t, MAX_STACK);
}

#if LUR_DEBUG_PRINT_DATA
static void dbg_print_data(const vm_t* vm) {
	assert(vm);
	const func_t* func = vm->fp->func;
	
	lur_printf("data:\n");
	for (size_t i = 0; i < func->ndata; i++) {
		lur_printf("  %02x: %s = ",
			i, type_name(func->data[i].tag));
		value_print_ex(func->data[i], true, vm->ctx);
		lur_printf("\n");
	}
	lur_printf("\n");
}
#endif

#if LUR_DEBUG_PRINT_CODE
static void dbg_print_header(const vm_t* vm) {
	assert(vm);
	
	lur_printf("\n[%d] ", vm->fp - vm->calls);
	text_print(vm->fp->func->name);
	lur_printf("\n");
	
	lur_printf("addr  line         name bytes      comment |\n");
	lur_printf("-------------------------------------------|\n");
}

static void dbg_print_opcode(const vm_t* vm) {
	assert(vm);
	
	size_t addr =
		vm->fp->ip - vm->fp->func->ops;
	int line = vm->fp->func->lines[addr];
	int prev_line = line;
	if (addr > 0)
		prev_line = vm->fp->func->lines[addr - 1];
	uint8_t opcode = *vm->fp->ip;
	
	if (prev_line != line || addr == 0)
		lur_printf("%04d %5d %12s %02x",
			addr, line, OPINFO[opcode].name, opcode);
	else
		lur_printf("%04d     | %12s %02x",
			addr, OPINFO[opcode].name, opcode);
	
	for (int i = 0; i < OPINFO[opcode].args; i++)
		lur_printf(" %02x", vm->fp->ip[i + 1]);
		
	for (int i = 0; i < 3 - OPINFO[opcode].args; i++)
		lur_printf("   ");
	
	uint16_t u16arg = vm->fp->ip[1] << 8 | vm->fp->ip[2];
	switch (opcode) {
	case OP_DATA: {
		value_t value = vm->fp->func->data[u16arg];
		value_print_ex(value, true, vm->ctx);
		break;
	}
	case OP_NEWFREF: {
		const func_t* func = get_func(
			vm->fp->func->data[u16arg]);
		text_print(func->name);
		break;
	}
	case OP_SWAP: {
		value_t a = vm->sp[-2];
		value_t b = vm->sp[-1];
		
		lur_printf("[");
		value_print(a, vm->ctx);
		lur_printf(", ");
		value_print(b, vm->ctx);
		lur_printf("] -> [");
		value_print(b, vm->ctx);
		lur_printf(", ");
		value_print(a, vm->ctx);
		lur_printf("]");
		break;
	}
	case OP_GETLOC: {
		value_t value = (vm->stack + vm->fp->slots)[
			u16arg];
		value_print_ex(value, true, vm->ctx);
		break;
	}
	case OP_GETFIELD: {
		value_t name = vm->fp->func->data[u16arg];
		value_print(name, vm->ctx);
		break;
	}
	case OP_SETFIELD: {
		value_t name = vm->fp->func->data[u16arg];
		value_print(name, vm->ctx);
		break;
	}
	case OP_GETGLOB: {
		value_t key = vm->fp->func->data[u16arg];
		value_print_ex(key, true, vm->ctx);
		lur_printf(": ");
		value_t value;
		map_get(vm->globals, key, &value, vm->ctx);
		value_print_ex(value, true, vm->ctx);
		break;
	}
	case OP_SETGLOB: {
		value_t value = vm->fp->func->data[u16arg];
		value_print_ex(value, true, vm->ctx);
		lur_printf(": ");
		value_print_ex(vm->sp[-1], true, vm->ctx);
		break;
	}
	case OP_ADDGLOB: {
		value_t value = vm->fp->func->data[u16arg];
		value_print_ex(value, false, vm->ctx);
		lur_printf(": ");
		value_t key = vm->fp->func->data[u16arg];
		map_get(vm->globals, key, &value, vm->ctx);
		value_print_ex(value, true, vm->ctx);
		break;
	}
	case OP_GETVREF: {
		vref_t* vref = vm->fp->fref->vrefs[u16arg];
		if (vref->index == -1) {
			lur_printf("(closed) ");
			value_print(vref->closed, vm->ctx);
		} else {
			lur_printf("(open) ");
			value_print(vm->stack[vref->index], vm->ctx);
		}
		break;
	}
	case OP_SETVREF: {
		vref_t* vref = vm->fp->fref->vrefs[u16arg];
		if (vref->index == -1) {
			lur_printf("(closed) ");
			value_print(vm->sp[-1], vm->ctx);
		} else {
			lur_printf("(open) ");
			value_print(vm->sp[-1], vm->ctx);
		}
		break;
	}
	case OP_CALL: {
		uint8_t argc = vm->fp->ip[1];
		value_t value = vm->sp[-argc - 1];
		
		assert(value.tag == TYPE_FREF);
		const fref_t* fref = get_fref(vm->sp[-argc - 1]);
		text_print(fref->func->name);
		lur_printf(" (%d arg%c)",
			fref->func->argc,
			(fref->func->argc == 1) ? '\0' : 's');
		break;
	}
	case OP_RET: {
		lur_printf("-> ");
		value_print_ex(vm->sp[-1], true, vm->ctx);
		break;
	}
	}
	
	#if !LUR_DEBUG_PRINT_STACK
	lur_printf("\n");
	#endif
}
#endif

#if LUR_DEBUG_PRINT_STACK
static void dbg_print_stack(const vm_t* vm) {
	assert(vm);
	
	value_t* cur = vm->stack + vm->fp->slots;
	value_t* end = vm->sp;
	
	lur_printf("\n\t [");
	while (cur != end) {
		value_print_ex(*cur, true, vm->ctx);
		cur++;
		if (cur != end)
			lur_printf(", ");
	}
	lur_printf("]\n");
}
#endif

static vref_t* vm_capture_vref(vm_t* vm, int index) {
	assert(vm);
	
	vref_t* prev = NULL;
	vref_t* cur = vm->open_vrefs;
	
	while (cur && cur->index > index) {
		prev = cur;
		cur = cur->next;
	}
	
	if (cur && cur->index == index)
		return cur;
	
	vref_t* fresh = vref_new(index, vm->ctx);
	fresh->next = cur;
	
	if (prev) prev->next = fresh;
	else vm->open_vrefs = fresh;
	return fresh;
}

static void vm_close_vrefs(vm_t* vm, int last) {
	assert(vm);
	
	while (vm->open_vrefs &&
		vm->open_vrefs->index >= last)
	{
		vref_t* vref = vm->open_vrefs;
		vref->closed = vm->stack[vref->index];
		vref->index = -1;
		vm->open_vrefs = vref->next;
	}
}

#define read_u8() (*vm->fp->ip++)
#define read_u16() (uint16_t)(read_u8() << 8 | read_u8())
#define push(value) (*vm->sp++ = (value))
#define pop() (*(--vm->sp))
#define get(offset) (vm->sp[-(offset) - 1])
#define set(offset, value) (vm->sp[-(offset) - 1] = (value))
#define stack_base (vm->stack + vm->fp->slots)
#define typecheck(offset, t) \
	if (get(offset).tag != (t)) \
		error(vm->ctx, \
			ERR_TYPECHECK(get(offset).tag, (t)))

static void vm_call(vm_t*, value_t, uint8_t, bool);

static value_t vm_launch(
	vm_t* vm, value_t callable, size_t argc, bool returns)
{
	assert(vm);
	vm_call(vm, callable, argc, returns);

	for (;;) {
		#if LUR_DEBUG_PRINT_CODE
		dbg_print_opcode(vm);
		#endif
		
		switch (read_u8()) {
		case OP_NOP: break;
		case OP_DATA: {
			push(vm->fp->func->data[read_u16()]);
			break;
		}
		case OP_NEWLIST: {
			gc_pause(vm->ctx);
			uint16_t len = read_u16();
			list_t* list = list_new(vm->ctx);
			for (size_t i = 0; i < len; i++)
				list_push(list, get(len - 1 - i), vm->ctx);
			vm->sp -= len;
			push(make_list(list));
			gc_resume(vm->ctx);
			break;
		}
		case OP_NEWMAP: {
			gc_pause(vm->ctx);
			uint16_t len = read_u16();
			map_t* map = map_new(vm->ctx);
			for (size_t i = 0; i < len * 2; i += 2) {
				value_t key = get(i + 1);
				value_t value = get(i);
				map_set(map, key, value, vm->ctx);
			}
			vm->sp -= len * 2;
			push(make_map(map));
			gc_resume(vm->ctx);
			break;
		}
		case OP_NEWFREF: {
			func_t* func = get_func(vm->fp->func->data[
				read_u16()]);
			fref_t* fref = fref_new(func, vm->ctx);
			push(make_fref(fref));
			for (size_t i = 0; i < fref->nvrefs; i++) {
				uint8_t index = read_u8();
				uint8_t is_local = read_u8();
				if (is_local)
					fref->vrefs[i] = vm_capture_vref(
						vm, vm->fp->slots + index);
				else
					fref->vrefs[i] = vm->fp->fref->vrefs[index];
			}
			break;
		}
		case OP_NEWVREF: {
			vm_close_vrefs(vm, vm->sp - vm->stack - 1);
			pop();
			break;
		}
		case OP_POP: {
			pop();
			break;
		}
		case OP_DUP: {
			push(get(0));
			break;
		}
		case OP_MOVE: {
			uint16_t pos = read_u16();
			value_t value = get(0);
			
			value_t* cur = vm->sp - 1;
			value_t* end = stack_base + pos;
			while (cur != end) {
				*cur = cur[-1];
				cur--;
			}
			
			stack_base[pos] = value;
			break;
		}
		case OP_SWAP: {
			value_t temp = get(0);
			set(0, get(1));
			set(1, temp);
			break;
		}
		case OP_GETLOC: {
			uint16_t slot = read_u16();
			push(stack_base[slot]);
			break;
		}
		case OP_SETLOC: {
			uint16_t slot = read_u16();
			stack_base[slot] = get(0);
			break;
		}
		case OP_GETFIELD: {
			typecheck(0, TYPE_MAP);
			map_t* map = get_map(get(0));
			value_t key = vm->fp->func->data[read_u16()];
			value_t value;
			if (!map_get(map, key, &value, vm->ctx))
				error(vm->ctx, ERR_UNDEFINED(key, vm->ctx));
			pop();
			push(value);
			break;
		}
		case OP_SETFIELD: {
			typecheck(1, TYPE_MAP);
			map_t* map = get_map(get(1));
			value_t key = vm->fp->func->data[read_u16()];
			map_set(map, key, get(0), vm->ctx);
			value_t backup = pop();
			pop();
			push(backup);
			break;
		}
		case OP_GETGLOB: {
			value_t key = vm->fp->func->data[read_u16()];
			value_t value;
			if (!map_get(vm->globals, key, &value, vm->ctx))
				error(vm->ctx, ERR_UNDEFINED(key, vm->ctx));
			push(value);
			break;
		}
		case OP_SETGLOB: {
			value_t key = vm->fp->func->data[read_u16()];
			if (!map_get(vm->globals, key, NULL, vm->ctx))
				error(vm->ctx, ERR_UNDEFINED(key, vm->ctx));
			map_set(vm->globals, key, get(0), vm->ctx);
			break;
		}
		case OP_ADDGLOB: {
			value_t key = vm->fp->func->data[read_u16()];
			map_set(vm->globals, key, get(0), vm->ctx);
			pop();
			break;
		}
		case OP_GETVREF: {
			uint16_t slot = read_u16();
			vref_t* vref = vm->fp->fref->vrefs[slot];
			if (vref->index == -1) push(vref->closed);
			else push(vm->stack[vref->index]);
			break;
		}
		case OP_SETVREF: {
			uint16_t slot = read_u16();
			vref_t* vref = vm->fp->fref->vrefs[slot];
			if (vref->index == -1) vref->closed = get(0);
			else vm->stack[vref->index] = get(0);
			break;
		}
		case OP_EQ:
		case OP_NE: {
			value_t b = pop();
			value_t a = pop();
			push(value_math(a, b, vm->fp->ip[-1], vm->ctx));
			break;
		}
		case OP_NOT: {
			value_t value = pop();
			push(value_math(value, make_null(), OP_NOT,
				vm->ctx));
			break;
		}
		case OP_LT:
		case OP_LTE:
		case OP_GT:
		case OP_GTE:
		case OP_ADD:
		case OP_SUB:
		case OP_MUL:
		case OP_POW:
		case OP_DIV:
		case OP_REM:
		case OP_CON: {
			value_t b = pop();
			value_t a = pop();
			push(value_math(a, b, vm->fp->ip[-1], vm->ctx));
			break;
		}
		case OP_INV: {
			value_t value = pop();
			push(value_math(value, make_null(), OP_INV,
				vm->ctx));
			break;
		}
		case OP_JMP: {
			vm->fp->ip += read_u16();
			break;
		}
		case OP_IF: {
			uint16_t offset = read_u16();
			if (get_bool(get(0))) vm->fp->ip += offset;
			break;
		}
		case OP_IFNOT: {
			uint16_t offset = read_u16();
			if (!get_bool(get(0))) vm->fp->ip += offset;
			break;
		}
		case OP_CALL: {
			uint8_t argc = read_u8();
			vm_call(vm, get(argc), argc, false);
			break;
		}
		case OP_RET: {
			value_t ret = pop();
			vm_close_vrefs(vm, vm->fp->slots);
			vm->sp = vm->stack + vm->fp->slots;
			push(ret);
			
			bool returns = vm->fp->returns;
			vm->ncalls--;
			vm->fp = &vm->calls[vm->ncalls - 1];
			
			#if LUR_DEBUG_PRINT_CODE
			dbg_print_header(vm);
			#if LUR_DEBUG_PRINT_DATA
			dbg_print_data(vm);
			#endif
			#endif
			
			if (returns) return make_null();
			break;
		}
		case OP_HALT: {
			vm->ncalls--;
			vm->fp = &vm->calls[vm->ncalls - 1];
			return get(0);
		}
		default: unreachable();
		}
		
		#if LUR_DEBUG_PRINT_STACK
		dbg_print_stack(vm);
		#endif
	}
}

static void vm_call(
	vm_t* vm, value_t value, uint8_t argc, bool returns)
{
	assert(vm);
	if (value.tag != TYPE_FREF)
		error(vm->ctx,
			ERR_NOT_CALLABLE(value, vm->ctx));
	
	const fref_t* fref = get_fref(value);
	if (argc != fref->func->argc)
		error(vm->ctx, ERR_ARGC(
			fref->func->name, argc, fref->func->argc));
	
	if (fref->func->syscall) {
		gc_pause(vm->ctx);
		value_t result = fref->func->syscall(
			vm->sp - argc, vm->ctx);
		gc_resume(vm->ctx);
			
		vm->sp -= argc + 1;
		push(result);
		return;
	}
	
	if (vm->ncalls == MAX_CFRAMES)
		error(vm->ctx, ERR_LIMIT(
			"call stack", MAX_CFRAMES));
	
	cframe_t call;
	call.func = fref->func;
	call.fref = fref;
	call.ip = fref->func->ops;
	call.slots = (vm->sp - vm->stack) - argc - 1;
	call.returns = returns;
	
	vm->calls[vm->ncalls++] = call;
	vm->fp = &vm->calls[vm->ncalls - 1];
	
	#if LUR_DEBUG_PRINT_CODE
	dbg_print_header(vm);
	#if LUR_DEBUG_PRINT_DATA
	dbg_print_data(vm);
	#endif
	#endif
}

#undef read_u8
#undef read_u16
#undef push
#undef pop
#undef get
#undef set
#undef stack_base
#undef typecheck
	
static void sc_init(
	scanner_t* sc, const text_t* src, lur_t* ctx)
{
	assert(sc && src && ctx);
	sc->pos = (char*)src->buffer;
	sc->line = 1;
	sc->ctx = ctx;
}

static token_t sc_next(scanner_t* sc) {
	assert(sc);
	
	#define make(t) (token_t){ \
		(t), \
		text_new((const uint8_t*)start, \
			sc->pos - start, sc->ctx), \
		sc->line}
	#define check(c) \
		((*sc->pos == (c)) ? sc->pos++, true : false)
	
	while (isspace((unsigned char)*sc->pos) ||
		(*sc->pos == '-' && sc->pos[1] == '-')) {
		if (*sc->pos == '-' && sc->pos[1] == '-') {
			while (*sc->pos != '\n' && *sc->pos != '\0')
				sc->pos++;
		}
		
		if (*sc->pos == '\n')
			sc->line++;
		sc->pos++;
	}
		
	const char* start = sc->pos;
	
	if (isalpha(*sc->pos) || *sc->pos == '_') {
		sc->pos++;
		while ((isalpha(*sc->pos)
			|| isdigit(*sc->pos)
			|| *sc->pos == '_') 
			&& *sc->pos != '\0')
			sc->pos++;
		
		size_t len = sc->pos - start;
		#define keyword(kw) \
			(strlen(kw) == len && \
				strncmp(start, kw, len) == 0)
		
		if (keyword("null")) return make(T_NULL);
		if (keyword("true")) return make(T_TRUE);
		if (keyword("false")) return make(T_FALSE);
		if (keyword("let")) return make(T_LET);
		if (keyword("do")) return make(T_DO);
		if (keyword("end")) return make(T_END);
		if (keyword("fun")) return make(T_FUN);
		if (keyword("return")) return make(T_RETURN);
		if (keyword("if")) return make(T_IF);
		if (keyword("then")) return make(T_THEN);
		if (keyword("else")) return make(T_ELSE);
		if (keyword("and")) return make(T_AND);
		if (keyword("or")) return make(T_OR);
		if (keyword("not")) return make(T_NOT);
		
		#undef keyword
		
		return make(T_NAME);
	}
	
	if (isdigit(*sc->pos)) {
		sc->pos++;
		bool is_hex = (*sc->pos == 'x');
		bool is_bin = (*sc->pos == 'b');
		while ((isdigit(*sc->pos) ||
			*sc->pos == '.' ||
			((is_hex || is_bin) && isalpha(*sc->pos))) &&
			*sc->pos != '\0')
		{
			sc->pos++;
		}
		
		return make((is_hex) ? T_NUMBER_HEX :
			(is_bin) ? T_NUMBER_BIN : T_NUMBER);
	}
	
	if (*sc->pos == '"') {
		sc->pos++;
		while (*sc->pos != '"' && *sc->pos != '\0') {
			sc->pos++;
			
			bool skip_close = sc->pos - start >= 1 &&
				*sc->pos == '"' &&
				sc->pos[-1] == '\\' &&
				sc->pos[-2] != '\\';
					
			if (skip_close)
				sc->pos++;
		}
		if (*sc->pos != '"')
			error(sc->ctx, ERR_UNTERMINATED_TEXT);
		sc->pos++;
		return make(T_TEXT);
	}
	
	switch (*sc->pos++) {
	case '+':
		if (check('=')) return make(T_PLUS_EQ);
		return make(T_PLUS);
	case '-':
		if (check('=')) return make(T_MINUS_EQ);
		if (check('>')) return make(T_MINUS_GT);
		return make(T_MINUS);
	case '*':
		if (check('=')) return make(T_STAR_EQ);
		return make(T_STAR);
	case '^':
		if (check('=')) return make(T_CARET_EQ);
		return make(T_CARET);
	case '/':
		if (check('=')) return make(T_SLASH_EQ);
		return make(T_SLASH);
	case '%':
		if (check('=')) return make(T_PERCENT_EQ);
		return make(T_PERCENT);
	case '(': return make(T_LPAREN);
	case ')': return make(T_RPAREN);
	case '[': return make(T_LSQUARE);
	case ']': return make(T_RSQUARE);
	case '{': return make(T_LCURLY);
	case '}': return make(T_RCURLY);
	case '&':
		if (check('=')) return make(T_AMPER_EQ);
		return make(T_AMPER);
	case '|': return make(T_PIPE);
	case '=':
		if (check('>')) return make(T_EQ_GT);
		return make(T_EQ);
	case '!':
		if (check('=')) return make(T_BANG_EQ);
		return make(T_BANG);
	case '<':
		if (check('=')) return make(T_LT_EQ);
		if (check('-')) return make(T_LT_MINUS);
		return make(T_LT);
	case '>':
		if (check('=')) return make(T_GT_EQ);
		return make(T_GT);
	case '.': return make(T_DOT);
	case ',': return make(T_COMMA);
	case ':': return make(T_COLON);
	case ';': return make(T_SEMICOLON);
	case '\0': return make(T_EOF);
	default: error(sc->ctx, 
		ERR_UNKNOWN_CHAR(sc->pos[-1]));
	}
	
	return make(T_EOF);
	
	#undef make
	#undef check
}

static void ps_init(
	parser_t* ps, const text_t* src, lur_t* ctx)
{
	assert(ps && src && ctx);
	
	sc_init(&ps->scanner, src, ctx);
	ps->cur.tag = -1;
	ps->cur.lex = NULL;
	ps->cur.line = 1;
	ps->prev.tag = -1;
	ps->prev.lex = NULL;
	ps->prev.line = 1;
	ps->prefix = NULL;
	ps->ctx = ctx;
}

static void ps_free(parser_t* ps) {
	assert(ps);
}

static void ps_next(parser_t* ps) {
	assert(ps);
	ps->prev = ps->cur;
	ps->cur = sc_next(&ps->scanner);
	
	#if LUR_DEBUG_PRINT_TOKENS
	lur_printf("token: ");
	text_print(ps->cur.lex);
	lur_printf("\n");
	#endif
}

static bool ps_check(parser_t* ps, token_tag_t tag) {
	assert(ps);
	return ps->cur.tag == tag;
}

static bool ps_match(parser_t* ps, token_tag_t tag) {
	assert(ps);
	if (ps_check(ps, tag))
		ps_next(ps);
	return ps->prev.tag == tag;
}

static void ps_eat(
	parser_t* ps, token_tag_t tag, const char* msg)
{
	assert(ps && msg);
	
	if (ps->cur.tag == tag) {
		ps_next(ps);
		return;
	}
	
	error(ps->ctx, msg);
}

static ast_node_t* ps_new_node(parser_t* ps) {
	assert(ps);
	ast_node_t* node =
		mem_alloc(ps->ctx, sizeof(ast_node_t));
	node->line = ps->prev.line;
	return node;
}

static ast_node_t* ps_prec(
	parser_t* ps,
	precedence_t prec)
{
	assert(ps);
	
	ps_next(ps);
	parse_fn_t prefix_rule = RULES[ps->prev.tag].prefix;
		
	if (!prefix_rule)
		error(ps->ctx, ERR_EXPECTED_EXPR(ps->prev.lex));
		
	ps->prefix = prefix_rule(ps);
	while (prec <= RULES[ps->cur.tag].prec) {
		ps_next(ps);
		parse_fn_t infix_rule = RULES[ps->prev.tag].infix;
		ps->prefix = infix_rule(ps);
	}
	
	return ps->prefix;
}

static ast_node_t* ps_expr(parser_t* ps) {
	assert(ps);
	return ps_prec(ps, PREC_ASSIGN);
}

static ast_node_t* ps_name(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	const text_t* name = ps->prev.lex;
	
	bool is_assignment = ps_match(ps, T_LT_MINUS) ||
		ps_match(ps, T_PLUS_EQ) ||
		ps_match(ps, T_MINUS_EQ) ||
		ps_match(ps, T_STAR_EQ) ||
		ps_match(ps, T_CARET_EQ) ||
		ps_match(ps, T_SLASH_EQ) ||
		ps_match(ps, T_PERCENT_EQ) ||
		ps_match(ps, T_AMPER_EQ);
	
	if (is_assignment) {
		node->tag = AST_STORE;
		node->data.store.tag = ps->prev.tag;
		node->data.store.name = name;
		node->data.store.value = ps_expr(ps);
		return node;
	}
	
	node->tag = AST_LOAD;
	node->data.load.name = name;
	return node;
}

static ast_node_t* ps_null(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_VALUE;
	node->data.value.data = make_null();
	return node;
}

static ast_node_t* ps_boolean(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_VALUE;
	node->data.value.data =
		make_bool(ps->prev.tag == T_TRUE);
	return node;
}

static ast_node_t* ps_number(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_VALUE;
	
	double num;
	switch (ps->prev.tag) {
	case T_NUMBER: {
		num = strtof(
			(const char*)ps->prev.lex->buffer, NULL);
		break;
	}
	case T_NUMBER_HEX: {
		num = strtol(
			(const char*)ps->prev.lex->buffer, NULL, 16);
		break;
	}
	case T_NUMBER_BIN: {
		num = strtol(
			(const char*)ps->prev.lex->buffer, NULL, 2);
		break;
	}
	default: unreachable();
	}
	
	node->data.value.data = make_number(num);
	return node;
}

static ast_node_t* ps_text(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_VALUE;
	text_t* text = text_new(
		ps->prev.lex->buffer + 1,
		ps->prev.lex->len - 2,
		ps->ctx);
	node->data.value.data = make_text(text);
	return node;
}

static ast_node_t* ps_list(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_LIST;
	
	node->data.list.items = NULL;
	node->data.list.len = 0;
	
	while (!ps_check(ps, T_EOF) &&
		!ps_check(ps, T_RSQUARE))
	{
		arr_alloc(ps->ctx,
			node->data.list.items, ast_node_t*,
			node->data.list.len, node->data.list.len + 1);
		node->data.list.items[node->data.list.len++] =
			ps_expr(ps);
			
		if (!ps_match(ps, T_COMMA))
			break;
	}
	
	ps_eat(ps, T_RSQUARE,
		"expected ',' or ']' after list items");
	return node;
}

static ast_node_t* ps_map(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_MAP;
	
	node->data.map.keys = NULL;
	node->data.map.values = NULL;
	node->data.map.len = 0;
	
	while (!ps_check(ps, T_EOF) &&
		!ps_check(ps, T_RCURLY))
	{
		arr_alloc(ps->ctx,
			node->data.map.keys, ast_node_t*,
			node->data.map.len, node->data.map.len + 1);
		node->data.map.keys[node->data.map.len++] =
			ps_expr(ps);
			
		ps_eat(ps, T_EQ_GT, "expected '=>' after entry key");
	
		arr_alloc(ps->ctx,
			node->data.map.values, ast_node_t*,
			node->data.map.len - 1, node->data.map.len);
		node->data.map.values[node->data.map.len - 1] =
			ps_expr(ps);
		
		if (!ps_match(ps, T_COMMA))
			break;
	}
	
	ps_eat(ps, T_RCURLY,
		"expected ',' or }' or after map items");
	return node;
}

static ast_node_t* ps_bind(parser_t* ps) {
	assert(ps);
	ps_eat(ps, T_NAME, "expected name after 'let'");
	const text_t* name = ps->prev.lex;
	
	if (ps_match(ps, T_LPAREN)) {
		ast_node_t* node = ps_new_node(ps);
		node->tag = AST_FUN;
		node->data.fun.name = name;
		node->data.fun.is_lambda = false;
		node->data.fun.nparams = 0;
	
		while (!ps_check(ps, T_EOF) &&
			!ps_check(ps, T_RPAREN))
		{
			if (node->data.fun.nparams == MAX_ARGS)
				error(ps->ctx, ERR_LIMIT(
					"max args", MAX_ARGS));
		
			ps_eat(ps, T_NAME, "expected parameter name");
			arr_alloc(ps->ctx,
				node->data.fun.params,
				text_t*,
				node->data.fun.nparams,
				node->data.fun.nparams + 1);
			node->data.fun.params[
				node->data.fun.nparams++] = ps->prev.lex;
				
			if (!ps_match(ps, T_COMMA))
				break;
		}
	
		ps_eat(ps, T_RPAREN,
			"expected ')' after parameters");
		ps_eat(ps, T_EQ, "expected '=' after ')'");
	
		node->data.fun.body = ps_expr(ps);
		return node;
	}
	
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_BIND;
	node->data.bind.name = ps->prev.lex;
	
	ps_eat(ps, T_EQ, "expected '=' after binding name");
	node->data.bind.value = ps_expr(ps);
	return node;
}

static ast_node_t* ps_unary(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_UNARY;
	
	token_tag_t tag = ps->prev.tag;
	node->data.unary.rhs = ps_prec(ps, PREC_UNARY);
	
	int opcode = -1;
	switch (tag) {
	case T_NOT: opcode = OP_NOT; break;
	case T_MINUS: opcode = OP_INV; break;
	default: unreachable();
	}
	
	node->data.unary.opcode = opcode;
	return node;
}

static ast_node_t* ps_binary(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_BINARY;
	node->data.binary.lhs = ps->prefix;
	
	token_tag_t tag = ps->prev.tag;
	parse_rule_t* rule = &RULES[tag];
	
	int prec = rule->prec + 1;
	if (tag == T_AND) prec = PREC_AND;
	else if (tag == T_OR) prec = PREC_OR;
	node->data.binary.rhs = ps_prec(ps, prec);
	
	int opcode = OP_NOP;
	switch (tag) {
	case T_EQ: opcode = OP_EQ; break;
	case T_BANG_EQ: opcode = OP_NE; break;
	case T_LT: opcode = OP_LT; break;
	case T_LT_EQ: opcode = OP_LTE; break;
	case T_GT: opcode = OP_GT; break;
	case T_GT_EQ: opcode = OP_GTE; break;
	case T_PLUS: opcode = OP_ADD; break;
	case T_MINUS: opcode = OP_SUB; break;
	case T_STAR: opcode = OP_MUL; break;
	case T_CARET: opcode = OP_POW; break;
	case T_SLASH: opcode = OP_DIV; break;
	case T_PERCENT: opcode = OP_REM; break;
	case T_AMPER: opcode = OP_CON; break;
	case T_AND: opcode = -1; break;
	case T_OR: opcode = -2; break;
	default: unreachable();
	}
	
	node->data.binary.opcode = opcode;
	return node;
}

static ast_node_t* ps_grouping(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_expr(ps);
	ps_eat(ps, T_RPAREN, "expected ')' after '('");
	return node;
}

static ast_node_t* ps_block(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_BLOCK;
	node->data.block.items = NULL;
	node->data.block.nitems = 0;
	
	while (!ps_check(ps, T_EOF) &&
		!ps_check(ps, T_END))
	{
		arr_alloc(ps->ctx,
			node->data.block.items,
			ast_node_t*,
			node->data.block.nitems,
			node->data.block.nitems + 1);
		node->data.block.items[
			node->data.block.nitems++] = ps_expr(ps);
	}
	
	ps_eat(ps, T_END, "expected 'end' after 'do'");
	return node;
}

static ast_node_t* ps_lambda(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_FUN;
	node->data.fun.name = text_lit("?", ps->ctx);
	node->data.fun.is_lambda = true;
	node->data.fun.nparams = 0;
	
	while (!ps_check(ps, T_EOF) &&
		!ps_check(ps, T_MINUS_GT))
	{
		if (node->data.fun.nparams == MAX_ARGS)
			error(ps->ctx, ERR_LIMIT("max args", MAX_ARGS));
		
		ps_eat(ps, T_NAME, "expected parameter name");
		arr_alloc(ps->ctx,
			node->data.fun.params,
			text_t*,
			node->data.fun.nparams,
			node->data.fun.nparams + 1);
		node->data.fun.params[
			node->data.fun.nparams++] = ps->prev.lex;
			
		if (!ps_match(ps, T_COMMA))
			break;
	}
	
	ps_eat(ps, T_MINUS_GT, "expected '->' after params");

	node->data.fun.body = ps_expr(ps);
	return node;
}

static ast_node_t* ps_call(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_CALL;
	
	node->data.call.func = ps->prefix;
	node->data.call.argc = 0;
	node->data.call.args = NULL;
	
	while (!ps_check(ps, T_EOF)) {
		if (node->data.call.argc == MAX_ARGS)
			error(ps->ctx, ERR_LIMIT("max call args",
				MAX_ARGS));
		
		arr_alloc(ps->ctx,
			node->data.call.args, ast_node_t*,
			node->data.call.argc, node->data.call.argc + 1);
		node->data.call.args[node->data.call.argc++] =
			ps_expr(ps);
			
		if (!ps_match(ps, T_COMMA))
			break;
	}
	
	return node;
}

static ast_node_t* ps_return(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_RETURN;
	node->data.ret.value = ps_expr(ps);
	return node;
}

static ast_node_t* ps_branch(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_BRANCH;
	
	node->data.branch.cond = ps_expr(ps);
	ps_eat(ps, T_THEN,
		"expected 'then' after branch condition");
	
	node->data.branch.a = ps_expr(ps);
	node->data.branch.b = NULL;
	if (ps_match(ps, T_ELSE))
		node->data.branch.b = ps_expr(ps);
	
	return node;
}

static ast_node_t* ps_dot(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_DOT;
	
	node->data.dot.lhs = ps->prefix;
	ps_eat(ps, T_NAME, "expected field name after '.'");
	node->data.dot.name = ps->prev.lex;
	
	node->data.dot.is_assignment =
		ps_match(ps, T_LT_MINUS) ||
		ps_match(ps, T_PLUS_EQ) ||
		ps_match(ps, T_MINUS_EQ) ||
		ps_match(ps, T_STAR_EQ) ||
		ps_match(ps, T_SLASH_EQ);
	node->data.dot.tag = ps->prev.tag;
	
	if (node->data.dot.is_assignment)
		node->data.dot.value = ps_expr(ps);
	return node;
}

static ast_node_t* ps_arrow(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_ARROW;
	
	ps_eat(ps, T_NAME,
		"expected method name after '->'");
	
	node->data.arrow.instance = ps->prefix;
	node->data.arrow.method_name = ps->prev.lex;
	node->data.arrow.argc = 0;
	node->data.arrow.args = NULL;
	
	ps_eat(ps, T_LPAREN,
		"expected '(' after method name");
	
	while (!ps_check(ps, T_EOF) &&
		!ps_check(ps, T_RPAREN))
	{
		if (node->data.call.argc == MAX_ARGS)
			error(ps->ctx, ERR_LIMIT("max call args",
				MAX_ARGS));
		
		arr_alloc(ps->ctx,
			node->data.arrow.args,
			ast_node_t*,
			nextpow2(node->data.arrow.argc),
			nextpow2(node->data.arrow.argc + 1));
		node->data.arrow.args[node->data.arrow.argc++] =
			ps_expr(ps);
		
		if (!ps_match(ps, T_COMMA))
			break;
	}
	
	ps_eat(ps, T_RPAREN, "expected ')' or ',' after args");
	return node;
}

static ast_node_t* ps_parse(parser_t* ps) {
	assert(ps);
	ast_node_t* ast = ps_new_node(ps);
	ast->tag = AST_BLOCK;
	ast->data.block.items = NULL;
	ast->data.block.nitems = 0;
	
	ps_next(ps);
	while (!ps_check(ps, T_EOF)) {
		arr_alloc(ps->ctx,
			ast->data.block.items,
			ast_node_t*,
			ast->data.block.nitems,
			ast->data.block.nitems + 1);
		ast->data.block.items[
			ast->data.block.nitems++] = ps_expr(ps);
	}
	
	ps_eat(ps, T_EOF, "expected end of file");
	return ast;
}

static void cl_add_sym(comp_t*, const text_t*);
static void cl_push_value(comp_t*, value_t);

static void cl_init(comp_t* cl, lur_t* ctx) {
	assert(cl && ctx);
	
	cl->func = func_new(ctx);
	cl->func->name = text_lit("<script>", ctx);
	cl->depth = -1;
	cl->syms = NULL;
	cl->nsyms = 0;
	cl->parent = NULL;
	cl->cur = NULL;
	cl->ctx = ctx;
	
	cl_add_sym(cl, text_lit("", ctx));
}

static void cl_free(comp_t* cl) {
	assert(cl);
	ps_free(&cl->parser);
	arr_free(cl->ctx, cl->syms, sym_t, cl->nsyms);
}

static void cl_write(comp_t* cl, uint8_t byte) {
	assert(cl);
	func_write(cl->func, byte, cl->cur->line, cl->ctx);
}

static size_t cl_write_jump(
	comp_t* cl, opcode_t opcode)
{
	assert(cl);
	cl_write(cl, opcode);
	cl_write(cl, 0xff);
	cl_write(cl, 0xff);
	return cl->func->nops - 2;
}

static void cl_patch_jump(comp_t* cl, size_t addr) {
	assert(cl);
	
	size_t offset = cl->func->nops - addr - 2;
	if (offset > MAX_JUMP)
		error(cl->ctx, ERR_LIMIT("max jump", MAX_JUMP));
	
	cl->func->ops[addr] = (offset >> 8) & 0xff;
	cl->func->ops[addr + 1] = offset & 0xff;
}

static void cl_push_value(comp_t* cl, value_t value) {
	assert(cl);
	
	if (value.tag == TYPE_FUNC) {
		size_t slot = func_write_value(
			cl->func, value, cl->ctx);
		cl_write(cl, OP_NEWFREF);
		cl_write(cl, (slot >> 8) & 0xff);
		cl_write(cl, slot & 0xff);
		return;
	}
	
	if (value.tag == TYPE_TEXT) {
		value = make_text(
			text_escape(get_text(value), cl->ctx));
	}
	
	size_t slot = func_write_value(cl->func, value, cl->ctx);
	cl_write(cl, OP_DATA);
	cl_write(cl, (slot >> 8) & 0xff);
	cl_write(cl, slot & 0xff);
}

static void cl_add_sym(comp_t* cl, const text_t* name) {
	assert(cl && name);
	
	sym_t sym;
	sym.name = name;
	sym.depth = cl->depth;
	sym.is_captured = false;
	
	arr_alloc(cl->ctx, cl->syms, sym_t,
		cl->nsyms, cl->nsyms + 1);
	cl->syms[cl->nsyms++] = sym;
}

static int64_t cl_get_sym(
	comp_t* cl, const text_t* name)
{
	assert(cl && name);
	if (cl->nsyms == 0) return -1;
	for (int64_t i = cl->nsyms - 1; i >= 0; i--)
		if (text_eq(name, cl->syms[i].name))
			return i;
	return -1;
}

static size_t cl_add_vref(
	comp_t* cl, size_t index, bool is_local)
{
	assert(cl);
	
	for (size_t i = 0; i < cl->func->nvrefs; i++) {
		cl_vref_t* vref = &cl->vrefs[i];
		if (vref->index == index && vref->is_local == is_local)
			return i;
	}
	
	if (cl->func->nvrefs == MAX_VREFS)
		error(cl->ctx, ERR_LIMIT("max vref", MAX_VREFS));
	
	cl->vrefs[cl->func->nvrefs].index = index;
	cl->vrefs[cl->func->nvrefs].is_local = is_local;
	return cl->func->nvrefs++;
}

static int64_t cl_get_vref(
	comp_t* cl, const text_t* name)
{
	assert(cl && name);
	if (!cl->parent) return -1;
	
	int64_t local = cl_get_sym(cl->parent, name);
	if (local != -1) {
		cl->parent->syms[local].is_captured = true;
		return cl_add_vref(cl, local, true);
	}
	
	int64_t vref = cl_get_vref(cl->parent, name);
	if (vref != -1)
		return cl_add_vref(cl, vref, false);
	
	return -1;
}

static void cl_open_scope(comp_t* cl) {
	assert(cl);
	cl->depth++;
}

static void cl_close_scope(comp_t* cl) {
	assert(cl);
	cl->depth--;
		
	int locals = 0;
	int index = cl->nsyms - 1;
	while (index > 0 && cl->syms[index].depth > cl->depth)
	{
		locals++;
		index--;
	}
		
	if (locals == 0) return;
	if (locals == 1) {
		cl_write(cl, OP_SWAP);
	} else {
		size_t pos = 1;
		cl_write(cl, OP_MOVE);
		cl_write(cl, (pos >> 8) & 0xff);
		cl_write(cl, pos & 0xff);
	}
		
	while (cl->nsyms > 0 &&
		cl->syms[cl->nsyms - 1].depth > cl->depth)
	{
		if (cl->syms[cl->nsyms - 1].is_captured)
			cl_write(cl, OP_NEWVREF);
		else
			cl_write(cl, OP_POP);
			
		cl->nsyms--;
	}
		
	arr_alloc(cl->ctx, cl->syms, sym_t,
		cl->nsyms + locals, cl->nsyms);
}

static void cl_emit_load(comp_t* cl, const text_t* name) {
	assert(cl && name);
	
	int64_t slot = cl_get_sym(cl, name);
	if (slot != -1) {
		cl_write(cl, OP_GETLOC);
		cl_write(cl, (slot >> 8) & 0xff);
		cl_write(cl, slot & 0xff);
		return;
	}
		
	slot = cl_get_vref(cl, name);
	if (slot != -1) {
		cl_write(cl, OP_GETVREF);
		cl_write(cl, (slot >> 8) & 0xff);
		cl_write(cl, slot & 0xff);
		return;
	}
		
	slot = func_write_value(
		cl->func, make_text(name), cl->ctx);
	cl_write(cl, OP_GETGLOB);
	cl_write(cl, (slot >> 8) & 0xff);
	cl_write(cl, slot & 0xff);
}

static void cl_compile_ast(
	comp_t* cl, ast_node_t* node)
{
	assert(cl && node);
	cl->cur = node;
	
	switch (node->tag) {
	case AST_VALUE: {
		ast_value_t* value = &node->data.value;
		cl_push_value(cl, value->data);
		break;
	}
	case AST_LIST: {
		ast_list_t* list = &node->data.list;
		if (list->len > MAX_LIST_LIT_ITEMS)
			error(cl->ctx, ERR_LIMIT(
				"list literal items", MAX_LIST_LIT_ITEMS));
		
		for (size_t i = 0; i < list->len; i++)
			cl_compile_ast(cl, list->items[i]);
		cl_write(cl, OP_NEWLIST);
		cl_write(cl, (list->len >> 8) & 0xff);
		cl_write(cl, list->len & 0xff);
		break;
	}
	case AST_MAP: {
		ast_map_t* map = &node->data.map;
		if (map->len > MAX_MAP_LIT_ITEMS)
			error(cl->ctx, ERR_LIMIT(
				"map literal items", MAX_MAP_LIT_ITEMS));
		
		for (size_t i = 0; i < map->len; i++) {
			cl_compile_ast(cl, map->keys[i]);
			cl_compile_ast(cl, map->values[i]);
		}
		cl_write(cl, OP_NEWMAP);
		cl_write(cl, (map->len >> 8) & 0xff);
		cl_write(cl, map->len & 0xff);
		break;
	}
	case AST_BIND: {
		ast_bind_t* bind = &node->data.bind;
		cl_compile_ast(cl, bind->value);
		
		bool is_local = cl->depth > 0;
		if (is_local) cl_add_sym(cl, bind->name);
		else {
			size_t slot = func_write_value(
				cl->func, make_text(bind->name), cl->ctx);
			cl_write(cl, OP_ADDGLOB);
			cl_write(cl, (slot >> 8) & 0xff);
			cl_write(cl, slot & 0xff);
		}
		break;
	}
	case AST_LOAD: {
		ast_load_t* load = &node->data.load;
		cl_emit_load(cl, load->name);
		break;
	}
	case AST_STORE: {
		ast_store_t* store = &node->data.store;
		if (store->tag != T_LT_MINUS)
			cl_emit_load(cl, store->name);
		
		cl_compile_ast(cl, store->value);
		
		switch (store->tag) {
		case T_LT_MINUS: break;
		case T_PLUS_EQ: cl_write(cl, OP_ADD); break;
		case T_MINUS_EQ: cl_write(cl, OP_SUB); break;
		case T_STAR_EQ: cl_write(cl, OP_MUL); break;
		case T_CARET_EQ: cl_write(cl, OP_POW); break;
		case T_SLASH_EQ: cl_write(cl, OP_DIV); break;
		case T_PERCENT_EQ: cl_write(cl, OP_REM); break;
		case T_AMPER_EQ: cl_write(cl, OP_CON); break;
		default: unreachable();
		}
		
		int64_t slot = cl_get_sym(cl, store->name);
		if (slot != -1) {
			cl_write(cl, OP_SETLOC);
			cl_write(cl, (slot >> 8) & 0xff);
			cl_write(cl, slot & 0xff);
			break;
		}
		
		slot = cl_get_vref(cl, store->name);
		if (slot != -1) {
			cl_write(cl, OP_SETVREF);
			cl_write(cl, (slot >> 8) & 0xff);
			cl_write(cl, slot & 0xff);
			break;
		}
		
		slot = func_write_value(
			cl->func, make_text(store->name), cl->ctx);
		cl_write(cl, OP_SETGLOB);
		cl_write(cl, (slot >> 8) & 0xff);
		cl_write(cl, slot & 0xff);
		break;
	}
	case AST_UNARY: {
		ast_unary_t* unary = &node->data.unary;
		cl_compile_ast(cl, unary->rhs);
		cl_write(cl, unary->opcode);
		break;
	}
	case AST_BINARY: {
		ast_binary_t* binary = &node->data.binary;
		cl_compile_ast(cl, binary->lhs);
		
		if (binary->opcode >= 0) {
			cl_compile_ast(cl, binary->rhs);
			cl_write(cl, binary->opcode);
		} else {
			int jump = cl_write_jump(cl,
				(binary->opcode == -2) ? OP_IF : OP_IFNOT);
			cl_write(cl, OP_POP);
			cl_compile_ast(cl, binary->rhs);
			cl_patch_jump(cl, jump);
		}
		break;
	}
	case AST_BLOCK: {
		ast_block_t* block = &node->data.block;
		if (block->nitems == 0) {
			cl_push_value(cl, make_null());
			break;
		}
			
		cl_open_scope(cl);
		for (size_t i = 0; i < block->nitems; i++) {
			cl_compile_ast(cl, block->items[i]);
			
			if (block->items[i]->tag != AST_BIND &&
				block->items[i]->tag != AST_FUN &&
				i != block->nitems - 1)
			{
				cl_write(cl, OP_POP);
			}
		}
		
		cl_close_scope(cl);
		break;
	}
	case AST_FUN: {
		ast_fun_t* fun = &node->data.fun;
		bool is_local = cl->depth > 0;
		if (is_local && !fun->is_lambda)
			 cl_add_sym(cl, fun->name);
			
		cl_open_scope(cl);
		
		comp_t env;
		cl_init(&env, cl->ctx);
		
		env.func->name = text_concat(
			cl->func->name, text_lit("/", cl->ctx), cl->ctx);
		env.func->name = text_concat(
			env.func->name, fun->name, cl->ctx);
			
		env.func->src = cl->func->src;
		env.func->argc = fun->nparams;
		env.depth = cl->depth;
		env.parent = cl;
	
		for (size_t i = 0; i < fun->nparams; i++)
			cl_add_sym(&env, fun->params[i]);
		cl_compile_ast(&env, fun->body);
		cl_write(&env, OP_RET);
		
		cl_free(&env);
		cl_close_scope(cl);
		
		cl_push_value(cl, make_function(env.func));
		for (size_t i = 0; i < env.func->nvrefs; i++) {
			cl_write(cl, env.vrefs[i].index);
			cl_write(cl, (env.vrefs[i].is_local) ? 1 : 0);
		}
		
		if (!fun->is_lambda) {
			bool is_local = cl->depth > 0;
			if (!is_local) {
				size_t slot = func_write_value(
					cl->func, make_text(fun->name), cl->ctx);
				cl_write(cl, OP_ADDGLOB);
				cl_write(cl, (slot >> 8) & 0xff);
				cl_write(cl, slot & 0xff);
			}
		}
		break;
	}
	case AST_CALL: {
		ast_call_t* call = &node->data.call;
		cl_compile_ast(cl, call->func);
		for (uint8_t i = 0; i < call->argc; i++)
			cl_compile_ast(cl, call->args[i]);
		
		cl_write(cl, OP_CALL);
		cl_write(cl, call->argc);
		break;
	}
	case AST_RETURN: {
		ast_return_t* ret = &node->data.ret;
		cl_compile_ast(cl, ret->value);
		cl_close_scope(cl);
		cl_write(cl, OP_RET);
		break;
	}
	case AST_DOT: {
		ast_dot_t* dot = &node->data.dot;
		cl_compile_ast(cl, dot->lhs);
		size_t slot = func_write_value(
			cl->func, make_text(dot->name), cl->ctx);
		
		if (dot->is_assignment) {
			if (dot->tag != T_LT_MINUS) {
				cl_compile_ast(cl, dot->lhs);
				cl_write(cl, OP_GETFIELD);
				cl_write(cl, (slot >> 8) & 0xff);
				cl_write(cl, slot & 0xff);
			}
			cl_compile_ast(cl, dot->value);
			
			switch (dot->tag) {
			case T_LT_MINUS: break;
			case T_PLUS_EQ: cl_write(cl, OP_ADD); break;
			case T_MINUS_EQ: cl_write(cl, OP_SUB); break;
			case T_STAR_EQ: cl_write(cl, OP_MUL); break;
			case T_SLASH_EQ: cl_write(cl, OP_DIV); break;
			default: break;
			}
			
			cl_write(cl, OP_SETFIELD);
			cl_write(cl, (slot >> 8) & 0xff);
			cl_write(cl, slot & 0xff);
			return;
		}
		
		cl_write(cl, OP_GETFIELD);
		cl_write(cl, (slot >> 8) & 0xff);
		cl_write(cl, slot & 0xff);
		break;
	}
	case AST_ARROW: {
		ast_arrow_t* arrow = &node->data.arrow;
		cl_compile_ast(cl, arrow->instance);
		size_t slot = func_write_value(
			cl->func,
			make_text(arrow->method_name),
			cl->ctx);
		
		cl_write(cl, OP_GETFIELD);
		cl_write(cl, (slot >> 8) & 0xff);
		cl_write(cl, slot & 0xff);
		
		cl_compile_ast(cl, arrow->instance);
		for (uint8_t i = 0; i < arrow->argc; i++)
			cl_compile_ast(cl, arrow->args[i]);
		
		cl_write(cl, OP_CALL);
		cl_write(cl, arrow->argc + 1);
		break;
	}
	case AST_BRANCH: {
		ast_branch_t* branch = &node->data.branch;
		cl_open_scope(cl);
		cl_compile_ast(cl, branch->cond);
		
		size_t skip_a = cl_write_jump(cl, OP_IFNOT);
		cl_write(cl, OP_POP);
		cl_compile_ast(cl, branch->a);
		
		size_t skip_b = cl_write_jump(cl, OP_JMP);
		cl_patch_jump(cl, skip_a);
		cl_write(cl, OP_POP);
		
		if (branch->b) cl_compile_ast(cl, branch->b);
		else cl_push_value(cl, make_null());
		
		cl_patch_jump(cl, skip_b);
		cl_close_scope(cl);
		break;
	}
	default: unreachable();
	}
}

static void cl_free_ast(
	comp_t* cl, ast_node_t* node)
{
	assert(cl && node);
	switch (node->tag) {
	case AST_VALUE: break;
	case AST_LIST: {
		ast_list_t* list = &node->data.list;
		for (size_t i = 0; i < list->len; i++)
			cl_free_ast(cl, list->items[i]);
		arr_free(cl->ctx,
			list->items, ast_node_t*, list->len);
		break;
	}
	case AST_MAP: {
		ast_map_t* map = &node->data.map;
		for (size_t i = 0; i < map->len; i++) {
			cl_free_ast(cl, map->keys[i]);
			cl_free_ast(cl, map->values[i]);
		}
		arr_free(cl->ctx,
			map->keys, ast_node_t*, map->len);
		arr_free(cl->ctx,
			map->values, ast_node_t*, map->len);
		break;
	}
	case AST_BIND: {
		ast_bind_t* bind = &node->data.bind;
		cl_free_ast(cl, bind->value);
		break;
	}
	case AST_LOAD: break;
	case AST_STORE: {
		ast_store_t* store = &node->data.store;
		cl_free_ast(cl, store->value);
		break;
	}
	case AST_UNARY: {
		ast_unary_t* unary = &node->data.unary;
		cl_free_ast(cl, unary->rhs);
		break;
	}
	case AST_BINARY: {
		ast_binary_t* binary = &node->data.binary;
		cl_free_ast(cl, binary->lhs);
		cl_free_ast(cl, binary->rhs);
		break;
	}
	case AST_BLOCK: {
		ast_block_t* block = &node->data.block;
		for (size_t i = 0; i < block->nitems; i++)
			cl_free_ast(cl, block->items[i]);
		arr_free(cl->ctx,
			block->items, ast_node_t*, block->nitems);
		break;
	}
	case AST_FUN: {
		ast_fun_t* fun = &node->data.fun;
		cl_free_ast(cl, fun->body);
		arr_free(cl->ctx,
			fun->params, text_t*, fun->nparams);
		break;
	}
	case AST_CALL: {
		ast_call_t* call = &node->data.call;
		cl_free_ast(cl, call->func);
		for (uint8_t i = 0; i < call->argc; i++)
			cl_free_ast(cl, call->args[i]);
		arr_free(cl->ctx,
			call->args, ast_node_t*, call->argc);
		break;
	}
	case AST_RETURN: {
		ast_return_t* ret = &node->data.ret;
		cl_free_ast(cl, ret->value);
		break;
	}
	case AST_DOT: {
		ast_dot_t* dot = &node->data.dot;
		if (dot->is_assignment)
			cl_free_ast(cl, dot->value);
		break;
	}
	case AST_ARROW: {
		ast_arrow_t* arrow = &node->data.arrow;
		cl_free_ast(cl, arrow->instance);
		for (uint8_t i = 0; i < arrow->argc; i++)
			cl_free_ast(cl, arrow->args[i]);
		arr_free(cl->ctx,
			arrow->args, ast_node_t*, arrow->argc);
		break;
	}
	case AST_BRANCH: {
		ast_branch_t* branch = &node->data.branch;
		cl_free_ast(cl, branch->cond);
		cl_free_ast(cl, branch->a);
		if (branch->b)
			cl_free_ast(cl, branch->b);
		break;
	}
	default: unreachable();
	}
	
	mem_free(cl->ctx, node, sizeof(ast_node_t));
}

static void cl_compile(
	comp_t* cl, const text_t* src, const text_t* name)
{
	assert(cl && src && name);
	
	ps_init(&cl->parser, src, cl->ctx);
	cl->func->name = name;
	cl->func->src = src;
	ast_node_t* ast = ps_parse(&cl->parser);
	cl_compile_ast(cl, ast);
	
	#if LUR_DEBUG_PRINT_AST
	dbg_print_ast(cl, ast, 0);
	#endif
	
	if (cl->ctx->interpreter) {
		size_t slot = func_write_value(
			cl->func,
			make_text(text_lit("IO", cl->ctx)),
			cl->ctx);
		cl_write(cl, OP_GETGLOB);
		cl_write(cl, (slot >> 8) & 0xff);
		cl_write(cl, slot & 0xff);
		
		slot = func_write_value(
			cl->func,
			make_text(text_lit("print_ln", cl->ctx)),
			cl->ctx);
		cl_write(cl, OP_GETFIELD);
		cl_write(cl, (slot >> 8) & 0xff);
		cl_write(cl, slot & 0xff);
		
		cl_write(cl, OP_SWAP);
		
		cl_write(cl, OP_CALL);
		cl_write(cl, 1);
	}
	
	cl_write(cl, OP_HALT);
	cl_free_ast(cl, ast);
}

static double math_rand(double start, double end) {
	double scale = rand() / (double)RAND_MAX;
	return start + scale * (end - start);
}

static text_t* io_read(const text_t* path, lur_t* ctx) {
	assert(path && ctx);
	FILE* fp = fopen((const char*)path->buffer, "r");
	if (!fp) error(ctx, ERR_READ_FAILED(path));
	
	fseek(fp, 0, SEEK_END);
	size_t len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	text_t* text = text_new(NULL, len,  ctx);
	fread(text->buffer, 1, len, fp);
	fclose(fp);
	return text;
}

static text_t* io_read_stdin(lur_t* ctx) {
	assert(ctx);
	
	char* line = NULL;
	size_t len = 0;
	if (getline(&line, &len, stdin) == -1) {
		free(line);
		error(ctx, ERR_READ_FAILED(text_lit("stdin", ctx)));
	}
	
	text_t* text = text_new(
		(const uint8_t*)line, strlen(line), ctx);
	free(line);
	return text;
}

static void io_write(
	const text_t* path, const text_t* output, lur_t* ctx)
{
	assert(path && output && ctx);
	FILE* fp = fopen((const char*)path->buffer, "w");
	if (!fp) error(ctx, ERR_WRITE_FAILED(path));
	fputs((char*)output->buffer, fp);
	fclose(fp);
}

static void io_append(
	const text_t* path, const text_t* output, lur_t* ctx)
{
	assert(path && output && ctx);
	FILE* fp = fopen((const char*)path->buffer, "a");
	if (!fp) error(ctx, ERR_WRITE_FAILED(path));
	fputs((char*)output->buffer, fp);
	fclose(fp);
}

static size_t io_file_size(const text_t* path, lur_t* ctx) {
	assert(path && ctx);
	FILE* fp = fopen((const char*)path->buffer, "r");
	if (!fp) error(ctx, ERR_READ_FAILED(path));
	fseek(fp, 0, SEEK_END);
	size_t len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	return len;
}

#define typecheck(arg, t) \
	if (args[arg].tag != (t)) \
		error(ctx, ERR_TYPECHECK(args[arg].tag, (t)))

value_t call_function(
	lur_t* ctx, const fref_t* fref, value_t* args, size_t argc)
{
	assert(ctx && fref);
	if (args) {
		for (size_t i = 0; i < argc; i++)
			*ctx->vm.sp++ = args[i];
	}
	
	if (fref->func->syscall)
		vm_call(&ctx->vm, make_fref(fref), argc, false);
	else vm_launch(&ctx->vm, make_fref(fref), argc, true);
	return ctx->vm.sp[-1];
}

static value_t std_load(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	cl_init(&ctx->cl, ctx);
	cl_compile(&ctx->cl,
		io_read(get_text(args[0]), ctx),
		get_text(args[0]));
	
	vm_launch(&ctx->vm,
		make_fref(fref_new(ctx->cl.func, ctx)), 0, true);
		
	cl_free(&ctx->cl);
	return make_null();
}

static value_t std_eval(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	cl_init(&ctx->cl, ctx);
	cl_compile(&ctx->cl,
		get_text(args[0]),
		get_text(args[0]));
	
	value_t result = vm_launch(&ctx->vm,
		make_fref(fref_new(ctx->cl.func, ctx)), 0, true);
		
	cl_free(&ctx->cl);
	return result;
}

static value_t std_system(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	system((const char*)get_text(args[0])->buffer);
	return make_null();
}

static value_t std_error(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	error(ctx, (const char*)get_text(args[0])->buffer);
	return make_null();
}

static value_t std_type_of(value_t* args, lur_t* ctx) {
	return make_text(
		text_lit(type_name(args[0].tag), ctx));
}

static value_t std_to_num(value_t* args, lur_t* ctx) {
	switch (args[0].tag) {
	case TYPE_NULL: return make_number(0);
	case TYPE_BOOL: return make_number(
		get_bool(args[0]));
	case TYPE_NUMBER: return args[0];
	case TYPE_TEXT: return make_number(strtof(
			(const char*)get_text(args[0])->buffer, NULL));
	case TYPE_LIST: {
		list_t* list = get_list(args[0]);
		text_t* text = list_join(list, ctx);
		return std_to_num(&make_text(text), ctx);
	}
	}
	return make_null();
}

static value_t std_to_text(value_t* args, lur_t* ctx) {
	return make_text(value_to_text(args[0], ctx));
}

static value_t std_repeat(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_FREF);
	double n = get_number(args[0]);
	fref_t* fref = get_fref(args[1]);
	
	for (size_t i = 0; i < n; i++) {
		value_t args[] = { make_number(i) };
		call_function(ctx, fref, args, 1);
	}
	
	return make_null();
}

static value_t std_while(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_FREF);
	fref_t* fref = get_fref(args[0]);
	
	while (true) {
		value_t args[] = {};
		value_t result = call_function(ctx, fref, args, 0);
		if (result.tag != TYPE_BOOL)
			error(ctx, ERR_TYPECHECK(
				result.tag, TYPE_BOOL));
		
		if (!get_bool(result))
			break;
	}
	
	return make_null();
}

static value_t std_call(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_FREF);
	return call_function(ctx, get_fref(args[0]), NULL, 0);
}

static value_t std_range(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	int64_t start = (int64_t)get_number(args[0]);
	int64_t end = (int64_t)get_number(args[1]);
	list_t* list = list_new(ctx);
	if (start < end)
		for (int64_t i = start; i < end; i++)
			list_push(list, make_number(i), ctx);
	else
		for (int64_t i = start; i > end; i--)
			list_push(list, make_number(i), ctx);
	return make_list(list);
}

static value_t std_range_inc(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	int64_t start = (int64_t)get_number(args[0]);
	int64_t end = (int64_t)get_number(args[1]);
	list_t* list = list_new(ctx);
	if (start < end)
		for (int64_t i = start; i <= end; i++)
			list_push(list, make_number(i), ctx);
	else
		for (int64_t i = start; i >= end; i--)
			list_push(list, make_number(i), ctx);
	return make_list(list);
}

static value_t std_enum(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	list_t* list = get_list(args[0]);
	map_t* enum_ = map_new(ctx);
	for (size_t i = 0; i < list->len; i++) {
		text_t* name = value_to_text(list->items[i], ctx);
		map_set(enum_,
			make_text(name),
			make_number(i),
			ctx);
	}
	return make_map(enum_);
}

static value_t std_exit(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	exit(get_number(args[0]));
}

static value_t std_assert(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_BOOL);
	if (!get_bool(args[0])) error(ctx, ERR_ASSERTION);
	return make_null();
}

static value_t std_timestamp(value_t* args, lur_t* ctx) {
	return make_number(time(NULL));
}

static value_t std_clock(value_t* args, lur_t* ctx) {
	return make_number((double)clock() / CLOCKS_PER_SEC);
}

static value_t std_math_eqf(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	typecheck(2, TYPE_NUMBER);
	double a = get_number(args[0]);
	double b = get_number(args[1]);
	double eps = get_number(args[2]);
	if (a == b) return make_bool(true);
	else return make_bool(fabs(a - b) < eps);
}

static value_t std_math_shl(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	uint64_t a = (uint64_t)get_number(args[0]);
	uint64_t b = (uint64_t)get_number(args[1]);
	return make_number(a << b);
}

static value_t std_math_shr(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	uint64_t a = (uint64_t)get_number(args[0]);
	uint64_t b = (uint64_t)get_number(args[1]);
	return make_number(a >> b);
}

static value_t std_math_band(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	uint64_t a = (uint64_t)get_number(args[0]);
	uint64_t b = (uint64_t)get_number(args[1]);
	return make_number(a & b);
}

static value_t std_math_bor(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	uint64_t a = (uint64_t)get_number(args[0]);
	uint64_t b = (uint64_t)get_number(args[1]);
	return make_number(a | b);
}

static value_t std_math_xor(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	uint64_t a = (uint64_t)get_number(args[0]);
	uint64_t b = (uint64_t)get_number(args[1]);
	return make_number(a ^ b);
}

static value_t std_math_bnot(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(
		~((int64_t)get_number(args[0])));
}

static value_t std_math_log(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	double target = get_number(args[0]);
	double base = get_number(args[1]);
	return make_number(log(target) / log(base));
}

static value_t std_math_sqrt(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(sqrt(get_number(args[0])));
}

static value_t std_math_squared(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_NUMBER);
	double x = get_number(args[0]);
	return make_number(x * x);
}

static value_t std_math_abs(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(fabs(get_number(args[0])));
}

static value_t std_math_min(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	double a = get_number(args[0]);
	double b = get_number(args[1]);
	return make_number((a < b) ? a : b);
}

static value_t std_math_max(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	double a = get_number(args[0]);
	double b = get_number(args[1]);
	return make_number((a > b) ? a : b);
}

static value_t std_math_clip(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	typecheck(2, TYPE_NUMBER);
	double x = get_number(args[0]);
	double lo = get_number(args[1]);
	double hi = get_number(args[2]);
	return make_number((x < lo) ? lo : (x > hi) ? hi : x);
}

static value_t std_math_lerp(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	typecheck(2, TYPE_NUMBER);
	double t = get_number(args[0]);
	double a = get_number(args[1]);
	double b = get_number(args[2]);
	return make_number(a * (1.0 - t) + b * t);
}

static value_t std_math_floor(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(floor(get_number(args[0])));
}

static value_t std_math_ceil(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(ceil(get_number(args[0])));
}

static value_t std_math_round(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(round(get_number(args[0])));
}

static value_t std_math_rad(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(
		get_number(args[0]) * (M_PI / 180.0));
}

static value_t std_math_deg(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(
		get_number(args[0]) * (180.0 / M_PI));
}

static value_t std_math_sin(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(sin(get_number(args[0])));
}

static value_t std_math_cos(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(cos(get_number(args[0])));
}

static value_t std_math_tan(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(tan(get_number(args[0])));
}

static value_t std_math_asin(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(asin(get_number(args[0])));
}

static value_t std_math_acos(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(acos(get_number(args[0])));
}

static value_t std_math_atan(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(atan(get_number(args[0])));
}

static value_t std_math_atan2(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	double x = get_number(args[0]);
	double y = get_number(args[1]);
	return make_number(atan2(x, y));
}

static value_t std_math_sinh(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(sinh(get_number(args[0])));
}

static value_t std_math_cosh(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(cosh(get_number(args[0])));
}

static value_t std_math_tanh(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(tanh(get_number(args[0])));
}

static value_t std_math_asinh(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(asinh(get_number(args[0])));
}

static value_t std_math_acosh(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(acosh(get_number(args[0])));
}

static value_t std_math_atanh(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(atanh(get_number(args[0])));
}

static value_t std_math_even(value_t* args, lur_t* ctx)  {
	typecheck(0, TYPE_NUMBER);
	return make_bool(fmod(
		get_number(args[0]), 2.0) == 0.0);
}

static value_t std_math_odd(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_bool(fmod(
		get_number(args[0]), 2.0) != 0.0);
}

static value_t std_math_hex(value_t* args, lur_t* ctx) {
	if (type_is_obj(args[0].tag))
		return make_text(text_fmt(
			ctx, "%p", args[0].data.obj));
	
	typecheck(0, TYPE_NUMBER);
	return make_text(text_fmt(
		ctx, "0x%x", (uint64_t)get_number(args[0])));
}

static value_t std_math_bin(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	int num = get_number(args[0]);
	
	static char buf[32] = {0};
	int i = 30;
	for(; num && i; i--, num /= 2)
		buf[i] = "01"[num % 2];
	
	text_t* text = text_lit(&buf[i + 1], ctx);
	text = text_concat(text_lit("0b", ctx), text, ctx);
	return make_text(text);
}

static value_t std_math_hash(value_t* args, lur_t* ctx) {
	return make_number(value_hash(args[0], ctx));
}

static value_t std_math_rand(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	return make_number(math_rand(get_number(args[0]),
		get_number(args[1])));
}

static value_t std_math_srand(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	srand(get_number(args[0]));
	return make_null();
}

static value_t std_math_size(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	const list_t* vec = get_list(args[0]);
	
	double size = 0.0;
	for (size_t i = 0; i < vec->len; i++) {
		if (vec->items[i].tag != TYPE_NUMBER)
			error(ctx, ERR_TYPECHECK(
				vec->items[i].tag, TYPE_NUMBER));
		
		double number = get_number(vec->items[i]);
		size += number * number;
	}
	
	return make_number(sqrt(size));
}

static value_t std_math_norm(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	const list_t* vec = get_list(args[0]);
	list_t* out = list_copy(vec, ctx);
	
	double size = 0.0;
	for (size_t i = 0; i < vec->len; i++) {
		if (vec->items[i].tag != TYPE_NUMBER)
			error(ctx, ERR_TYPECHECK(
				vec->items[i].tag, TYPE_NUMBER));
		
		double number = get_number(vec->items[i]);
		size += number * number;
	}
	
	size = sqrt(size);
	
	if (size > 0.0) {
		for (size_t i = 0; i < vec->len; i++) {
			out->items[i] = make_number(
				get_number(vec->items[i]) / size);
		}
	}
	
	return make_list(out);
}

static value_t std_math_dot(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	typecheck(1, TYPE_LIST);
	const list_t* a = get_list(args[0]);
	const list_t* b = get_list(args[1]);
	
	double dot = 0.0;
	size_t shortest = (a->len < b->len) ? a->len : b->len;
	for (size_t i = 0; i < shortest; i++) {
		if (a->items[i].tag != TYPE_NUMBER)
			error(ctx, ERR_TYPECHECK(
				a->items[i].tag, TYPE_NUMBER));
				
		if (b->items[i].tag != TYPE_NUMBER)
			error(ctx, ERR_TYPECHECK(
				b->items[i].tag, TYPE_NUMBER));
		
		dot += get_number(a->items[i]) *
			get_number(b->items[i]);
	}
	
	return make_number(dot);
}

static value_t std_math_cross(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	typecheck(1, TYPE_LIST);
	const list_t* a = get_list(args[0]);
	const list_t* b = get_list(args[1]);
	list_t* out = list_new(ctx);
	
	size_t shortest = (a->len < b->len) ? a->len : b->len;
	for (size_t i = 0; i < shortest; i++) {
		size_t j = (i + 1) % shortest;
		size_t k = (i + 2) % shortest;
		
		if (a->items[j].tag != TYPE_NUMBER)
			error(ctx, ERR_TYPECHECK(
				a->items[j].tag, TYPE_NUMBER));
				
		if (b->items[k].tag != TYPE_NUMBER)
			error(ctx, ERR_TYPECHECK(
				b->items[k].tag, TYPE_NUMBER));
				
		if (a->items[k].tag != TYPE_NUMBER)
			error(ctx, ERR_TYPECHECK(
				a->items[k].tag, TYPE_NUMBER));
				
		if (b->items[j].tag != TYPE_NUMBER)
			error(ctx, ERR_TYPECHECK(
				b->items[j].tag, TYPE_NUMBER));
		
		double result = get_number(a->items[j]) *
			get_number(b->items[k]) -
			get_number(a->items[k]) *
			get_number(b->items[j]);
		
		list_push(out, make_number(result), ctx);
	}
	
	return make_list(out);
}

static value_t std_text_len(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	return make_number(get_text(args[0])->len);
}

static value_t std_text_cmp(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_TEXT);
	const text_t* a = get_text(args[0]);
	const text_t* b = get_text(args[1]);
	return make_text(text_cmp(a, b));
}

static value_t std_text_chars(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	const text_t* text = get_text(args[0]);
	list_t* chars = list_new(ctx);
	for (size_t i = 0; i < text->len; i++)
		list_push(chars, make_text(text_new(
			text->buffer + i, 1, ctx)), ctx);
	return make_list(chars);
}

static value_t std_text_ascii(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	const text_t* text = get_text(args[0]);
	list_t* ascii = list_new(ctx);
	for (size_t i = 0; i < text->len; i++)
		list_push(ascii, make_number(text->buffer[i]), ctx);
	return make_list(ascii);
}

static value_t std_text_slice(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_NUMBER);
	typecheck(2, TYPE_NUMBER);
	
	const text_t* text = get_text(args[0]);
	double start = get_number(args[1]);
	if (start < 0)
		start = text->len + start;
	if (start >= text->len)
		error(ctx, ERR_INDEX(args[1]));
		
	double end = get_number(args[2]);
	if (end < 0)
		end = text->len + end;
	if (end >= text->len)
		error(ctx, ERR_INDEX(args[1]));
	
	text_t* new_text = text_new(
		text->buffer + (size_t)start,
		end - start,
		ctx);
	new_text->buffer[new_text->len] = '\0';
	return make_text(new_text);
}

static value_t std_text_split(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_TEXT);
	const text_t* input = get_text(args[0]);
	const text_t* sep = get_text(args[1]);
	
	list_t* parts = list_new(ctx);
	text_t* buffer = text_new(NULL, 0, ctx);
	
	for (size_t i = 0; i < input->len; i++) {
		if (strncmp(
			(const char*)input->buffer + i,
			(const char*)sep->buffer,
			sep->len) == 0 ||
			i == input->len - 1)
		{
			if (i == input->len - 1)
				text_push(buffer, input->buffer[i], ctx);
			
			list_push(parts,
				make_text(text_copy(buffer, ctx)), ctx);
			buffer = text_new(NULL, 0, ctx);
			continue;
		}
		
		text_push(buffer, input->buffer[i], ctx);
	}
	
	return make_list(parts);
}

static value_t std_text_to_upper(
	value_t* args, lur_t* ctx) 
{
	typecheck(0, TYPE_TEXT);
	const text_t* original = get_text(args[0]);
	text_t* text = text_copy(original, ctx);
	for (size_t i = 0; i < text->len; i++)
		text->buffer[i] = toupper(original->buffer[i]);
	return make_text(text);
}

static value_t std_text_to_lower(
	value_t* args, lur_t* ctx) 
{
	typecheck(0, TYPE_TEXT);
	const text_t* original = get_text(args[0]);
	text_t* text = text_copy(original, ctx);
	for (size_t i = 0; i < text->len; i++)
		text->buffer[i] = tolower(original->buffer[i]);
	return make_text(text);
}

static value_t std_text_is_lower(
	value_t* args, lur_t* ctx) 
{
	typecheck(0, TYPE_TEXT);
	const text_t* text = get_text(args[0]);
	for (size_t i = 0; i < text->len; i++)
		if (!islower(text->buffer[i]))
			return make_bool(false);
	return make_bool(true);
}

static value_t std_text_is_upper(
	value_t* args, lur_t* ctx) 
{
	typecheck(0, TYPE_TEXT);
	const text_t* text = get_text(args[0]);
	for (size_t i = 0; i < text->len; i++)
		if (!isupper(text->buffer[i]))
			return make_bool(false);
	return make_bool(true);
}

static value_t std_text_starts_with(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_TEXT);
	const text_t* text = get_text(args[0]);
	const text_t* start = get_text(args[1]);
	if (start->len > text->len) return make_bool(false);
	for (size_t i = 0; i < start->len; i++)
		if (text->buffer[i] != start->buffer[i])
			return make_bool(false);
			
	return make_bool(true);
}

static value_t std_text_ends_with(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_TEXT);
	const text_t* text = get_text(args[0]);
	const text_t* end = get_text(args[1]);
	if (end->len > text->len) return make_bool(false);
	for (size_t i = 0; i < end->len; i++)
		if (text->buffer[text->len - end->len + i] !=
			end->buffer[i])
			return make_bool(false);
			
	return make_bool(true);
}

static value_t std_text_trim_left(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_TEXT);
	const text_t* text = get_text(args[0]);
	uint8_t* cur = text->buffer;
	
	while (cur != text->buffer + text->len)
		if (isspace(*cur)) cur++;
		else break;
	
	return make_text(text_new(
		cur, strlen((const char*)cur), ctx));
}

static value_t std_text_trim_right(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_TEXT);
	const text_t* text = get_text(args[0]);
	uint8_t* cur = text->buffer + text->len;
	
	while (cur != text->buffer)
		if (isspace(*cur)) cur--;
		else break;
	
	size_t len = cur - text->buffer - 1;
	return make_text(text_new(text->buffer, len, ctx));
}

static value_t std_text_trim(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	value_t text = args[0];
	text = std_text_trim_left(&text, ctx);
	text = std_text_trim_right(&text, ctx);
	return text;
}

static value_t std_text_left_pad(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_NUMBER);
	typecheck(2, TYPE_TEXT);
	const text_t* text = get_text(args[0]);
	int width = get_number(args[1]);
	const text_t* pattern = get_text(args[2]);
	
	int n = (width - text->len) / pattern->len;
	for (int i = 0; i < n; i++)
		text = text_concat(pattern, text, ctx);
	
	return make_text(text);
}

static value_t std_text_right_pad(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_NUMBER);
	typecheck(2, TYPE_TEXT);
	const text_t* text = get_text(args[0]);
	int width = get_number(args[1]);
	const text_t* pattern = get_text(args[2]);
	
	int n = (width - text->len) / pattern->len;
	for (int i = 0; i < n; i++)
		text = text_concat(text, pattern, ctx);
	
	return make_text(text);
}

static value_t std_text_pad(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_NUMBER);
	typecheck(2, TYPE_TEXT);
	const text_t* text = get_text(args[0]);
	int width = get_number(args[1]);
	const text_t* pattern = get_text(args[2]);
	
	int n = (width - text->len) / pattern->len;
	for (int i = 0; i < n / 2; i++)
		text = text_concat(pattern, text, ctx);
		
	for (int i = 0; i < n / 2; i++)
		text = text_concat(text, pattern, ctx);
	
	return make_text(text);
}

static value_t std_text_find(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_TEXT);
	typecheck(2, TYPE_NUMBER);
	const text_t* text = get_text(args[0]);
	const text_t* find = get_text(args[1]);
	size_t start = (size_t)get_number(args[2]);
	
	for (size_t i = start; i < text->len; i++) {
		bool match = i <= text->len - find->len &&
			strncmp((const char*)find->buffer,
				(const char*)text->buffer + i,
				find->len) == 0;
				
		if (match)
			return make_number(i);
	}
	
	return make_null();
}

static value_t std_text_rep_all(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_TEXT);
	typecheck(2, TYPE_TEXT);
	const text_t* input = get_text(args[0]);
	const text_t* find = get_text(args[1]);
	const text_t* rep = get_text(args[2]);
	text_t* output = text_new(NULL, 0, ctx);
	
	for (size_t i = 0; i < input->len; i++) {
		bool match = i <= input->len - find->len &&
			strncmp((const char*)find->buffer,
				(const char*)input->buffer + i,
				find->len) == 0;
				
		if (match) {
			i += find->len - 1;
			output = text_concat(output, rep, ctx);
			continue;
		}
		
		text_push(output, input->buffer[i], ctx);
	}
	
	return make_text(output);
}

static value_t std_text_edit_dist(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_TEXT);
	return make_number(text_edit_distance(
		get_text(args[0]), get_text(args[1])));
}

static value_t std_text_rand_text(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_TEXT);
	size_t length = (size_t)get_number(args[0]);
	const text_t* charset = get_text(args[1]);
	text_t* text = text_new(NULL, 0, ctx);
	for (size_t i = 0; i < length; i++)
		text_push(text,
			charset->buffer[
				(size_t)math_rand(0, charset->len)],
			ctx);
	return make_text(text);
}

static value_t std_list_len(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	return make_number(get_list(args[0])->len);
}

static value_t std_list_get(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	list_t* list = get_list(args[0]);
	size_t index = list_convert_index(
		list, get_number(args[1]), ctx);
	return list->items[(size_t)index];
}

static value_t std_list_set(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	list_t* list = get_list(args[0]);
	size_t index = list_convert_index(
		list, get_number(args[1]), ctx);
	list->items[index] = args[2];
	return make_null();
}

static value_t std_list_insert(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	typecheck(1, TYPE_NUMBER);
	list_t* list = get_list(args[0]);
	double index = get_number(args[1]);
	if (index < 0)
		index = list->len + 1 + index;
	if (index > list->len)
		error(ctx, ERR_INDEX(args[1]));
	list_insert(list, (size_t)index, args[2], ctx);
	return make_null();
}

static value_t std_list_del(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	typecheck(1, TYPE_NUMBER);
	list_t* list = get_list(args[0]);
	size_t index = list_convert_index(
		list, get_number(args[1]), ctx);
	list_del(list, index, ctx);
	return make_null();
}

static value_t std_list_pop(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	return list_pop(get_list(args[0]), ctx);
}

static value_t std_list_head(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	const list_t* list = get_list(args[0]);
	if (list->len == 0)
		error(ctx, ERR_INDEX(make_number(0)));
	return list->items[0];
}

static value_t std_list_tail(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	const list_t* list = get_list(args[0]);
	if (list->len == 0)
		error(ctx, ERR_INDEX(make_number(0)));
	list_t* tail = list_copy(list, ctx);
	list_del(tail, 0, ctx);
	return make_list(tail);
}

static value_t std_list_last(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	const list_t* list = get_list(args[0]);
	if (list->len == 0)
		error(ctx, ERR_INDEX(make_number(0)));
	return list->items[list->len - 1];
}

static value_t std_list_fill(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_FREF);
	size_t n = get_number(args[0]);
	const fref_t* fref = get_fref(args[1]);
	list_t* list = list_new(ctx);
	for (size_t i = 0; i < n; i++) {
		value_t args[] = { make_number(i) };
		value_t item = call_function(ctx, fref, args, 1);
		list_push(list, item, ctx);
	}
	return make_list(list);
}

static value_t std_list_repeat(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	typecheck(1, TYPE_NUMBER);
	const list_t* list = get_list(args[0]);
	size_t n = (size_t)get_number(args[1]);
	return make_list(list_repeat(list, n, ctx));
}

static value_t std_list_count(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	const list_t* list = get_list(args[0]);
	size_t count = 0;
	for (size_t i = 0; i < list->len; i++)
		if (value_eq(list->items[i], args[1]))
			count++;
	return make_number(count);
}

static value_t std_list_contains(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	return make_bool(list_contains(
		get_list(args[0]), args[1]));
}

static value_t std_list_find(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	typecheck(2, TYPE_NUMBER);
	list_t* list = get_list(args[0]);
	value_t value = args[1];
	size_t start = (size_t)get_number(args[2]);
		
	if (start >= list->len) return make_null();
	for (size_t i = start; i < list->len; i++) {
		if (value_eq(list->items[i], value))
			return make_number(i);
	}
		
	return make_null();
}

static value_t std_list_iter(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	typecheck(1, TYPE_FREF);
	const list_t* list = get_list(args[0]);
	const fref_t* fref = get_fref(args[1]);
	
	for (size_t i = 0; i < list->len; i++) {
		value_t args[] = { list->items[i] };
		call_function(ctx, fref, args, 1);
	}
	
	return make_null();
}

static value_t std_list_iteri(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	typecheck(1, TYPE_FREF);
	const list_t* list = get_list(args[0]);
	const fref_t* fref = get_fref(args[1]);
	
	for (size_t i = 0; i < list->len; i++) {
		value_t args[] = { make_number(i), list->items[i] };
		call_function(ctx, fref, args, 2);
	}
	
	return make_null();
}

static value_t std_list_map(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	typecheck(1, TYPE_FREF);
	const list_t* list = get_list(args[0]);
	const fref_t* fref = get_fref(args[1]);
	list_t* mapped = list_new(ctx);
	
	for (size_t i = 0; i < list->len; i++) {
		value_t args[] = { list->items[i] };
		value_t result = call_function(ctx, fref, args, 1);
		list_push(mapped, result, ctx);
	}
	
	return make_list(mapped);
}

static value_t std_list_filter(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	typecheck(1, TYPE_FREF);
	const list_t* list = get_list(args[0]);
	const fref_t* fref = get_fref(args[1]);
	list_t* filtered = list_new(ctx);
	
	for (size_t i = 0; i < list->len; i++) {
		value_t args[] = { list->items[i] };
		value_t result = call_function(ctx, fref, args, 1);
		if (result.tag == TYPE_BOOL && get_bool(result))
			list_push(filtered, list->items[i], ctx);
	}
	
	return make_list(filtered);
}

static value_t std_list_fold(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	typecheck(2, TYPE_FREF);
	const list_t* list = get_list(args[0]);
	value_t result = args[1];
	const fref_t* fref = get_fref(args[2]);
	
	for (size_t i = 0; i < list->len; i++) {
		value_t args[] = { result, list->items[i] };
		result = call_function(ctx, fref, args, 2);
	}
	
	return result;
}

static value_t std_list_flat(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	return make_list(list_flatten(get_list(args[0]), ctx));
}

static value_t std_list_dedup(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	list_t* list = get_list(args[0]);
	list_t* result = list_new(ctx);
	for (size_t i = 0; i < list->len; i++)
		if (!list_contains(result, list->items[i]))
			list_push(result, list->items[i], ctx);
	return make_list(result);
}

static value_t std_list_sum(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	const list_t* list = get_list(args[0]);
	
	double result = 0.0;
	for (size_t i = 0; i < list->len; i++) {
		if (list->items[i].tag != TYPE_NUMBER)
			error(ctx, ERR_TYPECHECK(list->items[i].tag,
				TYPE_NUMBER));
		result += get_number(list->items[i]);
	}
	
	return make_number(result);
}

static value_t std_list_average(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	const list_t* list = get_list(args[0]);
	
	double result = 0.0;
	for (size_t i = 0; i < list->len; i++) {
		if (list->items[i].tag != TYPE_NUMBER)
			error(ctx, ERR_TYPECHECK(list->items[i].tag,
				TYPE_NUMBER));
		result += get_number(list->items[i]);
	}
	result /= list->len;
	
	return make_number(result);
}

static value_t std_list_mean(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	const list_t* list = get_list(args[0]);
	
	double result = 0.0;
	if (list->len % 2 == 1) {
		size_t i = list->len / 2;
		if (list->items[i].tag != TYPE_NUMBER)
			error(ctx, ERR_TYPECHECK(list->items[i].tag,
				TYPE_NUMBER));
		result = get_number(list->items[i]);
	} else {
		size_t a = list->len / 2;
		size_t b = list->len / 2 - 1;
		if (list->items[a].tag != TYPE_NUMBER)
			error(ctx, ERR_TYPECHECK(list->items[a].tag,
				TYPE_NUMBER));
		if (list->items[b].tag != TYPE_NUMBER)
			error(ctx, ERR_TYPECHECK(list->items[b].tag,
				TYPE_NUMBER));
		result = (get_number(list->items[a]) +
			get_number(list->items[b])) / 2;
	}
	
	return make_number(result);
}

static value_t std_list_any(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	typecheck(1, TYPE_FREF);
	const list_t* list = get_list(args[0]);
	const fref_t* fref = get_fref(args[1]);
	
	for (size_t i = 0; i < list->len; i++) {
		value_t args[] = { list->items[i] };
		value_t result = call_function(ctx, fref, args, 1);
		if (result.tag == TYPE_BOOL && get_bool(result))
			return make_bool(true);
	}
	
	return make_bool(false);
}

static value_t std_list_all(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	typecheck(1, TYPE_FREF);
	const list_t* list = get_list(args[0]);
	const fref_t* fref = get_fref(args[1]);
	
	for (size_t i = 0; i < list->len; i++) {
		value_t args[] = { list->items[i] };
		value_t result = call_function(ctx, fref, args, 1);
		if (result.tag == TYPE_BOOL && !get_bool(result))
			return make_bool(false);
	}
	
	return make_bool(true);
}

static value_t std_list_sort(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	return make_list(list_sort(get_list(args[0]), ctx));
}

static value_t std_list_swap(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	typecheck(1, TYPE_NUMBER);
	typecheck(2, TYPE_NUMBER);
	const list_t* input = get_list(args[0]);
	list_t* output = list_copy(input, ctx);
	
	size_t a = list_convert_index(
		input, get_number(args[1]), ctx);
	
	size_t b = list_convert_index(
		input, get_number(args[2]), ctx);
	
	list_swap(output, a, b);
	return make_list(output);
}

static value_t std_list_join(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	return make_text(list_join(get_list(args[0]), ctx));
}

static value_t std_list_zip(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	const list_t* lists = get_list(args[0]);
	list_t* zip = list_new(ctx);
	
	size_t biggest = 0;
	for (size_t i = 0; i < lists->len; i++) {
		if (lists->items[i].tag != TYPE_LIST)
			error(ctx, ERR_TYPECHECK(
				lists->items[i].tag, TYPE_LIST));
		
		const list_t* list = get_list(lists->items[i]);
		biggest = (list->len > biggest) ? list->len : biggest;
	}
	
	for (size_t i = 0; i < biggest; i++) {
		for (size_t j = 0; j < lists->len; j++) {
			const list_t* list = get_list(lists->items[j]);
			if (i < list->len)
				list_push(zip, list->items[i], ctx);
		}
	}
	
	return make_list(zip);
}

static value_t std_list_chunk(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_LIST);
	typecheck(1, TYPE_NUMBER);
	list_t* original = get_list(args[0]);
	size_t n = (size_t)get_number(args[1]);
	list_t* chunks = list_new(ctx);
	
	for (size_t i = 0; i < original->len; i += n) {
		list_t* chunk = list_new(ctx);
		for (size_t j = 0; j < n; j++) {
			list_push(chunk, original->items[i + j], ctx);
			if (i + j == original->len - 1)
				break;
		}
		list_push(chunks, make_list(chunk), ctx);
	}
	
	return make_list(chunks);
}

static value_t std_list_rand_item(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_LIST);
	list_t* list = get_list(args[0]);
	return list->items[(size_t)math_rand(0, list->len)];
}

static value_t std_map_len(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_MAP);
	return make_number(get_map(args[0])->len);
}

static value_t std_map_get(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_MAP);
	const map_t* map = get_map(args[0]);
	value_t value;
	if (!map_get(map, args[1], &value, ctx))
		error(ctx, ERR_INDEX(args[1]));
	return value;
}

static value_t std_map_set(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_MAP);
	map_set(get_map(args[0]), args[1], args[2], ctx);
	return make_null();
}

static value_t std_map_has_key(value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_MAP);
	const map_t* map = get_map(args[0]);
	
	for (size_t i = 0; i < map->cap; i++) {
		const map_entry_t* entry = &map->entries[i];
		if (entry->key.tag == TYPE_NULL) continue;
		if (value_eq(entry->key, args[1]))
			return make_bool(true);
	}
	
	return make_bool(false);
}

static value_t std_map_has_value(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_MAP);
	const map_t* map = get_map(args[0]);
	
	for (size_t i = 0; i < map->cap; i++) {
		const map_entry_t* entry = &map->entries[i];
		if (entry->key.tag == TYPE_NULL) continue;
		if (value_eq(entry->value, args[1]))
			return make_bool(true);
	}
	
	return make_bool(false);
}

static value_t std_map_keys(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_MAP);
	const map_t* map = get_map(args[0]);
	list_t* keys = list_new(ctx);
	
	for (size_t i = 0; i < map->cap; i++) {
		const map_entry_t* entry = &map->entries[i];
		if (entry->key.tag == TYPE_NULL) continue;
		list_push(keys, entry->key, ctx);
	}
	
	return make_list(keys);
}

static value_t std_map_values(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_MAP);
	const map_t* map = get_map(args[0]);
	list_t* values = list_new(ctx);
	
	for (size_t i = 0; i < map->cap; i++) {
		const map_entry_t* entry = &map->entries[i];
		if (entry->key.tag == TYPE_NULL) continue;
		list_push(values, entry->value, ctx);
	}
	
	return make_list(values);
}

static value_t std_map_kv_pairs(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_MAP);
	const map_t* map = get_map(args[0]);
	list_t* pairs = list_new(ctx);
	
	for (size_t i = 0; i < map->cap; i++) {
		const map_entry_t* entry = &map->entries[i];
		if (entry->key.tag == TYPE_NULL) continue;
		list_t* pair = list_new(ctx);
		list_push(pair, entry->key, ctx);
		list_push(pair, entry->value, ctx);
		list_push(pairs, make_list(pair), ctx);
	}
	
	return make_list(pairs);
}

static value_t std_map_from_kv_pairs(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_LIST);
	const list_t* pairs = get_list(args[0]);
	map_t* map = map_new(ctx);
	
	for (size_t i = 0; i  < pairs->len; i++) {
		if (pairs->items[i].tag != TYPE_LIST)
			error(ctx, ERR_TYPECHECK(
				pairs->items[i].tag, TYPE_LIST));
		
		const list_t* pair = get_list(pairs->items[i]);
		if (pair->len != 2)
			continue;
			
		map_set(map, pair->items[0], pair->items[1], ctx);
	}
	
	return make_map(map);
}

static value_t std_map_iter(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_MAP);
	typecheck(1, TYPE_FREF);
	const map_t* map = get_map(args[0]);
	const fref_t* fref = get_fref(args[1]);
	
	for (size_t i = 0; i < map->cap; i++) {
		const map_entry_t* entry = &map->entries[i];
		if (entry->key.tag == TYPE_NULL) continue;
		value_t args[] = { entry->key, entry->value };
		call_function(ctx, fref, args, 2);
	}
	
	return make_null();
}

static value_t std_map_get_or_default(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_MAP);
	map_t* map = get_map(args[0]);
	value_t value;
	if (!map_get(map, args[1], &value, ctx))
		return args[2];
	return value;
}

static value_t std_io_print(value_t* args, lur_t* ctx) {
	value_print(args[0], ctx);
	fflush(stdout);
	return make_null();
}

static value_t std_io_print_ln(value_t* args, lur_t* ctx) {
	value_print(args[0], ctx);
	lur_printf("\n");
	return make_null();
}

static value_t std_io_read_ln(value_t* args, lur_t* ctx) {
	return make_text(io_read_stdin(ctx));
}

static value_t std_io_read(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	return make_text(io_read(get_text(args[0]), ctx));
}

static value_t std_io_write(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_TEXT);
	io_write(get_text(args[0]), get_text(args[1]), ctx);
	return make_null();
}

static value_t std_io_append(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_TEXT);
	io_append(get_text(args[0]), get_text(args[1]), ctx);
	return make_null();
}

static value_t std_io_size(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	return make_number(
		io_file_size(get_text(args[0]), ctx));
}

static value_t std_io_list_dir(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	const text_t* path = get_text(args[0]);
	list_t* files = list_new(ctx);
	
	DIR *dir;
	struct dirent *ent;
	if ((dir = opendir((const char*)path->buffer)) != NULL) {
		while ((ent = readdir(dir)) != NULL) {
		 	text_t* file = text_lit(ent->d_name, ctx);
		 	list_push(files, make_text(file), ctx);
		}
		closedir(dir);
		return make_list(files);
	}
	
	error(ctx, ERR_OPEN_FAILED(path));
	return make_null();
}

static value_t std_io_del(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	remove((char*)get_text(args[0])->buffer);
	return make_null();
}

#undef typecheck

static void stdvar_add(
	lur_t* ctx, const char* name, value_t value)
{
	#if LUR_DEBUG_PRINT_STDLIB
	if (ctx->std_map_name)
		printf("stdlib: %s.%s\n",
			ctx->std_map_name, name);
	else
		printf("stdlib: %s\n", name);
	#endif
	
	if (!ctx->std_map) {
		map_set(ctx->vm.globals,
			make_text(text_lit(name, ctx)),
			value,
			ctx);
		return;
	}
	
	map_set(ctx->std_map,
		make_text(text_lit(name, ctx)),
		value,
		ctx);
}

static void stdfun_add(
	lur_t* ctx, const char* name, 
	uint8_t argc, syscall_fn_t fn)
{
	func_t* syscall = func_new(ctx);
	syscall->name = text_lit(name, ctx);
	syscall->argc = argc;
	syscall->syscall = fn;
	
	stdvar_add(ctx, name,
		make_fref(fref_new(syscall, ctx)));
}

static void std_set_map(
	lur_t* ctx, const char* name)
{
	if (!name) {
		ctx->std_map = NULL;
		ctx->std_map_name = NULL;
		return;
	}
	
	ctx->std_map = map_new(ctx);
	ctx->std_map_name = name;
	
	map_set(ctx->vm.globals,
		make_text(text_lit(name, ctx)),
		make_map(ctx->std_map),
		ctx);
}

void stdlib_load(lur_t* ctx) {
	gc_pause(ctx);
	
	stdvar_add(ctx, "__VERSION__",
		make_text(text_lit(LUR_VERSION, ctx)));
	stdvar_add(ctx, "__G__", make_map(ctx->vm.globals));
	stdfun_add(ctx, "load", 1, std_load);
	stdfun_add(ctx, "eval", 1, std_eval);
	stdfun_add(ctx, "system", 1, std_system);
	stdfun_add(ctx, "error", 1, std_error);
	stdfun_add(ctx, "type_of", 1, std_type_of);
	stdfun_add(ctx, "to_num", 1, std_to_num);
	stdfun_add(ctx, "to_text", 1, std_to_text);
	stdfun_add(ctx, "repeat", 2, std_repeat);
	stdfun_add(ctx, "while", 1, std_while);
	stdfun_add(ctx, "call", 1, std_call);
	stdfun_add(ctx, "range", 2, std_range);
	stdfun_add(ctx, "range_inc", 2, std_range_inc);
	stdfun_add(ctx, "enum", 1, std_enum);
	stdfun_add(ctx, "exit", 1, std_exit);
	stdfun_add(ctx, "assert", 1, std_assert);
	stdfun_add(ctx, "timestamp", 0, std_timestamp);
	stdfun_add(ctx, "clock", 0, std_clock);
	
	std_set_map(ctx, "Math");
	stdvar_add(ctx, "PI", make_number(M_PI));
	stdvar_add(ctx, "E", make_number(M_E));
	stdvar_add(ctx, "INF", make_number(INFINITY));
	stdvar_add(ctx, "NAN", make_number(NAN));
	stdfun_add(ctx, "eqf", 3, std_math_eqf);
	stdfun_add(ctx, "shl", 2, std_math_shl);
	stdfun_add(ctx, "shr", 2, std_math_shr);
	stdfun_add(ctx, "band", 2, std_math_band);
	stdfun_add(ctx, "bor", 2, std_math_bor);
	stdfun_add(ctx, "xor", 2, std_math_xor);
	stdfun_add(ctx, "bnot", 1, std_math_bnot);
	stdfun_add(ctx, "log", 2, std_math_log);
	stdfun_add(ctx, "sqrt", 1, std_math_sqrt);
	stdfun_add(ctx, "squared", 1, std_math_squared);
	stdfun_add(ctx, "abs", 1, std_math_abs);
	stdfun_add(ctx, "min", 2, std_math_min);
	stdfun_add(ctx, "max", 2, std_math_max);
	stdfun_add(ctx, "clip", 3, std_math_clip);
	stdfun_add(ctx, "lerp", 3, std_math_lerp);
	stdfun_add(ctx, "floor", 1, std_math_floor);
	stdfun_add(ctx, "ceil", 1, std_math_ceil);
	stdfun_add(ctx, "round", 1, std_math_round);
	stdfun_add(ctx, "rad", 1, std_math_rad);
	stdfun_add(ctx, "deg", 1, std_math_deg);
	stdfun_add(ctx, "sin", 1, std_math_sin);
	stdfun_add(ctx, "cos", 1, std_math_cos);
	stdfun_add(ctx, "tan", 1, std_math_tan);
	stdfun_add(ctx, "asin", 1, std_math_asin);
	stdfun_add(ctx, "acos", 1, std_math_acos);
	stdfun_add(ctx, "atan", 1, std_math_atan);
	stdfun_add(ctx, "atan2", 2, std_math_atan2);
	stdfun_add(ctx, "sinh", 1, std_math_sinh);
	stdfun_add(ctx, "cosh", 1, std_math_cosh);
	stdfun_add(ctx, "tanh", 1, std_math_tanh);
	stdfun_add(ctx, "asinh", 1, std_math_asinh);
	stdfun_add(ctx, "acosh", 1, std_math_acosh);
	stdfun_add(ctx, "atanh", 1, std_math_atanh);
	stdfun_add(ctx, "even", 1, std_math_even);
	stdfun_add(ctx, "odd", 1, std_math_odd);
	stdfun_add(ctx, "hex", 1, std_math_hex);
	stdfun_add(ctx, "bin", 1, std_math_bin);
	stdfun_add(ctx, "hash", 1, std_math_hash);
	stdfun_add(ctx, "rand", 2, std_math_rand);
	stdfun_add(ctx, "srand", 1, std_math_srand);
	stdfun_add(ctx, "size", 1, std_math_size);
	stdfun_add(ctx, "norm", 1, std_math_norm);
	stdfun_add(ctx, "dot", 2, std_math_dot);
	stdfun_add(ctx, "cross", 2, std_math_cross);
	std_set_map(ctx, NULL);
	
	std_set_map(ctx, "Text");
	stdvar_add(ctx, "LETTERS", make_text(
		text_lit("abcdefghijklmnopqrstuvwxyz", ctx)));
	stdvar_add(ctx, "DIGITS", make_text(
		text_lit("0123456789", ctx)));
	stdvar_add(ctx, "PUNCTUATION", make_text(
		text_lit("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~", ctx)));
	stdvar_add(ctx, "WHITESPACE", make_text(
		text_lit(" \t\n\r", ctx)));
	stdfun_add(ctx, "len", 1, std_text_len);
	stdfun_add(ctx, "cmp", 2, std_text_cmp);
	stdfun_add(ctx, "chars", 1, std_text_chars);
	stdfun_add(ctx, "ascii", 1, std_text_ascii);
	stdfun_add(ctx, "slice", 3, std_text_slice);
	stdfun_add(ctx, "split", 2, std_text_split);
	stdfun_add(ctx, "to_upper", 1, std_text_to_upper);
	stdfun_add(ctx, "to_lower", 1, std_text_to_lower);
	stdfun_add(ctx, "is_upper", 1, std_text_is_upper);
	stdfun_add(ctx, "is_lower", 1, std_text_is_lower);
	stdfun_add(ctx, "starts_with", 2, std_text_starts_with);
	stdfun_add(ctx, "ends_with", 2, std_text_ends_with);
	stdfun_add(ctx, "trim_left", 1, std_text_trim_left);
	stdfun_add(ctx, "trim_right", 1, std_text_trim_right);
	stdfun_add(ctx, "trim", 1,  std_text_trim);
	stdfun_add(ctx, "left_pad", 3, std_text_left_pad);
	stdfun_add(ctx, "right_pad", 3, std_text_right_pad);
	stdfun_add(ctx, "pad", 3,  std_text_pad);
	stdfun_add(ctx, "find", 3, std_text_find);
	stdfun_add(ctx, "rep_all", 3, std_text_rep_all);
	stdfun_add(ctx, "edit_dist", 2, std_text_edit_dist);
	stdfun_add(ctx, "rand_text", 2,  std_text_rand_text);
	std_set_map(ctx, NULL);
	
	std_set_map(ctx, "List");
	stdfun_add(ctx, "len", 1, std_list_len);
	stdfun_add(ctx, "get", 2, std_list_get);
	stdfun_add(ctx, "set", 3, std_list_set);
	stdfun_add(ctx, "insert", 3, std_list_insert);
	stdfun_add(ctx, "del", 2, std_list_del);
	stdfun_add(ctx, "pop", 1, std_list_pop);
	stdfun_add(ctx, "head", 1, std_list_head);
	stdfun_add(ctx, "tail", 1, std_list_tail);
	stdfun_add(ctx, "last", 1, std_list_last);
	stdfun_add(ctx, "fill", 2, std_list_fill);
	stdfun_add(ctx, "repeat", 2, std_list_repeat);
	stdfun_add(ctx, "count", 2, std_list_count);
	stdfun_add(ctx, "contains", 2, std_list_contains);
	stdfun_add(ctx, "find", 3, std_list_find);
	stdfun_add(ctx, "iter", 2, std_list_iter);
	stdfun_add(ctx, "iteri", 2, std_list_iteri);
	stdfun_add(ctx, "map", 2, std_list_map);
	stdfun_add(ctx, "filter", 2, std_list_filter);
	stdfun_add(ctx, "fold", 3, std_list_fold);
	stdfun_add(ctx, "flat", 1, std_list_flat);
	stdfun_add(ctx, "dedup", 1, std_list_dedup);
	stdfun_add(ctx, "sum", 1, std_list_sum);
	stdfun_add(ctx, "average", 1, std_list_average);
	stdfun_add(ctx, "mean", 1, std_list_mean);
	stdfun_add(ctx, "any", 2, std_list_any);
	stdfun_add(ctx, "all", 2, std_list_all);
	stdfun_add(ctx, "sort", 1, std_list_sort);
	stdfun_add(ctx, "swap", 3, std_list_swap);
	stdfun_add(ctx, "join", 1, std_list_join);
	stdfun_add(ctx, "zip", 1, std_list_zip);
	stdfun_add(ctx, "chunk", 2, std_list_chunk);
	stdfun_add(ctx, "rand_item", 1, std_list_rand_item);
	std_set_map(ctx, NULL);
	
	std_set_map(ctx, "Map");
	stdfun_add(ctx, "len", 1, std_map_len);
	stdfun_add(ctx, "get", 2, std_map_get);
	stdfun_add(ctx, "set", 3, std_map_set);
	stdfun_add(ctx, "has_key", 2, std_map_has_key);
	stdfun_add(ctx, "has_value", 2, std_map_has_value);
	stdfun_add(ctx, "keys", 1, std_map_keys);
	stdfun_add(ctx, "values", 1, std_map_values);
	stdfun_add(ctx, "kv_pairs", 1, std_map_kv_pairs);
	stdfun_add(ctx, "from_kv_pairs", 1,
		std_map_from_kv_pairs);
	stdfun_add(ctx, "iter", 2, std_map_iter);
	stdfun_add(ctx, "get_or_default", 3,
		std_map_get_or_default);
	std_set_map(ctx, NULL);
	
	std_set_map(ctx, "IO");
	stdfun_add(ctx, "print", 1, std_io_print);
	stdfun_add(ctx, "print_ln", 1, std_io_print_ln);
	stdfun_add(ctx, "read_ln", 0, std_io_read_ln);
	stdfun_add(ctx, "read", 1, std_io_read);
	stdfun_add(ctx, "write", 2, std_io_write);
	stdfun_add(ctx, "append", 2, std_io_append);
	stdfun_add(ctx, "size", 1, std_io_size);
	stdfun_add(ctx, "list_dir", 1, std_io_list_dir);
	stdfun_add(ctx, "del", 1, std_io_del);
	std_set_map(ctx, NULL);
	
	gc_resume(ctx);
}

lur_t* lur_new(void) {
	lur_t* ctx = mem_alloc(NULL, sizeof(lur_t));
	if (setjmp(ctx->errjmp)) return NULL;
	
	ctx->mem.bytes = sizeof(lur_t);
	ctx->mem.total = ctx->mem.bytes;
	ctx->mem.objs = NULL;
	ctx->mem.gc_pause = 0;
	ctx->mem.marked = NULL;
	ctx->mem.nmarked = 0;
	ctx->mem.gc_cleaned = 0;
	ctx->mem.gc_cycles = 0;
	
	ctx->running = false;
	ctx->interpreter = false;
	ctx->std_map = NULL;
	ctx->std_map_name = NULL;
	
	vm_init(&ctx->vm, ctx);
	stdlib_load(ctx);
	return ctx;
}

void lur_free(lur_t* ctx) {
	if (!ctx) return;
	
	vm_free(&ctx->vm, ctx);
	
	obj_t* cur = ctx->mem.objs;
	while (cur) {
		obj_t* next = cur->next;
		obj_free(cur, ctx);
		cur = next;
	}
	
	free(ctx->mem.marked);
	
	#if LUR_DEBUG_PRINT_MEM_STATS
	lur_printf("memory leaked: %td bytes\n",
		ctx->mem.bytes - sizeof(lur_t));
	lur_printf("total allocated: %zu bytes\n",
		ctx->mem.total);
	lur_printf("gc objects cleaned: %zu\n",
		ctx->mem.gc_cleaned);
	lur_printf("gc cycles: %zu\n", ctx->mem.gc_cycles);
	#endif
	
	mem_free(ctx, ctx, sizeof(lur_t));
}

static void exec(
	lur_t* ctx, const text_t* src, const text_t* path)
{
	cl_init(&ctx->cl, ctx);
	cl_compile(&ctx->cl, src, path);
	cl_free(&ctx->cl);
	
	value_t fref = make_fref(fref_new(ctx->cl.func, ctx));
	*ctx->vm.sp++ = fref;
	ctx->running = true;
	vm_launch(&ctx->vm, fref, 0, false);
	ctx->running = false;
}

void lur_xstring(lur_t* ctx, const char* src) {
	if (!ctx) return;
	if (setjmp(ctx->errjmp)) return;
	exec(ctx,
		text_lit(src, ctx), text_lit("<main>", ctx));
}

void lur_xfile(lur_t* ctx, const char* path) {
	if (!ctx) return;
	if (setjmp(ctx->errjmp)) return;
	exec(ctx,
		io_read(text_lit(path, ctx), ctx),
		text_lit(path, ctx));
}

static void interpret(lur_t* ctx) {
	puts(LUR_VERSION);
	ctx->interpreter = true;
	for (;;) {
		lur_printf(":: ");
		lur_xstring(ctx,
			(const char*)io_read_stdin(ctx)->buffer);
	}
}

int main(int argc, char* argv[]) {
	lur_t* ctx = lur_new();
	if (argc == 2) lur_xfile(ctx, argv[1]);
	else interpret(ctx);
	
	lur_free(ctx);
	return EXIT_SUCCESS;
}
