#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#define LUR_VERSION "lur 1.0"
#define LUR_VERSION_MAJ 1
#define LUR_VERSION_MIN 0
#define LUR_VERSION_PATCH 0

#define LUR_REPL_GREETING \
	"use 'help: \"all\"' to view the documentation."

#define LUR_DEBUG_ASSERTS 1
#define LUR_DEBUG_PRINT_CODE 0
#define LUR_DEBUG_PRINT_DATA 0
#define LUR_DEBUG_PRINT_STACK 0
#define LUR_DEBUG_PRINT_TOKENS 0
#define LUR_DEBUG_PRINT_ALLOCS 0
#define LUR_DEBUG_PRINT_MEM_STATS 1
#define LUR_DEBUG_DISABLE_GC 0

#define lur_printf printf
#define lur_eprintf printf
#define lur_dprintf printf

#define lur_realloc realloc
#define lur_gc_realloc realloc
#define lur_gc_free free

#define INIT_ARRAY_CAP 8
#define MAP_MAX_LOAD 0.75
#define FIRST_GC_LIMIT (1024 * 1024)
#define GC_GROWTH_FACTOR 2

#define MAX_CODE SIZE_MAX
#define MAX_DATA UINT16_MAX
#define MAX_LINES SIZE_MAX
#define MAX_CFRAMES UINT16_MAX
#define MAX_STACK UINT16_MAX
#define MAX_JUMP UINT16_MAX
#define MAX_ARGS UINT8_MAX
#define MAX_VREFS UINT8_MAX
#define MAX_ARRAY_LIT_ITEMS UINT16_MAX
#define MAX_MAP_LIT_ITEMS UINT16_MAX
#define MAX_THREADS 4096
#define MAX_ERR_MSG 2048

#define ERR_OUT_OF_MEMORY \
	"out of memory"
#define ERR_LIMIT(name, limit) \
	"%s limit reached (%d)", (name), (limit)
#define ERR_RANGE(start, end, got) \
	"out of range (%d..=%d, got %d)", \
		(start), (end), (got)
#define ERR_READ_FAILED(path) \
	"failed to read file '%.*s'", (path)->len, (path)->buffer
#define ERR_WRITE_FAILED(path) \
	"failed to write to file '%.*s'", (path)->len, (path)->buffer
#define ERR_OPEN_FAILED(path, mode) \
	"failed to open file '%.*s' in mode '%.*s'", \
		(path)->len, (path)->buffer, \
		(mode)->len, (mode)->buffer
#define ERR_INVALID_HANDLE(handle) \
	"invalid file handle '%d'", (handle)
#define ERR_THREAD_SPAWN_FAILED(code) \
	"thread.spawn failed (%d)", (code)
#define ERR_INVALID_THREAD_ID(id) \
	"invalid thread id '%d'", (id)
#define ERR_SOCKET_CREATE(msg) \
	"creating socket failed: %s", (msg)
#define ERR_SOCKET_RESOLVE_FAILED(addr) \
	"failed to resolve hostname '%.*s'", \
		(addr->len), (addr->buffer)
#define ERR_SOCKET_CONNECT(addr, port, msg) \
	"failed to connect to %.*s:%d: %s", \
		(addr->len), (addr->buffer), (port), (msg)
#define ERR_SOCKET_BIND(addr, port, msg) \
	"failed to bind socket to %.*s:%d: %s", \
		(addr->len), (addr->buffer), (port), (msg)
#define ERR_SOCKET_LISTEN_FAILED(msg) \
	"socket.listen failed: %s", (msg)
#define ERR_SOCKET_ACCEPT_FAILED(msg) \
	"socket.accept failed: %s", (msg)
#define ERR_SOCKET_SEND_FAILED(msg) \
	"socket.send failed: %s", (msg)
#define ERR_SOCKET_RECV_FAILED(msg) \
	"socket.recv failed: %s", (msg)
#define ERR_UNKNOWN_CHAR(c) \
	"unknown character '%c' (ASCII %d)", (c), (c)
#define ERR_EXPECTED_EXPR(got) \
	"expected expression, got '%.*s'", \
		(got)->len, (got)->buffer
#define ERR_UNTERMINATED_COMMENT \
	"unterminated multi-line comment (expected '--]')"
#define ERR_UNTERMINATED_TEXT \
	"unterminated text (expected '\"')"
#define ERR_INVALID_ESCAPE(ch) \
	"invalid escape sequence: '\\%c'", (ch)
#define ERR_TYPECHECK(got, wanted) \
	"got type %s but expected %s", \
		type_name(got), type_name(wanted)
#define ERR_TYPECHECK_ARG(got, wanted, arg) \
	"got type %s for arg #%d but expected %s", \
		type_name(got), (arg), type_name(wanted)
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
#define ERR_NOT_IMPLEMENTED(what) \
	"%s not implemented", (what)
#define ERR_DIV_BY_ZERO \
	"division by zero"
#define ERR_REMAINDER_OF_DIV_BY_ZERO \
	"remainder of division by zero"
#define ERR_ASSERTION \
	"assertion failed"
	
#if LUR_INCLUDE_SDL
#define ERR_SDL(msg) \
	"%s: %s", (msg), SDL_GetError()
#define ERR_SDL_IMAGE(msg) \
	"%s: %s", (msg), IMG_GetError()
#define ERR_SDL_TTF(msg) \
	"%s: %s", (msg), TTF_GetError()
#define ERR_SDL_MIXER(msg) \
	"%s: %s", (msg), Mix_GetError()
#endif

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
	OP_NONE,
	OP_FALSE,
	OP_TRUE,
	OP_DATA,
	OP_NEWARRAY,
	OP_NEWMAP,
	OP_NEWFREF,
	OP_NEWVREF,
	OP_POP,
	OP_DUP,
	OP_MOVE,
	OP_SWAP,
	OP_GETLOC,
	OP_SETLOC,
	OP_GETVREF,
	OP_SETVREF,
	OP_GETFIELD,
	OP_SETFIELD,
	OP_GETGLOB,
	OP_SETGLOB,
	OP_ADDGLOB,
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
	OP_JIF,
	OP_JIFNOT,
	OP_CALL,
	OP_TAILCALL,
	OP_RET,
	OP_HALT,
} opcode_t;

typedef struct {
	const char* name;
	int len;
} opinfo_t;

static opinfo_t OPINFO[] = {
	[OP_NOP] = {"nop", 0},
	[OP_NONE] = {"none", 0},
	[OP_FALSE] = {"false", 0},
	[OP_TRUE] = {"true", 0},
	[OP_DATA] = {"data", 2},
	[OP_NEWARRAY] = {"newarray", 2},
	[OP_NEWMAP] = {"newmap", 2},
	[OP_NEWFREF] = {"newfref", 2},
	[OP_NEWVREF] = {"newvref", 0},
	[OP_POP] = {"pop", 0},
	[OP_DUP] = {"dup", 0},
	[OP_MOVE] = {"move", 2},
	[OP_SWAP] = {"swap", 0},
	[OP_GETLOC] = {"getloc", 2},
	[OP_SETLOC] = {"setloc", 2},
	[OP_GETVREF] = {"getvref", 2},
	[OP_SETVREF] = {"setvref", 2},
	[OP_GETFIELD] = {"getfield", 2},
	[OP_SETFIELD] = {"setfield", 2},
	[OP_GETGLOB] = {"getglob", 2},
	[OP_SETGLOB] = {"setglob", 2},
	[OP_ADDGLOB] = {"addglob", 2},
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
	[OP_JIF] = {"jif", 2},
	[OP_JIFNOT] = {"jifnot", 2},
	[OP_CALL] = {"call", 1},
	[OP_TAILCALL] = {"tailcall", 1},
	[OP_RET] = {"ret", 0},
	[OP_HALT] = {"halt", 0},
};

typedef enum {
	TYPE_NONE,
	TYPE_BOOL,
	TYPE_NUMBER,
	TYPE_TEXT,
	TYPE_ARRAY,
	TYPE_MAP,
	TYPE_FREF,
	TYPE_VREF,
	TYPE_FUNC,
} type_t;

static const char* TYPE_NAMES[] = {
	"None",
	"Bool",
	"Number",
	"Text",
	"Array",
	"Map",
	"Function",
};

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

#define text_lit(chars, ctx) \
	text_new((const uint8_t*)(chars), strlen(chars), ctx)

typedef struct {
	obj_t obj;
	value_t* items;
	size_t len;
} array_t;

typedef struct {
	value_t key;
	value_t value;
} map_entry_t;

typedef struct {
	obj_t obj;
	int64_t* indices;
	map_entry_t* entries;
	size_t cap;
	size_t len;
} map_t;

typedef struct func_t func_t;

typedef struct {
	obj_t obj;
	const func_t* func;
	struct vref_t** vrefs;
	size_t nvrefs;
} fref_t;

typedef struct vref_t {
	obj_t obj;
	int64_t index;
	value_t closed;
	struct vref_t* next;
} vref_t;

typedef struct lur_t lur_t;
typedef value_t (*syscall_fn_t)(value_t*, lur_t*);

typedef struct func_t {
	obj_t obj;
	const text_t* name;
	const text_t* src;
	uint8_t argc;
	syscall_fn_t syscall;
	uint8_t* code;
	size_t ncode;
	value_t* data;
	size_t ndata;
	int* lines;
	size_t nlines;
	size_t nvrefs;
} func_t;

#define make_none() \
	(value_t){TYPE_NONE, {.num = 0}}
#define make_bool(value) \
	(value_t){TYPE_BOOL, {.q = (value)}}
#define make_number(value) \
	(value_t){TYPE_NUMBER, {.num = (value)}}
#define make_text(value) \
	(value_t){TYPE_TEXT, {.obj = (obj_t*)(value)}}
#define make_array(value) \
	(value_t){TYPE_ARRAY, {.obj = (obj_t*)(value)}}
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
#define get_array(value) \
	((array_t*)((value).data.obj))
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
	pthread_t id;
	bool is_free;
} thread_t;

typedef struct {
	cframe_t* calls;
	size_t ncalls;
	cframe_t* fp;
	
	value_t* stack;
	size_t max_stack;
	value_t* sp;
	
	vref_t* open_vrefs;
	map_t* globals;
	thread_t threads[MAX_THREADS];
	lur_t* ctx;
} vm_t;

typedef enum {
	T_NAME,
	T_NONE,
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
	T_IS,
	T_DO,
	T_END,
	T_FUN,
	T_RETURN,
	T_IF,
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
	AST_ARRAY,
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
} ast_array_t;

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
		ast_array_t array;
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
static ast_node_t* ps_array(parser_t*);
static ast_node_t* ps_map(parser_t*);
static ast_node_t* ps_unary(parser_t*);
static ast_node_t* ps_binary(parser_t*);
static ast_node_t* ps_grouping(parser_t*);
static ast_node_t* ps_block(parser_t*);
static ast_node_t* ps_lambda(parser_t*);
static ast_node_t* ps_call(parser_t*);
static ast_node_t* ps_call_no_args(parser_t*);
static ast_node_t* ps_return(parser_t*);
static ast_node_t* ps_branch(parser_t*);
static ast_node_t* ps_dot(parser_t*);
static ast_node_t* ps_arrow(parser_t*);

static parse_rule_t RULES[] = {
	[T_NAME] = 
		{ps_name, NULL, PREC_NONE},
	[T_NONE] =
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
		{NULL, NULL, PREC_NONE},
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
		{ps_array, NULL, PREC_NONE},
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
		{NULL, ps_call_no_args, PREC_CALL},
	[T_IS] =
		{NULL, ps_binary, PREC_EQ},
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
	size_t next_gc;
	size_t nallocs;
	obj_t* objs;
	int gc_pause;
	obj_t** marked;
	size_t nmarked;
	size_t gc_cleaned;
	size_t gc_cycles;
} mem_t;

typedef struct {
	size_t memory_limit;
} lur_config_t;

typedef struct lur_t {
	lur_config_t cfg;
	comp_t cl;
	vm_t vm;
	size_t cur_vm;
	mem_t mem;
	
	bool running;
	bool interpreter;
	
	jmp_buf errjmp;
	
	map_t* help;
	
	map_t* std_map;
	const char* std_map_name;
	
	fref_t* thread_fref;
	array_t* thread_args;
	value_t thread_retval;
	
	array_t* args;
	
	text_t* error_msg;
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
static void text_eprint(const text_t*);
static void print_error_line(const text_t*, int);
static void print_stack_trace(const lur_t*);
static text_t* text_fmt(lur_t*, const char*, ...);

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
		ctx->vm.fp->ip - func->code];
	else line = ctx->cl.parser.prev.line;
	
	ctx->error_msg = text_fmt(ctx,
		"[%.*s:%d] error: %s\n",
			func->name->len, func->name->buffer,
			line, buffer);
	lur_eprintf("%s", ctx->error_msg->buffer);
	print_error_line(func->src, line);
	
	if (ctx->running) {
		print_stack_trace(ctx);
		ctx->running = false;
		ctx->vm.ncalls--;
		ctx->vm.fp = &ctx->vm.calls[ctx->vm.ncalls - 1];
	}
	
	longjmp(ctx->errjmp, 1);
}

const char* lur_get_error(lur_t* ctx) {
	return (const char*)ctx->error_msg->buffer;
}

static void print_error_line(const text_t* text, int line) {
	int count = 1;
	size_t len = 0;
	
	for (size_t i = 0; i < text->len; i++) {
		uint8_t ch = text->buffer[i];
		if (ch == '\n') count++;
		if (count == line) {
			for (size_t j = i + 1; j < text->len; j++) {
				ch = text->buffer[j];
				len++;
				
				if (ch == '\n' || j == text->len - 1) break;
			}
			
			lur_eprintf("%.*s\n", len, text->buffer + i);
			break;
		}
	}
}

static void print_stack_trace(const lur_t* ctx) {
	const cframe_t* cur = ctx->vm.fp;
	const cframe_t* end = ctx->vm.calls;
	if (cur == end)
		return;
	
	lur_eprintf("[ stack trace: ]\n");
	int index = 0;
	do {
		lur_eprintf("%d: ", index);
		text_eprint(cur->func->name);
		lur_eprintf("\n");
		cur--;
		index++;
	} while (cur > end);
}

static void gc_collect(lur_t*);
static void gc_pause(lur_t*);
static void gc_resume(lur_t*);

value_t lur_call_function(
	lur_t*, const fref_t*, value_t*, size_t);
static text_t* text_new(const uint8_t*, size_t, lur_t*);

static void* alloc(
	lur_t* ctx, void* p, size_t os, size_t ns,
	int64_t line, const char* func)
{
	if (os == ns) return p;
	
	#if LUR_DEBUG_PRINT_ALLOCS
	if (ctx && p != ctx)
		lur_dprintf("[mem: %+d - %s:%llu - %p]\n",
			ns - os, func, line, p);
	#endif
		
	if (ctx) {
		ctx->mem.bytes += ns - os;
		ctx->mem.total += ns;
		ctx->mem.nallocs++;
		
		if (ctx->cfg.memory_limit != 0 &&
			ctx->mem.bytes + ns - os > ctx->cfg.memory_limit)
			error(ctx, ERR_OUT_OF_MEMORY);
		
		if (ns > os && ctx->mem.total > ctx->mem.next_gc) {
			ctx->mem.next_gc *= GC_GROWTH_FACTOR;
			gc_collect(ctx);
		}
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
	alloc(ctx, NULL, 0, (size), __LINE__, __func__)
#define mem_free(ctx, p, size) \
	alloc(ctx, (p), (size), 0, __LINE__, __func__)
#define arr_alloc(ctx, p, t, on, nn) \
	((p) = alloc( \
		ctx, p, sizeof(t) * (on), sizeof(t) * (nn), \
			__LINE__, __func__))
#define arr_free(ctx, p, t, n) \
	((p) = alloc(ctx, p, sizeof(t) * (n), 0, \
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
static void array_free(array_t*, lur_t*);
static void map_free(map_t*, lur_t*);
static void func_free(func_t*, lur_t*);
static void fref_free(fref_t*, lur_t*);
static void vref_free(vref_t*, lur_t*);

static void obj_free(obj_t* obj, lur_t* ctx) {
	assert(obj && ctx);
	switch (obj->tag) {
	case TYPE_TEXT: text_free((text_t*)obj, ctx); break;
	case TYPE_ARRAY: array_free((array_t*)obj, ctx); break;
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

static text_t* text_copy(const text_t* src, lur_t* ctx) {
	assert(src && ctx);
	return text_new(src->buffer, src->len, ctx);
}

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
		uint8_t a_ch = tolower(a->buffer[i]);
		uint8_t b_ch = tolower(b->buffer[i]);
		if (a_ch < b_ch) return (text_t*)a;
		if (b_ch < a_ch) return (text_t*) b;
	}
	
	return (a->len > b->len) ? (text_t*)a : (text_t*)b;
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
	result->buffer[result->len] = '\0';
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

static text_t* text_slice(
	const text_t* text, size_t start, size_t end, lur_t* ctx)
{
	text_t* new_text = text_new(
		text->buffer + (size_t)start,
		end - start,
		ctx);
	new_text->buffer[new_text->len] = '\0';
	return new_text;
}

static text_t* text_repeat(
	const text_t* text, size_t times, lur_t* ctx) {
	assert(text);
	gc_pause(ctx);
	text_t* result = text_new(NULL, 0, ctx);
	for (size_t i = 0; i < times; i++)
		result = text_concat(result, text, ctx);
	gc_resume(ctx);
	return result;
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

static int64_t text_find_cstr(
	const text_t* text,
	const char* find,
	size_t find_len,
	size_t start)
{
	for (size_t i = start; i < text->len; i++) {
		bool match = i <= text->len - find_len &&
			strncmp(find, (const char*)text->buffer + i, find_len)
				== 0;
				
		if (match)
			return i;
	}
	
	return -1;
}

static int64_t text_find(
	const text_t* text, const text_t* find, size_t start)
{
	return text_find_cstr(text,
		(const char*)find->buffer, find->len, start);
}

static text_t* text_replace_all(
	const text_t* text,
	const text_t* find,
	const text_t* rep,
	lur_t* ctx)
{
	text_t* output = text_new(NULL, 0, ctx);
	
	for (size_t i = 0; i < text->len; i++) {
		bool match = i <= text->len - find->len &&
			strncmp(
				(const char*)find->buffer,
				(const char*)text->buffer + i,
				find->len) == 0;
				
		if (match) {
			i += find->len - 1;
			output = text_concat(output, rep, ctx);
			continue;
		}
		
		text_push(output, text->buffer[i], ctx);
	}
	
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

#define min2(a, b) ((a) < (b)) ? (a) : (b)
#define min3(a, b, c) min2(a, min2(b, c))

static int32_t text_edit_dist(
	const text_t* a, const text_t* b, lur_t* ctx)
{
	assert(a && b && ctx);
	if (strcmp((const char*)a->buffer,
		(const char*)b->buffer) == 0) return 0;
	if (a->len == 0) return b->len;
	if (b->len == 0) return a->len;
    
	int* v0 = mem_alloc(ctx, (b->len + 1) * sizeof(int));
	int* v1 = mem_alloc(ctx, (b->len + 1) * sizeof(int));
    
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
	mem_free(ctx, v0, (b->len + 1) * sizeof(int));
	mem_free(ctx, v1, (b->len + 1) * sizeof(int));
	return distance;
}

#undef min2
#undef min3

static void text_print(const text_t* text) {
	assert(text);
	lur_printf("%.*s", (int)text->len, text->buffer);
}

static void text_eprint(const text_t* text) {
	assert(text);
	lur_dprintf("%.*s", (int)text->len, text->buffer);
}

static void text_dprint(const text_t* text) {
	assert(text);
	lur_dprintf("%.*s", (int)text->len, text->buffer);
}

static array_t* array_new(lur_t* ctx) {
	assert(ctx);
	array_t* array = (array_t*)obj_new(
		sizeof(array_t), TYPE_ARRAY, ctx);
	array->items = NULL;
	array->len = 0;
	return array;
}

static void array_push(array_t*, value_t, lur_t*);

array_t* lur_new_array(
	const value_t* items, size_t len, lur_t* ctx)
{
	array_t* array = array_new(ctx);
	for (size_t i = 0; i < len; i++)
		array_push(array, items[i], ctx);
	return array;
}

array_t* lur_new_num_array(
	const double* items, size_t len, lur_t* ctx)
{
	array_t* array = array_new(ctx);
	for (size_t i = 0; i < len; i++)
		array_push(array, make_number(items[i]), ctx);
	return array;
}

static value_t value_copy(value_t, bool, lur_t*);
static bool value_is_obj(value_t);

static array_t* array_copy(
	const array_t* src, bool deep, lur_t* ctx)
{
	assert(src && ctx);
	gc_pause(ctx);
	
	array_t* dst = array_new(ctx);
	for (size_t i = 0; i < src->len; i++) {
		value_t item;
		if (deep) item = value_copy(src->items[i], deep, ctx);
		else item = src->items[i];
		array_push(dst, item, ctx);
	}
		
	gc_resume(ctx);
	return dst;
}

static void array_free(array_t* array, lur_t* ctx) {
	assert(array && ctx);
	arr_free(ctx,
		array->items, value_t, nextpow2(array->len));
	mem_free(ctx, array, sizeof(array));
}

static text_t* value_to_text(value_t, lur_t*);

static size_t array_convert_index(
	const array_t* array, double index, lur_t* ctx)
{
	if (index < 0)
		index = array->len + index;
	if (index >= array->len)
		error(ctx, ERR_INDEX(make_number(index)));
	return index;
}

static bool value_eq(value_t, value_t);

static bool array_eq(const array_t* a, const array_t* b) {
	assert(a && b);
	if (a == b) return true;
	if (a->len != b->len) return false;
	for (size_t i = 0; i < a->len; i++)
		if (!value_eq(a->items[i], b->items[i]))
			return false;
	return true;
}

static void array_push(
	array_t* array, value_t value, lur_t* ctx)
{
	assert(array && ctx);
	arr_alloc(ctx, array->items, value_t,
		nextpow2(array->len), nextpow2(array->len + 1));
	array->items[array->len++] = value;
}

static value_t array_pop(array_t* array, lur_t* ctx) {
	assert(array && ctx);
	if (array->len == 0)
		error(ctx, ERR_INDEX(make_number(0)));
	
	value_t value = array->items[array->len - 1];
	arr_alloc(ctx, array->items, value_t,
		nextpow2(array->len), nextpow2(--array->len));
	return value;
}

static void array_insert(
	array_t* array, size_t index, value_t value, lur_t* ctx)
{
	assert(array && ctx);
	if (array->len == 0) {
		array_push(array, value, ctx);
		return;
	}
	
	arr_alloc(ctx, array->items, value_t,
		nextpow2(array->len), nextpow2(array->len++));
	for (int64_t i = array->len - 1; i > index; i--)
		array->items[i] = array->items[i - 1];
	
	array->items[index] = value;
}

static void array_del(
	array_t* array, size_t index, lur_t* ctx)
{
	assert(array && ctx);
	if (array->len == 0) return;
	for (int64_t i = index; i < array->len - 1; i++)
		array->items[i] = array->items[i + 1];
	
	arr_alloc(ctx, array->items, value_t,
		nextpow2(array->len), nextpow2(--array->len));
}

static array_t* array_concat(
	const array_t* a, const array_t* b, lur_t* ctx)
{
	assert(a && b && ctx);
	gc_pause(ctx);
	array_t* result = array_new(ctx);
	for (size_t i = 0; i < a->len; i++)
		array_push(result, a->items[i], ctx);
	for (size_t i = 0; i < b->len; i++)
		array_push(result, b->items[i], ctx);
	gc_resume(ctx);
	return result;
}

static array_t* array_repeat(
	const array_t* target, size_t times, lur_t* ctx)
{
	assert(target && target > 0 && ctx);
	gc_pause(ctx);
	
	array_t* array = array_new(ctx);
	for (size_t i = 0; i < times; i++)
		array = array_concat(array, target, ctx);
		
	gc_resume(ctx);
	return array;
}

static void array_swap(array_t* array, size_t a, size_t b) {
	assert(array && a < array->len && b < array->len);
	value_t temp = array->items[a];
	array->items[a] = array->items[b];
	array->items[b] = temp;
}

static bool array_contains(array_t* array, value_t item) {
	assert(array);
	for (size_t i = 0; i < array->len; i++) {
		if (value_eq(array->items[i], item))
			return true;
	}
	return false;
}

static const char* type_name(type_t type);
static value_t value_math(value_t, value_t, int, lur_t*);

static void array_sort_impl(
	array_t* array, size_t n, fref_t* by, lur_t* ctx)
{
	assert(array && by && ctx);
	size_t swapped = 0;
	for (size_t i = 0; i < n - 1; i++) {
		value_t a = array->items[i];
		value_t b = array->items[i + 1];
		
		value_t args[] = {a, b};
		value_t result = lur_call_function(ctx, by, args, 2);
		bool swap = value_eq(result, b);
		
		if (swap) {
			array_swap(array, i, i + 1);
			swapped++;
		}
	}
	
	if (swapped != 0) array_sort_impl(array, n - 1, by, ctx);
}

static array_t* array_sort(
	const array_t* input, fref_t* by, lur_t* ctx)
{
	assert(input && by && ctx);
	array_t* sorted = array_copy(input, false, ctx);
	if (sorted->len <= 1) return sorted;
	array_sort_impl(sorted, sorted->len, by, ctx);
	return sorted;
}

static array_t* array_flat(const array_t* input, lur_t* ctx) {
	assert(input && ctx);
	array_t* output = array_new(ctx);
	for (size_t i = 0; i < input->len; i++) {
		value_t item = input->items[i];
		if (item.tag == TYPE_ARRAY)
			output = array_concat(output,
				array_flat(get_array(item), ctx), ctx);
		else
			array_push(output, item, ctx);
	}
	return output;
}

static array_t* array_reverse(
	const array_t* array, lur_t* ctx)
{
	assert(array);
	gc_pause(ctx);
	array_t* rev = array_new(ctx);
	if (array->len == 0) return rev;
	for (int64_t i = array->len - 1; i >= 0; i--)
		array_push(rev, array->items[i], ctx);
	gc_resume(ctx);
	return rev;
}

static text_t* value_to_text(value_t, lur_t*);

static text_t* array_join(const array_t* array, lur_t* ctx) {
	assert(array && ctx);
	gc_pause(ctx);
	text_t* text = text_new(NULL, 0, ctx);
	for (size_t i = 0; i < array->len; i++)
		text = text_concat(text, value_to_text(
			array->items[i], ctx), ctx);
	gc_resume(ctx);
	return text;
}

static array_t* array_rotate_left(
	const array_t* input, size_t n, lur_t* ctx)
{
	assert(input && ctx);
	array_t* array = array_copy(input, false, ctx);
	for (size_t i = 0; i < n; i++) {
		value_t temp = array->items[0];
		for (size_t j = 0; j < array->len - 1; j++) {
			array->items[j] = array->items[j + 1];
		}
		array->items[array->len - 1] = temp;
	}
	return array;
}

static array_t* array_rotate_right(
	const array_t* input, size_t n, lur_t* ctx)
{
	assert(input && ctx);
	array_t* array = array_copy(input, false, ctx);
	for (size_t i = 0; i < n; i++) {
		value_t temp = array->items[array->len - 1];
		for (int64_t j = array->len - 1; j >= 0; j--) {
			array->items[j] = array->items[j - 1];
		}
		array->items[0] = temp;
	}
	return array;
}

static map_t* map_new(lur_t* ctx) {
	assert(ctx);
	map_t* map = (map_t*)obj_new(
		sizeof(map_t), TYPE_MAP, ctx);
	map->indices = NULL;
	map->entries = NULL;
	map->cap = 0;
	map->len = 0;
	return map;
}

static bool map_set(map_t*, value_t, value_t, lur_t*);

static map_t* map_copy(
	const map_t* src, bool deep, lur_t* ctx)
{
	assert(src && ctx);
	
	gc_pause(ctx);
	map_t* dst = map_new(ctx);
	
	for (size_t i = 0; i < src->len; i++) {
		const map_entry_t* entry = &src->entries[i];
		if (entry->key.tag == TYPE_NONE) continue;
		
		value_t key, value;
		if (deep) {
			key = value_copy(entry->key, deep, ctx);
			value = value_copy(entry->value, deep, ctx);
		} else {
			key = entry->key;
			value = entry->value;
		}
		
		map_set(dst, key, value, ctx);
	}
	
	gc_resume(ctx);
	return dst;
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
		if (a->indices[i] == -1) continue;
		if (b->indices[i] == -1) continue;
		
		const map_entry_t* a_entry = &a->entries[
			a->indices[i]];
		const map_entry_t* b_entry = &b->entries[
			b->indices[i]];
		
		if (!value_eq(a_entry->key, b_entry->key))
			return false;
			
		if (!value_eq(a_entry->value, b_entry->value))
			return false;
	}
	return true;
}

static void map_debug(const map_t* map) {
	printf("cap %d, len %d\n", map->cap, map->len);
	puts("indices:");
	for (int64_t i = 0; i < map->cap; i++) {
		printf("%d ", i);
	}
	printf("\n");
	
	for (int64_t i = 0; i < map->cap; i++) {
		printf("%d ", map->indices[i]);
	}
	printf("\n");
}

static uint32_t value_hash(value_t, lur_t*);

/*static map_entry_t* map_find_entry(
	int64_t* indices,
	map_entry_t* entries,
	size_t cap,
	value_t key,
	lur_t* ctx)
{
	assert(entries);
	uint32_t hash = value_hash(key, ctx);
	uint32_t index = hash & (cap - 1);
	map_entry_t* tombstone = NULL;
	
	for (;;) {
		int64_t ind = indices[index];
		map_entry_t* entry = &entries[ind];
		printf("%d %d\n", index, ind);
		
		//if (entry->key.tag == TYPE_NONE) {
		if (ind == -1) {
			if (entry->value.tag == TYPE_NONE)
				return (tombstone) ? tombstone : entry;
			else if (!tombstone)
				tombstone = entry;
		} else if (value_eq(entry->key, key)) {
			return entry;
		}
		
		index = (index + 1) & (cap - 1);
	}
}*/

static int64_t map_find_index(
	int64_t* indices,
	map_entry_t* entries,
	size_t cap,
	size_t len,
	value_t key,
	lur_t* ctx)
{
	assert(entries);
	uint32_t hash = value_hash(key, ctx);
	uint32_t index = hash & (cap - 1);
	map_entry_t* tombstone = NULL;
	
	for (;;) {
		if (indices[index] == -1) {
			indices[index] = len;
			map_entry_t* entry = &entries[indices[index]];
			entry->key = make_none();
			entry->value = make_none();
			return indices[index];
			/*if (entry->value.tag == TYPE_NONE)
				return (tombstone) ? tombstone : entry;
			else if (!tombstone)
				tombstone = entry;*/
		} else {
			const map_entry_t* entry = &entries[indices[index]];
			if (value_eq(entry->key, key))
				return indices[index];
		}
		
		index = (index + 1) & (cap - 1);
	}
}

static void map_set_cap(
	map_t* map, size_t cap, lur_t* ctx)
{
	assert(map && ctx);
	int64_t* indices = mem_alloc(
		ctx, sizeof(int64_t) * cap);
	map_entry_t* entries = mem_alloc(
		ctx, sizeof(map_entry_t) * cap);
	
	for (size_t i = 0; i < cap; i++) {
		indices[i] = -1;
		entries[i].key = make_none();
		entries[i].value = make_none();
	}
	
	map->len = 0;
	for (size_t i = 0; i < map->cap; i++) {
		const map_entry_t* src = &map->entries[i];
		if (src->key.tag == TYPE_NONE) continue;
		
		int64_t index = map_find_index(
			indices, entries, cap, map->len, src->key, ctx);
		map_entry_t* dst = &entries[index];
		
		indices[i] = map->indices[i];
		dst->key = src->key;
		dst->value = src->value;
		map->len++;
	}
	
	arr_free(ctx, map->indices, int64_t, map->cap);
	arr_free(ctx, map->entries, map_entry_t, map->cap);
	map->indices = indices;
	map->entries = entries;
	map->cap = cap;
}

static void value_print(value_t, lur_t*);

static bool map_set(
	map_t* map, value_t key, value_t value, lur_t* ctx)
{
	assert(map && ctx);
	
	if (map->len + 1 > map->cap * MAP_MAX_LOAD) {
		size_t cap = nextpow2(map->cap + 1);
		map_set_cap(map, cap, ctx);
	}
	
	int64_t index = map_find_index(
		map->indices,
		map->entries,
		map->cap,
		map->len,
		key,
		ctx);
	
	map_entry_t* entry = &map->entries[index];
	
	bool is_new = entry->key.tag == TYPE_NONE;
	if (is_new && entry->value.tag == TYPE_NONE)
		map->len++;
	
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
	
	int64_t index = map_find_index(
		map->indices,
		map->entries,
		map->cap,
		map->len,
		key,
		ctx);
	
	const map_entry_t* entry = &map->entries[index];
	if (entry->key.tag == TYPE_NONE) return false;
	
	if (value)
		*value = entry->value;
	return true;
}

bool map_del(map_t* map, value_t key, lur_t* ctx) {
	assert(map && ctx);
	if (map->len == 0) return false;
	
	int64_t index = map_find_index(
		map->indices,
		map->entries,
		map->cap,
		map->len,
		key,
		ctx);
	
	map_entry_t* entry = &map->entries[index];
	if (entry->key.tag == TYPE_NONE) return false;
	
	entry->key = make_none();
	entry->value = make_bool(true);
	map->len--;
	return true;
}

static void map_extend(
	map_t* dst, const map_t* src, lur_t* ctx)
{
	assert(dst && src && ctx);
	for (size_t i = 0; i < src->len; i++) {
		const map_entry_t* entry = &src->entries[i];
		if (entry->key.tag == TYPE_NONE) continue;
		map_set(dst, entry->key, entry->value, ctx);
	}
}

static map_t* map_reverse(
	const map_t* src, lur_t* ctx)
{
	assert(src && ctx);
	gc_pause(ctx);
	
	map_t* dst = map_new(ctx);
	for (size_t i = 0; i < src->len; i++) {
		const map_entry_t* entry = &src->entries[i];
		if (entry->key.tag == TYPE_NONE) continue;
		map_set(dst, entry->value, entry->key, ctx);
	}
	
	gc_resume(ctx);
	return dst;
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

static fref_t* fref_copy(
	const fref_t* src, bool deep, lur_t* ctx)
{
	assert(src && ctx);
	gc_pause(ctx);
	
	if (deep) {
		// TODO: perform deep copy
		fref_t* dst = fref_new(src->func, ctx);
		dst->vrefs = src->vrefs;
		dst->nvrefs = src->nvrefs;
		gc_resume(ctx);
		return dst;
	}
	
	fref_t* dst = fref_new(src->func, ctx);
	dst->vrefs = src->vrefs;
	dst->nvrefs = src->nvrefs;
	gc_resume(ctx);
	return dst;
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
	vref->closed = make_none();
	vref->next = NULL;
	return vref;
}

static void vref_free(vref_t* vref, lur_t* ctx) {
	assert(vref && ctx);
	mem_free(ctx, vref, sizeof(vref_t));
}

static func_t* func_new(lur_t* ctx) {
	assert(ctx);
	func_t* func = (func_t*)obj_new(
		sizeof(func_t), TYPE_FUNC, ctx);
	func->name = NULL;
	func->src = NULL;
	func->argc = 0;
	func->syscall = NULL;
	func->code = NULL;
	func->ncode = 0;
	func->data = NULL;
	func->ndata = 0;
	func->lines = NULL;
	func->nlines = 0;
	func->nvrefs = 0;
	return func;
}

static void func_free(func_t* func, lur_t* ctx) {
	assert(func && ctx);
	arr_free(ctx, func->code, uint8_t,
		nextpow2(func->ncode));
	arr_free(ctx, func->data, value_t,
		nextpow2(func->ndata));
	arr_free(ctx, func->lines, int,
		nextpow2(func->nlines));
	mem_free(ctx, func, sizeof(func_t));
}

static bool func_eq(const func_t* a, const func_t* b) {
	if (a == b) return true;
	if (a->ncode != b->ncode) return false;
	if (a->ndata != b->ndata) return false;
	for (size_t i = 0; i < a->ncode; i++)
		if (a->code[i] != b->code[i])
			return false;
	for (size_t i = 0; i < a->ndata; i++)
		if (!value_eq(a->data[i], b->data[i]))
			return false;
	if (a->argc != b->argc) return false;
	if (a->syscall != b->syscall) return false;
	return true;
}

static void func_write(
	func_t* func, uint8_t byte, int line, lur_t* ctx)
{
	assert(func && ctx);
	if (func->ncode == MAX_CODE)
		error(ctx, ERR_LIMIT("opcodes", MAX_CODE));
	
	arr_alloc(ctx, func->code, uint8_t,
		nextpow2(func->ncode),
		nextpow2(func->ncode + 1));
	func->code[func->ncode++] = byte;
	
	if (func->ncode == MAX_LINES)
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

static const char* type_name(type_t type) {
	switch (type) {
	case TYPE_NONE:
	case TYPE_BOOL:
	case TYPE_NUMBER:
	case TYPE_TEXT:
	case TYPE_ARRAY:
	case TYPE_MAP:
	case TYPE_FREF: return TYPE_NAMES[type];
	default: unreachable();
	}
	
	return NULL;
}

static bool type_is_obj(type_t type) {
	switch (type) {
	case TYPE_NONE:
	case TYPE_BOOL:
	case TYPE_NUMBER: return false;
	case TYPE_TEXT:
	case TYPE_ARRAY:
	case TYPE_MAP:
	case TYPE_FREF:
	case TYPE_VREF:
	case TYPE_FUNC: return true;
	default: unreachable();
	}
	
	return false;
}

static text_t* value_to_text_ex(
	value_t value, bool quote_text, size_t max_len, lur_t* ctx)
{
	assert(ctx);
	text_t* result = NULL;
	gc_pause(ctx);
	
	switch (value.tag) {
	case TYPE_NONE: {
		result = text_lit("none", ctx);
		break;
	}
	case TYPE_BOOL: {
		result = text_lit(
			(get_bool(value)) ? "true" : "false", ctx);
		break;
	}
	case TYPE_NUMBER: {
		double number = get_number(value);
		if (trunc(number) == number)
			result = text_fmt(ctx, "%d", (long)number);
		else
			result = text_fmt(ctx, "%f", number);
		break;
	}
	case TYPE_TEXT: {
		if (quote_text) {
			result = text_fmt(ctx, "\"%s\"",
				get_text(value)->buffer);
		} else result = get_text(value);
		break;
	}
	case TYPE_ARRAY: {
		const array_t* array = get_array(value);
		result = text_lit("[", ctx);
		size_t printed = 0;
		for (size_t i = 0; i < array->len; i++) {
			if (value_eq(array->items[i], value)) {
				printed++;
				continue;
			}
			
			result = text_concat(result, value_to_text_ex(
				array->items[i], true, max_len, ctx), ctx);
			
			if (printed < array->len - 1)
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
		for (size_t i = 0; i < map->len; i++) {
			const map_entry_t* entry = &map->entries[i];
			if (entry->key.tag == TYPE_NONE) continue;
			if (value_eq(value, entry->value)) {
				printed++;
				continue;
			}
			
			result = text_concat(result, value_to_text_ex(
				entry->key, true, max_len, ctx), ctx);
			result = text_concat(result, text_lit(" => ", ctx), ctx);
			result = text_concat(result, value_to_text_ex(
				entry->value, true, max_len, ctx), ctx);
			
			if (printed < map->len - 1)
				result = text_concat(result, text_lit(", ", ctx), ctx);
			printed++;
		}
		result = text_concat(result, text_lit("}", ctx), ctx);
		break;
	}
	case TYPE_FUNC: {
		const func_t* func = get_func(value);
		result = text_fmt(ctx, "%.*s(%d)",
			func->name->len, func->name->buffer, func->argc);
		break;
	}
	case TYPE_FREF: {
		const fref_t* fref = get_fref(value);
		result = value_to_text(make_function(fref->func), ctx);
		break;
	}
	case TYPE_VREF: {
		result = text_lit("vref", ctx);
		break;
	}
	default: unreachable();
	}
	
	if (max_len > 0) {
		result = text_slice(result, 0, max_len - 3, ctx);
		result = text_concat(result, text_lit("...", ctx), ctx);
	}
	
	gc_resume(ctx);
	return result;
}

static text_t* value_to_text(value_t value, lur_t* ctx) {
	assert(ctx);
	return value_to_text_ex(value, false, 0, ctx);
}

static void value_print_ex(
	value_t value, bool quote_text, size_t max_len, lur_t* ctx)
{
	assert(ctx);
	text_print(value_to_text_ex(
		value, quote_text, max_len, ctx));
}

static void value_dprint_ex(
	value_t value, bool quote_text, size_t max_len, lur_t* ctx)
{
	assert(ctx);
	text_dprint(value_to_text_ex(
		value, quote_text, max_len, ctx));
}

static void value_print(value_t value, lur_t* ctx) {
	assert(ctx);
	text_print(value_to_text(value, ctx));
}

static void value_dprint(value_t value, lur_t* ctx) {
	assert(ctx);
	text_dprint(value_to_text(value, ctx));
}

static bool value_eq(value_t a, value_t b) {
	if (a.tag != b.tag) return false;
	switch (a.tag) {
	case TYPE_NONE: return true;
	case TYPE_BOOL: return get_bool(a) == get_bool(b);
	case TYPE_NUMBER:
		return get_number(a) == get_number(b);
	case TYPE_TEXT:
		return text_eq(get_text(a), get_text(b));
	case TYPE_ARRAY:
		return array_eq(get_array(a), get_array(b));
	case TYPE_MAP:
		return map_eq(get_map(a), get_map(b));
	case TYPE_FUNC:
		return func_eq(get_func(a), get_func(b));
	case TYPE_FREF:
		return func_eq(get_fref(a)->func, get_fref(b)->func);
	case TYPE_VREF: return get_vref(a) == get_vref(b);
	default: unreachable();
	}
	return false;
}

static text_t* value_serialize(value_t value, lur_t* ctx) {
	gc_pause(ctx);
	text_t* result = NULL;
	
	switch (value.tag) {
	case TYPE_NONE: {
		result = text_fmt(ctx, "%d ", TYPE_NONE);
		break;
	}
	case TYPE_BOOL: {
		result = text_fmt(ctx, "%d %d ",
			TYPE_BOOL, get_bool(value));
		break;
	}
	case TYPE_NUMBER: {
		result = text_fmt(ctx, "%d ", TYPE_NUMBER);
		double number = get_number(value);
		if (trunc(number) == number)
			result = text_concat(result, text_fmt(ctx,
				"%d ", (long)number), ctx);
		else
			result = text_concat(result, text_fmt(ctx,
				"%f ", number), ctx);
		break;
	}
	case TYPE_TEXT: {
		const text_t* text = get_text(value);
		result = text_fmt(ctx, "%d %ld %.*s ",
			TYPE_TEXT, text->len, (int)text->len, text->buffer);
		break;
	}
	case TYPE_ARRAY: {
		const array_t* array = get_array(value);
		result = text_fmt(ctx, "%d %ld ",
			TYPE_ARRAY, array->len);
		size_t printed = 0;
		for (size_t i = 0; i < array->len; i++) {
			if (value_eq(array->items[i], value)) {
				printed++;
				continue;
			}
			
			result = text_concat(result, value_serialize(
				array->items[i], ctx), ctx);
			
			if (printed < array->len)
				result = text_concat(result, text_lit(" ", ctx), ctx);
			printed++;
		}
		
		break;
	}
	case TYPE_MAP: {
		const map_t* map = get_map(value);
		result = text_fmt(ctx, "%d %d ",
			TYPE_MAP, map->len);
		
		size_t printed = 0;
		for (size_t i = 0; i < map->len; i++) {
			const map_entry_t* entry = &map->entries[i];
			if (entry->key.tag == TYPE_NONE) continue;
			
			if (value_eq(value, entry->value)) {
				printed++;
				continue;
			}
			
			result = text_concat(result, value_serialize(
				entry->key, ctx), ctx);
			result = text_concat(result, value_serialize(
				entry->value, ctx), ctx);
			
			if (printed < map->len)
				result = text_concat(result, text_lit(" ", ctx), ctx);
			printed++;
		}
		break;
	}
	case TYPE_FREF: {
		const fref_t* fref = get_fref(value);
		const func_t* func = fref->func;
		result = text_fmt(ctx, "%d %ld %.*s %d %ld ",
			TYPE_FREF,
			func->name->len,
			(int)func->name->len,
			func->name->buffer,
			func->argc,
			(long)func->syscall);
			
		if (func->syscall)
			break;
		
		result = text_concat(result,
			text_fmt(ctx, "%d ", func->ncode), ctx);
		for (size_t i = 0; i < func->ncode; i++)
			result = text_concat(result,
				text_fmt(ctx, "%d ", func->code[i]),
				ctx);
		
		result = text_concat(result,
			text_fmt(ctx, "%d ", func->ndata), ctx);
		for (size_t i = 0; i < func->ndata; i++) {
			const text_t* data = value_serialize(
				func->data[i], ctx);
			result = text_concat(result,
				text_fmt(ctx, "%.*s ", data->len, data->buffer),
				ctx);
		}
		
		result = text_concat(result,
			text_fmt(ctx, "%d ", func->nlines), ctx);
		for (size_t i = 0; i < func->nlines; i++)
			result = text_concat(result,
				text_fmt(ctx, "%d ", func->lines[i]),
				ctx);
				
		result = text_concat(result,
			text_fmt(ctx, "%d ", func->nvrefs), ctx);
		for (size_t i = 0; i < func->nvrefs; i++)
			result = text_concat(result,
				value_serialize(make_vref(fref->vrefs[i]), ctx), ctx);
		break;
	}
	case TYPE_VREF: {
		const vref_t* vref = get_vref(value);
		result = text_fmt(ctx, "%d %ld ",
			TYPE_VREF, vref->index);
		result = text_concat(result,
			value_serialize(vref->closed, ctx), ctx);
		result = text_concat(result,
			text_fmt(ctx, "%ld ", (long)vref->next), ctx);
		break;
	}
	default: unreachable();
	}
	
	gc_resume(ctx);
	return result;
}

static value_t deserialize(
	char* data, size_t offset, lur_t* ctx)
{
	const char* delim = " ";
	char* tok = strtok((offset == 0) ? data : NULL, delim);
	int type = atoi(tok);
	value_t value;
	
	#define get_token() \
		(tok = strtok(NULL, delim), offset += strlen(tok), tok)
	
	switch (type) {
	case TYPE_NONE: {
		value = make_none();
		break;
	}
	case TYPE_BOOL: {
		value = make_bool(atoi(get_token()));
		break;
	}
	case TYPE_NUMBER: {
		value = make_number(atof(get_token()));
		break;
	}
	case TYPE_TEXT: {
		long len = atol(get_token());
		text_t* text = text_new(NULL, 0, ctx);
		while (text->len < len) {
			char* token = get_token();
			if (data[offset] == ' ')
				text_push(text, ' ', ctx);
			
			text = text_concat(text, text_lit(token, ctx), ctx);
			if (text->len < len)
				text_push(text, ' ', ctx);
		}
		value = make_text(text);
		break;
	}
	case TYPE_ARRAY: {
		int len = atoi(get_token());
		array_t* array = array_new(ctx);
		while (array->len < len)
			array_push(array, deserialize(data, offset, ctx), ctx);
		value = make_array(array);
		break;
	}
	case TYPE_MAP: {
		int len = atoi(get_token());
		map_t* map = map_new(ctx);
		while (map->len < len) {
			value_t key = deserialize(data, offset, ctx);
			value_t value = deserialize(data, offset, ctx);
			map_set(map, key, value, ctx);
		}
		value = make_map(map);
		break;
	}
	case TYPE_FREF: {
		long len = atol(get_token());
		text_t* name = text_new(NULL, 0, ctx);
		while (name->len < len) {
			char* token = get_token();
			if (data[offset] == ' ')
				text_push(name, ' ', ctx);
			
			name = text_concat(name, text_lit(token, ctx), ctx);
			if (name->len < len)
				text_push(name, ' ', ctx);
		}
		
		func_t* func = func_new(ctx);
		func->name = name;
		func->argc = atoi(get_token());
		func->syscall = (syscall_fn_t)atol(get_token());
		
		if (func->syscall) {
			value = make_fref(fref_new(func, ctx));
			break;
		}
		
		len = atoi(get_token());
		for (size_t i = 0; i < len; i++)
			func_write(func, atoi(get_token()), 1, ctx);
		
		len = atoi(get_token());
		for (size_t i = 0; i < len; i++)
			func_write_value(func,
				deserialize(data, offset, ctx), ctx);
		
		func->nlines = atoi(get_token());
		for (size_t i = 0; i < func->nlines; i++)
			func->lines[i] = atoi(get_token());
		
		func->nvrefs = atoi(get_token());
		fref_t* fref = fref_new(func, ctx);
		fref->nvrefs = func->nvrefs;
		for (size_t i = 0; i < fref->nvrefs; i++)
			fref->vrefs[i] = get_vref(
				deserialize(data, offset, ctx));
		
		value = make_fref(fref);
		break;
	}
	case TYPE_VREF: {
		vref_t* vref = vref_new(
			atol(get_token()), ctx);
		vref->closed = deserialize(data, offset, ctx);
		vref->next = (vref_t*)atol(get_token());
		value = make_vref(vref);
		break;
	}
	default: unreachable();
	}
	
	return value;
	
	#undef get_token
}

static value_t value_deserialize(
	const text_t* data, lur_t* ctx)
{
	assert(data);
	gc_pause(ctx);
	value_t result = deserialize(
		(char*)data->buffer, 0, ctx);
	gc_resume(ctx);
	return result;
}

static value_t value_copy(
	value_t value, bool deep, lur_t* ctx)
{
	switch (value.tag) {
	case TYPE_NONE:
	case TYPE_BOOL:
	case TYPE_NUMBER: return value;
	case TYPE_TEXT: {
		const text_t* text = get_text(value);
		return make_text(text_copy(text, ctx));
	}
	case TYPE_ARRAY: {
		const array_t* array = get_array(value);
		return make_array(array_copy(array, deep, ctx));
	}
	case TYPE_MAP: {
		const map_t* map = get_map(value);
		return make_map(map_copy(map, deep, ctx));
	}
	case TYPE_FREF: {
		const fref_t* fref = get_fref(value);
		return make_fref(fref_copy(fref, deep, ctx));
	}
	default: unreachable();
	}
}

#define typecheck(value, t) \
	if ((value).tag != (t)) \
		error(ctx, ERR_TYPECHECK((value).tag, (t)))
		
static value_t vector_math(
	value_t a, value_t b, int op, lur_t* ctx)
{
	gc_pause(ctx);
			
	if (b.tag == TYPE_ARRAY) {
		const array_t* v1 = get_array(a);
		const array_t* v2 = get_array(b);
		array_t* out = array_new(ctx);
		size_t shortest = (v1->len < v2->len) ?
			v1->len : v2->len;
		
		for (size_t i = 0; i < shortest; i++) {
			typecheck(v1->items[i], TYPE_NUMBER);
			typecheck(v2->items[i], TYPE_NUMBER);	
			array_push(out,
				value_math(v1->items[i], v2->items[i], op, ctx),
				ctx);
		}
				
		gc_resume(ctx);
		return make_array(out);
	}
	
	typecheck(b, TYPE_NUMBER);
	const array_t* array = get_array(a);
	array_t* out = array_new(ctx);
			
	for (size_t i = 0; i < array->len; i++) {
		typecheck(array->items[i], TYPE_NUMBER);
		array_push(out,
			value_math(array->items[i], b, op, ctx),
			ctx);
	}
			
	gc_resume(ctx);
	return make_array(out);
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
		if (a.tag == TYPE_ARRAY || b.tag == TYPE_ARRAY)
			return vector_math(a, b, op, ctx);
		
		typecheck(a, TYPE_NUMBER);
		typecheck(b, TYPE_NUMBER);
		return make_number(
			get_number(a) + get_number(b));
	}
	case OP_SUB: {
		if (a.tag == TYPE_ARRAY || b.tag == TYPE_ARRAY)
			return vector_math(a, b, op, ctx);
		
		typecheck(a, TYPE_NUMBER);
		typecheck(b, TYPE_NUMBER);
		return make_number(
			get_number(a) - get_number(b));
	}
	case OP_MUL: {
		if (a.tag == TYPE_ARRAY || b.tag == TYPE_ARRAY)
			return vector_math(a, b, op, ctx);
		
		typecheck(a, TYPE_NUMBER);
		typecheck(b, TYPE_NUMBER);
		return make_number(
			get_number(a) * get_number(b));
	}
	case OP_POW: {
		if (a.tag == TYPE_ARRAY || b.tag == TYPE_ARRAY)
			return vector_math(a, b, op, ctx);
		
		typecheck(a, TYPE_NUMBER);
		typecheck(b, TYPE_NUMBER);
		return make_number(
			powf(get_number(a), get_number(b)));
	}
	case OP_DIV: {
		if (a.tag == TYPE_ARRAY || b.tag == TYPE_ARRAY)
			return vector_math(a, b, op, ctx);
		
		typecheck(a, TYPE_NUMBER);
		typecheck(b, TYPE_NUMBER);
		if (get_number(b) == 0.0)
			error(ctx, ERR_DIV_BY_ZERO);
		return make_number(
			get_number(a) / get_number(b));
	}
	case OP_REM: {
		if (a.tag == TYPE_ARRAY || b.tag == TYPE_ARRAY)
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
		if (a.tag == TYPE_ARRAY) {
			gc_pause(ctx);
			array_t* array = get_array(a);
			if (b.tag == TYPE_ARRAY)
				array = array_concat(array, get_array(b), ctx);
			else
				array_push(array, b, ctx);
			gc_resume(ctx);
			return make_array(array);
		} 
			
		if (b.tag == TYPE_ARRAY) {
			gc_pause(ctx);
			array_t* array = get_array(b);
			array_insert(array, 0, a, ctx);
			gc_resume(ctx);
			return make_array(array);
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
			value_to_text(a, ctx),
			value_to_text(b, ctx),
			ctx);
		gc_resume(ctx);
		return make_text(text);
	}
	case OP_INV: {
		if (a.tag == TYPE_TEXT)
			return make_text(text_reverse(get_text(a), ctx));
		if (a.tag == TYPE_ARRAY)
			return make_array(array_reverse(get_array(a), ctx));
		if (a.tag == TYPE_MAP)
			return make_map(map_reverse(get_map(a), ctx));
		typecheck(a, TYPE_NUMBER);
		return make_number(-get_number(a));
	}
	default: unreachable();
	}
	
	return make_none();
}

#undef typecheck

static uint32_t value_hash(value_t value, lur_t* ctx) {
	assert(ctx);
	switch (value.tag) {
	case TYPE_NONE: return 0;
	case TYPE_BOOL: return get_bool(value) + 1;
	case TYPE_NUMBER: return get_number(value);
	case TYPE_TEXT: {
		const text_t* text = get_text(value);
		uint32_t hash = 2166136261u;
		for (int i = 0; i < text->len; i++) {
			hash ^= text->buffer[i];
    		hash *= 16777619;
		}
		return hash;
	}
	case TYPE_ARRAY:
	case TYPE_MAP: {
		const text_t* text = value_to_text(value, ctx);
		return value_hash(make_text(text), ctx);
	}
	case TYPE_FREF: {
		const fref_t* fref = get_fref(value);
		return value_hash(make_text(fref->func->name), ctx);
	}
	case TYPE_FUNC: {
		const func_t* func = get_func(value);
		return value_hash(make_text(func->name), ctx);
	}
	default: unreachable();
	}
	
	return 0;
}

static void gc_mark_obj(obj_t* obj, lur_t* ctx) {
	assert(ctx);
	if (!obj || obj->marked) return;
	obj->marked = true;
	ctx->mem.marked = lur_gc_realloc(ctx->mem.marked,
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
		case TYPE_ARRAY: {
			array_t* array = (array_t*)obj;
			for (size_t i = 0; i < array->len; i++)
				gc_mark_value(array->items[i], ctx);
			break;
		}
		case TYPE_MAP: {
			map_t* map = (map_t*)obj;
			for (size_t i = 0; i < map->len; i++) {
				const map_entry_t* entry = &map->entries[i];
				if (entry->key.tag == TYPE_NONE) continue;
				gc_mark_value(entry->key, ctx);
				gc_mark_value(entry->value, ctx);
			}
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
		case TYPE_FUNC: {
			func_t* func = (func_t*)obj;
			gc_mark_obj((obj_t*)func->name, ctx);
			gc_mark_obj((obj_t*)func->src, ctx);
			for (size_t i = 0; i < func->ndata; i++)
				gc_mark_value(func->data[i], ctx);
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
	
	ctx->mem.next_gc = ctx->mem.bytes *
		GC_GROWTH_FACTOR;
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
	for (size_t i = 0; i < vm->globals->len; i++) {
		map_entry_t* entry = &vm->globals->entries[i];
		if (entry->key.tag == TYPE_NONE) continue;
		gc_mark_value(entry->key, ctx);
		gc_mark_value(entry->value, ctx);
	}
	
	gc_mark_obj((obj_t*)ctx->args, ctx);
	for (int i = 0; i < ctx->args->len; i++)
		gc_mark_value(ctx->args->items[i], ctx);
		
	gc_mark_obj((obj_t*)ctx->help, ctx);
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
	
	for (size_t i = 0; i < MAX_THREADS; i++)
		vm->threads[i].is_free = true;
	
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
	
	lur_dprintf("data:\n");
	for (size_t i = 0; i < func->ndata; i++) {
		lur_dprintf("  %02x: %s = ",
			i, type_name(func->data[i].tag));
		value_dprint_ex(func->data[i], true, vm->ctx);
		lur_dprintf("\n");
	}
	lur_dprintf("\n");
}
#endif

#if LUR_DEBUG_PRINT_CODE
static void dbg_print_header(const vm_t* vm) {
	assert(vm);
	
	lur_dprintf("\n[%d] ", vm->fp - vm->calls);
	text_dprint(vm->fp->func->name);
	lur_dprintf("\n");
	
	lur_dprintf("addr  line         name bytes      comment |\n");
	lur_dprintf("-------------------------------------------|\n");
}

static void dbg_print_opcode(const vm_t* vm) {
	assert(vm);
	
	size_t addr =
		vm->fp->ip - vm->fp->func->code;
	int line = vm->fp->func->lines[addr];
	int prev_line = line;
	if (addr > 0)
		prev_line = vm->fp->func->lines[addr - 1];
	uint8_t opcode = *vm->fp->ip;
	
	if (prev_line != line || addr == 0)
		lur_dprintf("%04d %5d %12s %02x",
			addr, line, OPINFO[opcode].name, opcode);
	else
		lur_dprintf("%04d     | %12s %02x",
			addr, OPINFO[opcode].name, opcode);
	
	for (int i = 0; i < OPINFO[opcode].len; i++)
		lur_dprintf(" %02x", vm->fp->ip[i + 1]);
		
	for (int i = 0; i < 3 - OPINFO[opcode].len; i++)
		lur_dprintf("   ");
	
	uint16_t u16arg = vm->fp->ip[1] << 8 | vm->fp->ip[2];
	switch (opcode) {
	case OP_DATA: {
		value_t value = vm->fp->func->data[u16arg];
		value_dprint_ex(value, true, 64, vm->ctx);
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
		
		lur_dprintf("[");
		value_dprint(a, vm->ctx);
		lur_dprintf(", ");
		value_dprint(b, vm->ctx);
		lur_dprintf("] -> [");
		value_dprint(b, vm->ctx);
		lur_dprintf(", ");
		value_dprint(a, vm->ctx);
		lur_dprintf("]");
		break;
	}
	case OP_GETLOC: {
		value_t value = (vm->stack + vm->fp->slots)[
			u16arg];
		value_dprint_ex(value, true, 64, vm->ctx);
		break;
	}
	case OP_GETVREF: {
		vref_t* vref = vm->fp->fref->vrefs[u16arg];
		if (vref->index == -1) {
			lur_dprintf("(closed) ");
			value_print(vref->closed, vm->ctx);
		} else {
			lur_dprintf("(open) ");
			value_print(vm->stack[vref->index], vm->ctx);
		}
		break;
	}
	case OP_SETVREF: {
		vref_t* vref = vm->fp->fref->vrefs[u16arg];
		if (vref->index == -1) {
			lur_dprintf("(closed) ");
			value_dprint(vm->sp[-1], vm->ctx);
		} else {
			lur_dprintf("(open) ");
			value_dprint(vm->sp[-1], vm->ctx);
		}
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
		value_dprint_ex(key, true, 64, vm->ctx);
		lur_dprintf(": ");
		value_t value;
		map_get(vm->globals, key, &value, vm->ctx);
		value_dprint_ex(value, true, 64, vm->ctx);
		break;
	}
	case OP_SETGLOB: {
		value_t value = vm->fp->func->data[u16arg];
		value_dprint_ex(value, true, 64, vm->ctx);
		lur_dprintf(": ");
		value_dprint_ex(vm->sp[-1], true, 64, vm->ctx);
		break;
	}
	case OP_ADDGLOB: {
		value_t value = vm->fp->func->data[u16arg];
		value_dprint_ex(value, false, 64, vm->ctx);
		lur_dprintf(": ");
		value_t key = vm->fp->func->data[u16arg];
		map_get(vm->globals, key, &value, vm->ctx);
		value_dprint_ex(value, true, 64, vm->ctx);
		break;
	}
	case OP_CALL: {
		uint8_t argc = vm->fp->ip[1];
		value_t value = vm->sp[-argc - 1];
		
		assert(value.tag == TYPE_FREF);
		const fref_t* fref = get_fref(vm->sp[-argc - 1]);
		text_dprint(fref->func->name);
		lur_dprintf(" (%d arg%c)",
			fref->func->argc,
			(fref->func->argc == 1) ? '\0' : 's');
		break;
	}
	case OP_RET: {
		lur_dprintf("-> ");
		value_dprint_ex(vm->sp[-1], true, 64, vm->ctx);
		break;
	}
	}
	
	#if !LUR_DEBUG_PRINT_STACK
	lur_dprintf("\n");
	#endif
}
#endif

#if LUR_DEBUG_PRINT_STACK
static void dbg_print_stack(const vm_t* vm) {
	assert(vm);
	
	value_t* cur = vm->stack + vm->fp->slots;
	value_t* end = vm->sp;
	
	lur_dprintf("\n\t [");
	while (cur != end) {
		value_print_ex(*cur, true, vm->ctx);
		cur++;
		if (cur != end)
			lur_printf(", ");
	}
	lur_dprintf("]\n");
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

	while (true) {
		#if LUR_DEBUG_PRINT_CODE
		dbg_print_opcode(vm);
		#endif
		
		switch (read_u8()) {
		case OP_NOP: break;
		case OP_NONE: push(make_none()); break;
		case OP_FALSE: push(make_bool(false)); break;
		case OP_TRUE: push(make_bool(true)); break;
		case OP_DATA: {
			push(vm->fp->func->data[read_u16()]);
			break;
		}
		case OP_NEWARRAY: {
			gc_pause(vm->ctx);
			uint16_t len = read_u16();
			array_t* array = array_new(vm->ctx);
			for (size_t i = 0; i < len; i++)
				array_push(array, get(i), vm->ctx);
			vm->sp -= len;
			push(make_array(array));
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
		case OP_GETFIELD: {
			if (get(0).tag == TYPE_ARRAY) {
				gc_pause(vm->ctx);
				
				const array_t* array = get_array(pop());
				value_t key = vm->fp->func->data[read_u16()];
				if (value_eq(key,
					make_text(text_lit("x", vm->ctx))))
				{
					push(array->items[
						array_convert_index(array, 0.0, vm->ctx)]);
				} else if (value_eq(key,
					make_text(text_lit("y", vm->ctx))))
				{
					push(array->items[
						array_convert_index(array, 1.0, vm->ctx)]);
				} else if (value_eq(key,
					make_text(text_lit("z", vm->ctx))))
				{
					push(array->items[
						array_convert_index(array, 2.0, vm->ctx)]);
				} else if (value_eq(key,
					make_text(text_lit("w", vm->ctx))))
				{
					push(array->items[
						array_convert_index(array, 3.0, vm->ctx)]);
				} else {
					error(vm->ctx, ERR_UNDEFINED(key, vm->ctx));
					unreachable();
				}
				
				gc_resume(vm->ctx);
				break;
			}
			
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
			if (get(1).tag == TYPE_ARRAY) {
				gc_pause(vm->ctx);
				
				value_t value = pop();
				const array_t* array = get_array(pop());
				value_t key = vm->fp->func->data[read_u16()];
				if (value_eq(key,
					make_text(text_lit("x", vm->ctx))))
				{
					array->items[
						array_convert_index(array, 0.0, vm->ctx)] =
							value;
				} else if (value_eq(key,
					make_text(text_lit("y", vm->ctx))))
				{
					array->items[
						array_convert_index(array, 1.0, vm->ctx)] =
							value;
				} else if (value_eq(key,
					make_text(text_lit("z", vm->ctx))))
				{
					array->items[
						array_convert_index(array, 2.0, vm->ctx)] =
							value;
				} else if (value_eq(key,
					make_text(text_lit("w", vm->ctx))))
				{
					array->items[
						array_convert_index(array, 3.0, vm->ctx)] =
							value;
				} else {
					error(vm->ctx, ERR_UNDEFINED(key, vm->ctx));
					unreachable();
				}
				
				push(value);
				gc_resume(vm->ctx);
				break;
			}
			
			typecheck(1, TYPE_MAP);
			map_t* map = get_map(get(1));
			value_t key = vm->fp->func->data[read_u16()];
			map_set(map, key, get(0), vm->ctx);
			value_t result = pop();
			pop();
			push(result);
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
		case OP_EQ:
		case OP_NE: {
			value_t b = get(0);
			value_t a = get(1);
			value_t result =
				value_math(a, b, vm->fp->ip[-1], vm->ctx);
			pop();
			pop();
			push(result);
			break;
		}
		case OP_NOT: {
			value_t value = get(0);
			value_t result = 
				value_math(value, make_none(), OP_NOT,
					vm->ctx);
			pop();
			push(result);
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
			value_t b = get(0);
			value_t a = get(1);
			value_t result =
				value_math(a, b, vm->fp->ip[-1], vm->ctx);
			pop();
			pop();
			push(result);
			break;
		}
		case OP_INV: {
			value_t value = get(0);
			value_t result = 
				value_math(value, make_none(), OP_INV,
					vm->ctx);
			pop();
			push(result);
			break;
		}
		case OP_JMP: {
			vm->fp->ip += read_u16();
			break;
		}
		case OP_JIF: {
			uint16_t offset = read_u16();
			if (get_bool(get(0))) vm->fp->ip += offset;
			break;
		}
		case OP_JIFNOT: {
			uint16_t offset = read_u16();
			if (!get_bool(get(0))) vm->fp->ip += offset;
			break;
		}
		case OP_CALL: {
			uint8_t argc = read_u8();
			vm_call(vm, get(argc), argc, false);
			break;
		}
		case OP_TAILCALL: {
			uint8_t argc = read_u8();
			const fref_t* fref = get_fref(get(argc));
			if (argc != fref->func->argc)
				error(vm->ctx, ERR_ARGC(
					fref->func->name, argc, fref->func->argc));
			
			vm->fp->func = fref->func;
			vm->fp->fref = fref;
			vm->fp->ip = fref->func->code;
			vm->fp->slots = (vm->sp - vm->stack) - argc - 1;
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
			
			if (returns) return get(0);
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
	call.ip = fref->func->code;
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

value_t lur_call_function(
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
	
	bool is_in_comment = false;
	while (isspace((unsigned char)*sc->pos) ||
		(*sc->pos == '-' && sc->pos[1] == '-')) {
		if  (*sc->pos == '-' &&
			sc->pos[1] == '-' &&
			sc->pos[2] == '[')
			is_in_comment = true;
		
		if (*sc->pos == '-' && sc->pos[1] == '-' &&
			sc->pos[2] == '[')
		{
			while (*sc->pos != '\0' &&
				!(*sc->pos == '-' && sc->pos[1] == '-' &&
					sc->pos[2] == ']'))
			{
				if (*sc->pos == '\n')
					sc->line++;
				sc->pos++;
			}
			is_in_comment = false;
		}
		
		if (*sc->pos == '-' && sc->pos[1] == '-') {
			while (*sc->pos != '\n' && *sc->pos != '\0')
				sc->pos++;
		}
			
		if (is_in_comment)
			error(sc->ctx, ERR_UNTERMINATED_COMMENT);
		
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
		
		if (keyword("is")) return make(T_IS);
		if (keyword("none")) return make(T_NONE);
		if (keyword("true")) return make(T_TRUE);
		if (keyword("false")) return make(T_FALSE);
		if (keyword("do")) return make(T_DO);
		if (keyword("end")) return make(T_END);
		if (keyword("fun")) return make(T_FUN);
		if (keyword("return")) return make(T_RETURN);
		if (keyword("if")) return make(T_IF);
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
		while (isdigit(*sc->pos) ||
			*sc->pos == '.' ||
			*sc->pos == '_' ||
			((is_hex || is_bin) && isalpha(*sc->pos) &&
			*sc->pos != '\0'))
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
	lur_dprintf("token: ");
	text_dprint(ps->cur.lex);
	lur_dprintf("\n");
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
	
	if (ps_match(ps, T_EQ)) {
		node->tag = AST_BIND;
		node->data.bind.name = name;
		node->data.bind.value = ps_expr(ps);
		return node;
	}
	
	if (ps_match(ps, T_LPAREN)) {
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
	
	node->tag = AST_LOAD;
	node->data.load.name = name;
	return node;
}

static ast_node_t* ps_null(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_VALUE;
	node->data.value.data = make_none();
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
	
	text_t* text = text_copy(ps->prev.lex, ps->ctx);
	text = text_replace_all(
		text, text_lit("_", ps->ctx), text_lit("", ps->ctx), ps->ctx);
	
	double num;
	switch (ps->prev.tag) {
	case T_NUMBER: {
		num = strtof((const char*)text->buffer, NULL);
		break;
	}
	case T_NUMBER_HEX: {
		num = strtol((const char*)text->buffer, NULL, 16);
		break;
	}
	case T_NUMBER_BIN: {
		num = strtol( (const char*)text->buffer, NULL, 2);
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

static ast_node_t* ps_array(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_ARRAY;
	
	node->data.array.items = NULL;
	node->data.array.len = 0;
	
	while (!ps_check(ps, T_EOF) &&
		!ps_check(ps, T_RSQUARE))
	{
		arr_alloc(ps->ctx,
			node->data.array.items, ast_node_t*,
			node->data.array.len, node->data.array.len + 1);
		node->data.array.items[node->data.array.len++] =
			ps_expr(ps);
			
		if (!ps_match(ps, T_COMMA))
			break;
	}
	
	ps_eat(ps, T_RSQUARE,
		"expected ',' or ']' after array items");
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
	case T_IS: opcode = OP_EQ; break;
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

static ast_node_t* ps_call_no_args(parser_t* ps) {
	assert(ps);
	ast_node_t* node = ps_new_node(ps);
	node->tag = AST_CALL;
	node->data.call.func = ps->prefix;
	node->data.call.argc = 0;
	node->data.call.args = NULL;
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
		ps_match(ps, T_CARET_EQ) ||
		ps_match(ps, T_SLASH_EQ) ||
		ps_match(ps, T_PERCENT_EQ) ||
		ps_match(ps, T_AMPER_EQ);
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

static void cl_init(comp_t* cl, const text_t* src, lur_t* ctx) {
	assert(cl && ctx);
	
	cl->func = func_new(ctx);
	cl->func->name = text_lit("<script>", ctx);
	cl->func->src = src;
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
	return cl->func->ncode - 2;
}

static void cl_patch_jump(comp_t* cl, size_t addr) {
	assert(cl);
	
	size_t offset = cl->func->ncode - addr - 2;
	if (offset > MAX_JUMP)
		error(cl->ctx, ERR_LIMIT("max jump", MAX_JUMP));
	
	cl->func->code[addr] = (offset >> 8) & 0xff;
	cl->func->code[addr + 1] = offset & 0xff;
}

static void cl_push_value(comp_t* cl, value_t value) {
	assert(cl);
	
	if (value.tag == TYPE_NONE) {
		cl_write(cl, OP_NONE);
		return;
	}
	
	if (value.tag == TYPE_BOOL) {
		cl_write(cl, (get_bool(value)) ? OP_TRUE : OP_FALSE);
		return;
	}
	
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
		
	if (locals == 0 || locals == 1) return;
	if (locals == 2) {
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
	case AST_ARRAY: {
		ast_array_t* array = &node->data.array;
		if (array->len > MAX_ARRAY_LIT_ITEMS)
			error(cl->ctx, ERR_LIMIT(
				"array literal item", MAX_ARRAY_LIT_ITEMS));
		
		for (int64_t i = array->len - 1; i >= 0; i--)
			cl_compile_ast(cl, array->items[i]);
		
		cl_write(cl, OP_NEWARRAY);
		cl_write(cl, (array->len >> 8) & 0xff);
		cl_write(cl, array->len & 0xff);
		break;
	}
	case AST_MAP: {
		ast_map_t* map = &node->data.map;
		if (map->len > MAX_MAP_LIT_ITEMS)
			error(cl->ctx, ERR_LIMIT(
				"map literal item", MAX_MAP_LIT_ITEMS));
		
		for (int64_t i = map->len - 1; i >= 0; i--) {
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
		
		cl_write(cl, OP_DUP);
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
				(binary->opcode == -2) ? OP_JIF : OP_JIFNOT);
			cl_write(cl, OP_POP);
			cl_compile_ast(cl, binary->rhs);
			cl_patch_jump(cl, jump);
		}
		break;
	}
	case AST_BLOCK: {
		ast_block_t* block = &node->data.block;
		if (block->nitems == 0) {
			cl_push_value(cl, make_none());
			break;
		}
			
		cl_open_scope(cl);
		for (size_t i = 0; i < block->nitems; i++) {
			cl_compile_ast(cl, block->items[i]);
			
			if (block->items[i]->tag != AST_FUN &&
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
		cl_init(&env, cl->func->src, cl->ctx);
		
		env.func->name = text_concat(
			cl->func->name, text_lit("/", cl->ctx), cl->ctx);
		env.func->name = text_concat(
			env.func->name, fun->name, cl->ctx);
			
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
		
		bool is_tailcall = false;
		cl_write(cl, (is_tailcall) ? OP_TAILCALL : OP_CALL);
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
			case T_CARET_EQ: cl_write(cl, OP_POW); break;
			case T_SLASH_EQ: cl_write(cl, OP_DIV); break;
			case T_PERCENT_EQ: cl_write(cl, OP_REM); break;
			case T_AMPER_EQ: cl_write(cl, OP_CON); break;
			default: unreachable();
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
		
		size_t skip_a = cl_write_jump(cl, OP_JIFNOT);
		cl_write(cl, OP_POP);
		cl_compile_ast(cl, branch->a);
		
		size_t skip_b = cl_write_jump(cl, OP_JMP);
		cl_patch_jump(cl, skip_a);
		cl_write(cl, OP_POP);
		
		if (branch->b) cl_compile_ast(cl, branch->b);
		else cl_push_value(cl, make_none());
		
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
	case AST_ARRAY: {
		ast_array_t* array = &node->data.array;
		for (size_t i = 0; i < array->len; i++)
			cl_free_ast(cl, array->items[i]);
		arr_free(cl->ctx,
			array->items, ast_node_t*, array->len);
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
	ast_node_t* ast = ps_parse(&cl->parser);
	cl_compile_ast(cl, ast);
	
	#if LUR_DEBUG_PRINT_AST
	dbg_print_ast(cl, ast, 0);
	#endif
	
	if (cl->ctx->interpreter) {
		size_t slot = func_write_value(
			cl->func,
			make_text(text_lit("io", cl->ctx)),
			cl->ctx);
		cl_write(cl, OP_GETGLOB);
		cl_write(cl, (slot >> 8) & 0xff);
		cl_write(cl, slot & 0xff);
		
		slot = func_write_value(
			cl->func,
			make_text(text_lit("print", cl->ctx)),
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

#define typecheck(arg, t) \
	if (args[arg].tag != (t)) \
		error(ctx, ERR_TYPECHECK_ARG( \
			args[arg].tag, (t), arg))
			
static text_t* io_read(const text_t*, lur_t*);

#define STD_HELP_DOC \
	"name\n" \
	"shows help text for a function or variable. " \
	"use 'help: \"all\"' to see all documentation."
	
static value_t std_help(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	const text_t* name = get_text(args[0]);
	
	if (text_eq(name, text_lit("all", ctx))) {
		text_t* text = text_new(NULL, 0, ctx);
		for (size_t i = 0; i < ctx->help->len; i++) {
			const map_entry_t* entry = &ctx->help->entries[i];
			if (entry->key.tag == TYPE_NONE) continue;
			
			text = text_concat(text,
				text_fmt(ctx, "%s %s\n\n",
					get_text(entry->key)->buffer,
					(entry->value.tag == TYPE_TEXT) ?
						get_text(entry->value)->buffer :
						text_lit("(no docs)", ctx)->buffer),
				ctx);
		}
		return make_text(text);
	}
	
	value_t help_text;
	if (!map_get(ctx->help, args[0], &help_text, ctx))
		return make_text(text_fmt(ctx,
			"no associated docstring found for variable '%s'",
			get_text(args[0])->buffer));
	
	return make_text(text_fmt(ctx, "%s %s\n",
			get_text(args[0])->buffer,
			(help_text.tag == TYPE_TEXT) ?
				get_text(help_text)->buffer :
				text_lit("none", ctx)->buffer));
}

#define STD_LOAD_DOC \
	"path\n" \
	"loads and executes a script from a path."

static value_t std_load(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	cl_init(&ctx->cl, ctx->cl.func->src, ctx);
	cl_compile(&ctx->cl,
		io_read(get_text(args[0]), ctx),
		get_text(args[0]));
	
	vm_launch(&ctx->vm,
		make_fref(fref_new(ctx->cl.func, ctx)), 0, true);
		
	cl_free(&ctx->cl);
	return make_none();
}

#define STD_EVAL_DOC \
	"code\n" \
	"evaluates some code from text, returns its return " \
	"value."

static value_t std_eval(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	cl_init(&ctx->cl, ctx->cl.func->src, ctx);
	cl_compile(&ctx->cl,
		get_text(args[0]),
		get_text(args[0]));
	value_t result = vm_launch(&ctx->vm,
		make_fref(fref_new(ctx->cl.func, ctx)), 0, true);
	cl_free(&ctx->cl);
	return result;
}

#define STD_ERROR_DOC \
	"msg\nexits the interpreter with an error message."

static value_t std_error(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	error(ctx, (const char*)get_text(args[0])->buffer);
	return make_none();
}

#define STD_TYPE_OF_DOC \
	"value\nreturns the type of a value."

static value_t std_type_of(value_t* args, lur_t* ctx) {
	return make_number(args[0].tag);
}

#define STD_DEFAULT_DOC \
	"type\nreturns a default value for a given type."

static value_t std_default(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	int64_t type = (int64_t)get_number(args[0]);
	switch (type) {
	case TYPE_NONE: return make_none();
	case TYPE_BOOL: return make_bool(false);
	case TYPE_NUMBER: return make_number(0);
	case TYPE_TEXT: return make_text(text_lit("", ctx));
	case TYPE_ARRAY: return make_array(array_new(ctx));
	case TYPE_MAP: return make_map(map_new(ctx));
	case TYPE_FREF: {
		func_t* func = func_new(ctx);
		func_write(func, OP_NONE, 1, ctx);
		func_write(func, OP_RET, 1, ctx);
		return make_fref(fref_new(func, ctx));
	}
	default: error(ctx, ERR_RANGE(0, TYPE_FREF, type));
	}
	
	unreachable();
	return make_none();
}

#define STD_SERIALIZE_DOC \
	"value\nserializes the value into text."

static value_t std_serialize(
	value_t* args, lur_t* ctx)
{
	return make_text(value_serialize(args[0], ctx));
}

#define STD_DESERIALIZE_DOC \
	"data\nreturns a value decoded from serialized data."

static value_t std_deserialize(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_TEXT);
	return value_deserialize(get_text(args[0]), ctx);
}

#define STD_AS_NUMBER_DOC \
	"value\nreturns value converted into a number."

static value_t std_as_number(value_t* args, lur_t* ctx) {
	switch (args[0].tag) {
	case TYPE_NONE: return make_number(0);
	case TYPE_BOOL: return make_number(
		get_bool(args[0]));
	case TYPE_NUMBER: return args[0];
	case TYPE_TEXT: return make_number(strtof(
			(const char*)get_text(args[0])->buffer, NULL));
	case TYPE_ARRAY: {
		text_t* text = array_join(get_array(args[0]), ctx);
		return std_as_number(&make_text(text), ctx);
	}
	default: unreachable();
	}
	return make_number(0);
}

#define STD_AS_TEXT_DOC \
	"value\nreturns value converted into text."

static value_t std_as_text(value_t* args, lur_t* ctx) {
	return make_text(value_to_text(args[0], ctx));
}

#define STD_LOOP_DOC \
	"n, fn\ncreates a loop that calls the function fn n "  \
	"times with the current index as argument ."

static value_t std_loop(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_FREF);
	double n = get_number(args[0]);
	fref_t* fref = get_fref(args[1]);
	
	for (size_t i = 0; i < n; i++) {
		value_t args[] = { make_number(i) };
		lur_call_function(ctx, fref, args, 1);
	}
	
	return make_none();
}

#define STD_WHILE_DOC \
	"fn\ncalls the function fn repeatedly until it returns false."

static value_t std_while(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_FREF);
	fref_t* fref = get_fref(args[0]);
	
	while (true) {
		value_t args[] = {};
		value_t result = lur_call_function(ctx, fref, args, 0);
		if (result.tag != TYPE_BOOL)
			error(ctx, ERR_TYPECHECK(
				result.tag, TYPE_BOOL));
		
		if (!get_bool(result))
			break;
	}
	
	return make_none();
}

#define STD_RANGE_DOC \
	"start, end\ncreates an array containing values from " \
	"start up until end."

static value_t std_range(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	int64_t start = (int64_t)get_number(args[0]);
	int64_t end = (int64_t)get_number(args[1]);
	array_t* array = array_new(ctx);
	if (start < end)
		for (int64_t i = start; i < end; i++)
			array_push(array, make_number(i), ctx);
	else
		for (int64_t i = start; i > end; i--)
			array_push(array, make_number(i), ctx);
	return make_array(array);
}

#define STD_RANGE_INC_DOC \
	"start, end\ncreates an array containing values from " \
	"start up to and including end."

static value_t std_range_inc(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	int64_t start = (int64_t)get_number(args[0]);
	int64_t end = (int64_t)get_number(args[1]);
	array_t* array = array_new(ctx);
	if (start < end)
		for (int64_t i = start; i <= end; i++)
			array_push(array, make_number(i), ctx);
	else
		for (int64_t i = start; i >= end; i--)
			array_push(array, make_number(i), ctx);
	return make_array(array);
}

#define STD_DEFAULT_SORT_DOC \
	"a, b\ndefault sorting function, used by array.sort."

static value_t std_default_sort(value_t* args, lur_t* ctx) {
	value_t a = args[0];
	value_t b = args[1];
	if (a.tag == TYPE_NUMBER) {
		if (b.tag != TYPE_NUMBER) \
			error(ctx, ERR_TYPECHECK(
				b.tag, TYPE_NUMBER));
		return get_bool(value_math(a, b, OP_LT, ctx)) ? a : b;
		
	} else if (a.tag == TYPE_TEXT) {
		if (b.tag != TYPE_TEXT) \
			error(ctx, ERR_TYPECHECK(b.tag, TYPE_TEXT));
		return text_cmp(get_text(a), get_text(b)) ? b : a;
	}
	return a;
}

#define STD_ENUM_DOC \
	"names\ntakes an array of enum discriminator names " \
	"and returns a map mapping the names to numbers."

static value_t std_enum(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	array_t* array = get_array(args[0]);
	map_t* enum_ = map_new(ctx);
	for (size_t i = 0; i < array->len; i++) {
		text_t* name = value_to_text(array->items[i], ctx);
		map_set(enum_,
			make_text(name),
			make_number(i),
			ctx);
	}
	return make_map(enum_);
}

#define STD_ASSERT_DOC \
	"condition\nasserts that condition is true, otherwise " \
	"halts with an error message."

static value_t std_assert(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_BOOL);
	if (!get_bool(args[0])) error(ctx, ERR_ASSERTION);
	return make_none();
}

static size_t mem_total_ram(void) {
	struct sysinfo info;
	sysinfo(&info);
	return info.totalram;
}

static size_t mem_free_ram(void) {
	struct sysinfo info;
	sysinfo(&info);
	return info.freeram;
}

#define STD_MEM_ADDR_DOC \
	"value\n" \
	"returns the pointer address in host memory of an " \
	"object, or the text 'stack' if the value is not an object type"

static value_t std_mem_addr(value_t* args, lur_t* ctx) {
	if (!type_is_obj(args[0].tag))
		return make_text(text_lit("stack", ctx));
	return make_text(text_fmt(ctx, "%p", args[0].data.obj));
}

#define STD_MEM_COPY_DOC \
	"value\n" \
	"returns a copied version of the value with a new " \
	"address."

static value_t std_mem_copy(value_t* args, lur_t* ctx) {
	return value_copy(args[0], false, ctx);
}

#define STD_MEM_DEEP_COPY_DOC \
	"value\n" \
	"returns a deep copied version of the value with a new " \
	"address."

static value_t std_mem_deep_copy(
	value_t* args, lur_t* ctx)
{
	return value_copy(args[0], true, ctx);
}

#define STD_MEM_TOTAL_RAM_DOC \
	"returns the total number of bytes of ram available."

static value_t std_mem_total_ram(
	value_t* args, lur_t* ctx)
{
	return make_number(mem_total_ram());
}

#define STD_MEM_FREE_RAM_DOC \
	"returns the number of bytes of free ram available."

static value_t std_mem_free_ram(
	value_t* args, lur_t* ctx)
{
	return make_number(mem_free_ram());
}

#define STD_TIME_NOW_DOC \
	"returns the current time and date as text."

static value_t std_time_now(value_t* args, lur_t* ctx) {
	time_t current_time;
	time(&current_time);
	const char* str = ctime(&current_time);
	return make_text(text_lit(str, ctx));
}

#define STD_TIME_YEAR_DOC \
	"returns the current year."

static value_t std_time_year(value_t* args, lur_t* ctx) {
	time_t now = time(NULL);
	struct tm* t = localtime(&now);
	return make_number(t->tm_year + 1900);
}

#define STD_TIME_MONTH_DOC \
	"returns the current month."

static value_t std_time_month(value_t* args, lur_t* ctx) {
	time_t now = time(NULL);
	struct tm* t = localtime(&now);
	return make_number(t->tm_mon + 1);
}

#define STD_TIME_DAY_DOC \
	"returns the current day."

static value_t std_time_day(value_t* args, lur_t* ctx) {
	time_t now = time(NULL);
	struct tm* t = localtime(&now);
	return make_number(t->tm_mday);
}

#define STD_TIME_HOUR_DOC \
	"returns the current hour."

static value_t std_time_hour(value_t* args, lur_t* ctx) {
	time_t now = time(NULL);
	struct tm* t = localtime(&now);
	return make_number(t->tm_hour);
}

#define STD_TIME_MINUTE_DOC \
	"returns the current minute."

static value_t std_time_minute(value_t* args, lur_t* ctx) {
	time_t now = time(NULL);
	struct tm* t = localtime(&now);
	return make_number(t->tm_min);
}

#define STD_TIME_SECOND_DOC \
	"returns the current second."

static value_t std_time_second(value_t* args, lur_t* ctx) {
	time_t now = time(NULL);
	struct tm* t = localtime(&now);
	return make_number(t->tm_sec);
}

#define STD_TIME_TIMESTAMP_DOC \
	"returns a timestamp measured in seconds."

static value_t std_time_timestamp(
	value_t* args, lur_t* ctx)
{
	return make_number(time(NULL));
}

#define STD_TIME_CLOCK_DOC \
	"returns the CPU clock time in seconds."

static value_t std_time_clock(value_t* args, lur_t* ctx) {
	return make_number((double)clock() /
		CLOCKS_PER_SEC);
}

#define STD_TIME_SLEEP_DOC \
	"seconds\nsleeps the current thread for x amount of " \
	"seconds."

static value_t std_time_sleep(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	double secs = get_number(args[0]) * 1000.0 * 1000.0;
	usleep(secs);
	return make_none();
}

static const char* system_get_platform(void) {
	#ifdef _WIN32
    return "Windows 32-bit";
    #elif _WIN64
    return "Windows 64-bit";
    #elif __APPLE__ || __MACH__
    return "Mac OSX";
    #elif __linux__
    return "Linux";
    #elif __FreeBSD__
    return "FreeBSD";
    #elif __unix || __unix__
    return "Unix";
    #else
    return "Unknown";
    #endif
}

#define STD_SYSTEM_UPTIME_DOC \
	"returns the system uptime."

static value_t std_system_uptime(
	value_t* args, lur_t* ctx)
{
	struct sysinfo info;
	sysinfo(&info);
	return make_number(info.uptime);
}

static size_t system_average_load(void) {
	struct sysinfo info;
	sysinfo(&info);
	float factor = 1.0 / (1 << SI_LOAD_SHIFT);
	return info.loads[0] * factor;
}

#define STD_SYSTEM_AVERAGE_LOAD_DOC \
	"returns the average load over the last minute."

static value_t std_system_average_load(
	value_t* args, lur_t* ctx)
{
	return make_number(system_average_load());
}

#define STD_SYSTEM_PROCESS_COUNT_DOC \
	"returns the current process count."

static value_t std_system_process_count(
	value_t* args, lur_t* ctx)
{
	struct sysinfo info;
	sysinfo(&info);
	return make_number(info.procs);
}

#define STD_SYSTEM_CMD_DOC \
	"cmd\nruns a command."

static value_t std_system_cmd(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	system((const char*)get_text(args[0])->buffer);
	return make_none();
}

#define STD_SYSTEM_EXIT_DOC \
	"code\nexits the interpreter with an exit code."

static value_t std_system_exit(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	exit(get_number(args[0]));
}

static double math_rand(double start, double end) {
	double scale = rand() / (double)RAND_MAX;
	return start + scale * (end - start);
}

#define STD_MATH_EQF_DOC \
	"a, b, epsilon\ncompares two numbers with a " \
	"tolerance of epsilon."

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

#define STD_MATH_BIT_SHL_DOC \
	"a, b\nbitshifts a b positions to the left."

static value_t std_math_bit_shl(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	uint64_t a = (uint64_t)get_number(args[0]);
	uint64_t b = (uint64_t)get_number(args[1]);
	return make_number(a << b);
}

#define STD_MATH_BIT_SHR_DOC \
	"a, b\nbitshifts a b positions to the right."

static value_t std_math_bit_shr(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	uint64_t a = (uint64_t)get_number(args[0]);
	uint64_t b = (uint64_t)get_number(args[1]);
	return make_number(a >> b);
}

#define STD_MATH_BIT_AND_DOC \
	"a, b\nperforms a bitwise AND between a and b."

static value_t std_math_bit_and(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	uint64_t a = (uint64_t)get_number(args[0]);
	uint64_t b = (uint64_t)get_number(args[1]);
	return make_number(a & b);
}

#define STD_MATH_BIT_OR_DOC \
	"a, b\nperforms a bitwise OR between a and b."

static value_t std_math_bit_or(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	uint64_t a = (uint64_t)get_number(args[0]);
	uint64_t b = (uint64_t)get_number(args[1]);
	return make_number(a | b);
}

#define STD_MATH_BIT_XOR_DOC \
	"a, b\nperforms a bitwise XOR between a and b."

static value_t std_math_bit_xor(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	uint64_t a = (uint64_t)get_number(args[0]);
	uint64_t b = (uint64_t)get_number(args[1]);
	return make_number(a ^ b);
}

#define STD_MATH_BIT_NOT_DOC \
	"x\nperforms a bitwise NOT on x."

static value_t std_math_bit_not(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(
		~((int64_t)get_number(args[0])));
}

#define STD_MATH_LOG_DOC \
	"target, base\nreturns the number needed to raise " \
	"base to target."

static value_t std_math_log(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	double target = get_number(args[0]);
	double base = get_number(args[1]);
	return make_number(log(target) / log(base));
}

#define STD_MATH_SQRT_DOC \
	"x\nreturns the square root of x."

static value_t std_math_sqrt(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(sqrt(get_number(args[0])));
}

#define STD_MATH_SQUARED_DOC \
	"x\nreturns x * x."

static value_t std_math_squared(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_NUMBER);
	double x = get_number(args[0]);
	return make_number(x * x);
}

#define STD_MATH_ABS_DOC \
	"x\nreturns the absolute (positive) value of x."

static value_t std_math_abs(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(fabs(get_number(args[0])));
}

#define STD_MATH_MIN_DOC \
	"a, b\nreturns the smaller value of a and b."

static value_t std_math_min(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	double a = get_number(args[0]);
	double b = get_number(args[1]);
	return make_number((a < b) ? a : b);
}

#define STD_MATH_MAX_DOC \
	"a, b\nreturns the larger value of a and b."

static value_t std_math_max(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	double a = get_number(args[0]);
	double b = get_number(args[1]);
	return make_number((a > b) ? a : b);
}

#define STD_MATH_CLIP_DOC \
	"x, lo, hi\nclamps x between lo and hi."

static value_t std_math_clip(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	typecheck(2, TYPE_NUMBER);
	double x = get_number(args[0]);
	double lo = get_number(args[1]);
	double hi = get_number(args[2]);
	return make_number((x < lo) ? lo : (x > hi) ? hi : x);
}

#define STD_MATH_LERP_DOC \
	"t, a, b\nlinearly interpolates between a and b based on t."

static value_t std_math_lerp(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	typecheck(2, TYPE_NUMBER);
	double t = get_number(args[0]);
	double a = get_number(args[1]);
	double b = get_number(args[2]);
	return make_number(a * (1.0 - t) + b * t);
}

#define STD_MATH_INTERP_TO_DOC \
	"current, target, speed, dt\ninterpolates current " \
	"towards target based on speed and dt."

static value_t std_math_interp_to(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	typecheck(2, TYPE_NUMBER);
	typecheck(3, TYPE_NUMBER);
	
	double current = get_number(args[0]);
	double target = get_number(args[1]);
	double speed = get_number(args[2]);
	double dt = get_number(args[3]);
	
	return make_number(
			current + (target - current) * (speed * dt));
}

#define STD_MATH_FLOOR_DOC \
	"x\nrounds towards -inf."

static value_t std_math_floor(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(floor(get_number(args[0])));
}

#define STD_MATH_CEIL_DOC \
	"x\nrounds towards +inf."

static value_t std_math_ceil(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(ceil(get_number(args[0])));
}

#define STD_MATH_ROUND_DOC \
	"x\nrounds x to the nearest integer."

static value_t std_math_round(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(round(get_number(args[0])));
}

#define STD_MATH_RAD_DOC \
	"x\nconverts degrees to radians."

static value_t std_math_rad(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(
		get_number(args[0]) * (M_PI / 180.0));
}

#define STD_MATH_DEG_DOC \
	"x\nconverts radians to degrees."

static value_t std_math_deg(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(
		get_number(args[0]) * (180.0 / M_PI));
}

#define STD_MATH_SIN_DOC \
	"x\nreturns the sine of x."

static value_t std_math_sin(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(sin(get_number(args[0])));
}

#define STD_MATH_COS_DOC \
	"x\nreturns the cosine of x."

static value_t std_math_cos(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(cos(get_number(args[0])));
}

#define STD_MATH_TAN_DOC \
	"x\nreturns the tangent of x."

static value_t std_math_tan(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(tan(get_number(args[0])));
}

#define STD_MATH_ASIN_DOC \
	"x\nreturns the arcsine of x."

static value_t std_math_asin(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(asin(get_number(args[0])));
}

#define STD_MATH_ACOS_DOC \
	"x\nreturns the arccosine of x."

static value_t std_math_acos(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(acos(get_number(args[0])));
}

#define STD_MATH_ATAN_DOC \
	"x\nreturns the arctangent of x."

static value_t std_math_atan(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(atan(get_number(args[0])));
}

#define STD_MATH_ATAN2_DOC \
	"x, y\ntwo argument arctangent."

static value_t std_math_atan2(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	double x = get_number(args[0]);
	double y = get_number(args[1]);
	return make_number(atan2(x, y));
}

#define STD_MATH_SINH_DOC \
	"x\nreturns the hyperbolic sine of x."

static value_t std_math_sinh(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(sinh(get_number(args[0])));
}

#define STD_MATH_COSH_DOC \
	"x\nreturns the hyperbolic cosine of x."

static value_t std_math_cosh(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(cosh(get_number(args[0])));
}

#define STD_MATH_TANH_DOC \
	"x\nreturns the hyperbolic tangent of x."

static value_t std_math_tanh(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(tanh(get_number(args[0])));
}

#define STD_MATH_ASINH_DOC \
	"x\nreturns the hyperbolic arcsine of x."

static value_t std_math_asinh(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(asinh(get_number(args[0])));
}

#define STD_MATH_ACOSH_DOC \
	"x\nreturns the hyperbolic arccosine of x."

static value_t std_math_acosh(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(acosh(get_number(args[0])));
}

#define STD_MATH_ATANH_DOC \
	"x\nreturns the hyperbolic arctangent of x."

static value_t std_math_atanh(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_number(atanh(get_number(args[0])));
}

#define STD_MATH_IS_EVEN_DOC \
	"x\nreturns true if x is even."

static value_t std_math_is_even(value_t* args, lur_t* ctx)  {
	typecheck(0, TYPE_NUMBER);
	return make_bool(fmod(
		get_number(args[0]), 2.0) == 0.0);
}

#define STD_MATH_IS_ODD_DOC \
	"x\nreturns true if x is odd."

static value_t std_math_is_odd(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_bool(fmod(
		get_number(args[0]), 2.0) != 0.0);
}

#define STD_MATH_IS_PRIME_DOC \
	"x\nreturns true if x is prime."

static value_t std_math_is_prime(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	uint64_t number = (uint64_t)get_number(args[0]);
	if (number <= 1) return make_bool(false);
	for (size_t i = 2; i * i <= number; i++) {
		if (number % i == 0)
			return make_bool(false);
	}
	return make_bool(true);
}

#define STD_MATH_HEX_DOC \
	"x\nreturns x as a hexidecimal string."

static value_t std_math_hex(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	return make_text(text_fmt(
		ctx, "0x%x", (int64_t)get_number(args[0])));
}

#define STD_MATH_BIN_DOC \
	"x\nreturns x as a binary string."

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

#define STD_MATH_HASH_DOC \
	"value\nreturns the FNV-1a hash of the value."

static value_t std_math_hash(value_t* args, lur_t* ctx) {
	return make_number(value_hash(args[0], ctx));
}

#define STD_VEC_SIZE_DOC \
	"v\nreturns the magnitude of v."

static value_t std_vec_size(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	const array_t* vec = get_array(args[0]);
	
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

#define STD_VEC_NORM_DOC \
	"v\nreturns a normalized copy of v."

static value_t std_vec_norm(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	const array_t* vec = get_array(args[0]);
	array_t* out = array_copy(vec, false, ctx);
	
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
	
	return make_array(out);
}

#define STD_VEC_DOT_DOC \
	"v1, v2\nreturns the dot product of v1 and v2."

static value_t std_vec_dot(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(1, TYPE_ARRAY);
	const array_t* a = get_array(args[0]);
	const array_t* b = get_array(args[1]);
	
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

#define STD_VEC_CROSS_DOC \
	"v1, v2\nreturns the cross product of v1 and v2."

static value_t std_vec_cross(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(1, TYPE_ARRAY);
	const array_t* a = get_array(args[0]);
	const array_t* b = get_array(args[1]);
	array_t* out = array_new(ctx);
	
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
		
		array_push(out, make_number(result), ctx);
	}
	
	return make_array(out);
}

#define STD_VEC_INTERP_TO_DOC \
	"v1, v2, speed, dt\ninterpolates v1 towards v2 based on " \
	"speed and dt."

static value_t std_vec_interp_to(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(1, TYPE_ARRAY);
	typecheck(2, TYPE_NUMBER);
	typecheck(3, TYPE_NUMBER);
	
	const array_t* v1 = get_array(args[0]);
	const array_t* v2 = get_array(args[1]);
	double speed = get_number(args[2]);
	double dt = get_number(args[3]);
	
	array_t* ret = array_new(ctx);
	size_t shortest = (v1->len < v2->len) ? v1->len : v2->len;
	for (size_t i = 0; i < shortest; i++) {
		// TODO: typecheck
		double current = get_number(v1->items[i]);
		double target = get_number(v2->items[i]);
		array_push(ret, make_number(
			current + (target - current) * (speed * dt)),
			ctx);
	}
	
	return make_array(ret);
}

#define STD_RAND_SEED_DOC \
	"seed\nseeds the random number generator."

static value_t std_rand_seed(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	srand(get_number(args[0]));
	return make_none();
}

#define STD_RAND_NUMBER_DOC \
	"min, max\nreturns a random number between " \
	"min and max."

static value_t std_rand_number(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	return make_number(math_rand(
		get_number(args[0]),
		get_number(args[1])));
}

#define STD_RAND_TEXT_DOC \
	"len, charset\ngenerates a random string of characters " \
	"from charset len characters long."

static value_t std_rand_text(value_t* args, lur_t* ctx) {
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

#define STD_RAND_ITEM_DOC \
	"array\nreturns a random item from the array."

static value_t std_rand_item(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	array_t* array = get_array(args[0]);
	return array->items[(size_t)math_rand(0, array->len)];
}

#define STD_TEXT_LEN_DOC \
	"text\nreturns the length of the text."

static value_t std_text_len(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	return make_number(get_text(args[0])->len);
}

#define STD_TEXT_CMP_DOC \
	"a, b\nreturns the alphabetically superior of a and b."

static value_t std_text_cmp(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_TEXT);
	const text_t* a = get_text(args[0]);
	const text_t* b = get_text(args[1]);
	return make_text(text_cmp(a, b));
}

#define STD_TEXT_BYTES_DOC \
	"text\nreturns an array containing the characters that " \
	"make up a string."

static value_t std_text_bytes(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	const text_t* text = get_text(args[0]);
	array_t* bytes = array_new(ctx);
	for (size_t i = 0; i < text->len; i++)
		array_push(bytes, make_text(text_new(
			text->buffer + i, 1, ctx)), ctx);
	return make_array(bytes);
}

#define STD_TEXT_ASCII_DOC \
	"text\nreturns an array containing the ASCII of a string."

static value_t std_text_ascii(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	const text_t* text = get_text(args[0]);
	array_t* ascii = array_new(ctx);
	for (size_t i = 0; i < text->len; i++)
		array_push(ascii, make_number(text->buffer[i]), ctx);
	return make_array(ascii);
}

#define STD_TEXT_WORDS_DOC \
	"text delim\nsplits the text into words based on a " \	
	"delimiter."

static value_t std_text_words(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_TEXT);
	
	const text_t* text = get_text(args[0]);
	const text_t* delim = get_text(args[1]);
	array_t* tokens = array_new(ctx);
	
	char* token = strtok(text->buffer, delim->buffer);
	if (!token) return make_array(tokens);
	
	do {
		array_push(tokens,
			make_text(text_lit(token, ctx)), ctx);
	} while (token = strtok(NULL, delim->buffer));
	
	return make_array(tokens);
}

#define STD_TEXT_SLICE_DOC \
	"text, start, end\nreturns a substring based on indices " \
	"start and end."

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
	
	return make_text(text_slice(text, start, end, ctx));
}

#define STD_TEXT_REPEAT_DOC \
	"text, n\nrepeats text n times."

static value_t std_text_repeat(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_NUMBER);
	return make_text(text_repeat(get_text(args[0]),
		(size_t)get_number(args[1]), ctx));
}

#define STD_TEXT_AS_UPPER_DOC \
	"text\nreturns text converted to uppercase."

static value_t std_text_as_upper(
	value_t* args, lur_t* ctx) 
{
	typecheck(0, TYPE_TEXT);
	const text_t* original = get_text(args[0]);
	text_t* text = text_copy(original, ctx);
	for (size_t i = 0; i < text->len; i++)
		text->buffer[i] = toupper(original->buffer[i]);
	return make_text(text);
}

#define STD_TEXT_AS_LOWER_DOC \
	"text\nreturns text converted to lowercase."

static value_t std_text_as_lower(
	value_t* args, lur_t* ctx) 
{
	typecheck(0, TYPE_TEXT);
	const text_t* original = get_text(args[0]);
	text_t* text = text_copy(original, ctx);
	for (size_t i = 0; i < text->len; i++)
		text->buffer[i] = tolower(original->buffer[i]);
	return make_text(text);
}

#define STD_TEXT_IS_LOWER_DOC \
	"text\nreturns true if the text is entirely lowercase."

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

#define STD_TEXT_IS_UPPER_DOC \
	"text\nreturns true if the text is entirely uppercase."

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

#define STD_TEXT_STARTS_WITH_DOC \
	"text, start\nreturns true if text starts with start."

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

#define STD_TEXT_ENDS_WITH_DOC \
	"text, end\nreturns true if text ends with end."

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

#define STD_TEXT_TRIM_LEFT_DOC \
	"text\nstrips whitespace off the start of the text."

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

#define STD_TEXT_TRIM_RIGHT_DOC \
	"text\nstrips whitespace off the end of the text."

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

#define STD_TEXT_TRIM_DOC \
	"text\nstrips whitespace off both ends of the text."

static value_t std_text_trim(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	value_t text = args[0];
	text = std_text_trim_left(&text, ctx);
	text = std_text_trim_right(&text, ctx);
	return text;
}

#define STD_TEXT_LEFT_PAD_DOC \
	"text, width, pattern\npads the left side of the text so " \
	"that it is width characters long."

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

#define STD_TEXT_RIGHT_PAD_DOC \
	"text, width, pattern\npads the right side of the text so " \
	"that it is width characters long."

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

#define STD_TEXT_PAD_DOC \
	"text, width, pattern\npads the center of the text so " \
	"that it is width characters long."

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

#define STD_TEXT_FIND_DOC \
	"text, find, start\nfinds the text find within the text after " \
	"position start."

static value_t std_text_find(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_TEXT);
	typecheck(2, TYPE_NUMBER);
	int64_t pos = text_find(
		get_text(args[0]),
		get_text(args[1]),
		get_number(args[2]));
	return (pos == -1) ? make_none() : make_number(pos);
}

#define STD_TEXT_REPLACE_ALL_DOC \
	"text, find, with\nreplaces all occurences of find within " \
	"text with with."

static value_t std_text_replace_all(value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_TEXT);
	typecheck(2, TYPE_TEXT);
	const text_t* text = get_text(args[0]);
	const text_t* find = get_text(args[1]);
	const text_t* rep = get_text(args[2]);
	return make_text(text_replace_all(
		text, find, rep, ctx));
}

#define STD_TEXT_EDIT_DIST_DOC \
	"a, b\ncalculates the edit distance between two strings."

static value_t std_text_edit_dist(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_TEXT);
	return make_number(text_edit_dist(
		get_text(args[0]), get_text(args[1]), ctx));
}

#define STD_TEXT_RATIO_DOC \
	"a, b\nreturns the ratio of text match amount " \
	"between a and b."

static value_t std_text_ratio(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_TEXT);
	const text_t* a = get_text(args[0]);
	const text_t* b = get_text(args[1]);
	size_t len = a->len + b->len;
	return make_number(
		(len - text_edit_dist(a, b, ctx)) / (double)len);
}

#define STD_ARRAY_LEN_DOC \
	"array\nreturns the length of the array."

static value_t std_array_len(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	return make_number(get_array(args[0])->len);
}

#define STD_ARRAY_GET_DOC \
	"array, index\nreturns the item at the nth position " \
	"within the array."

static value_t std_array_get(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(1, TYPE_NUMBER);
	array_t* array = get_array(args[0]);
	size_t index = array_convert_index(
		array, get_number(args[1]), ctx);
	return array->items[(size_t)index];
}

#define STD_ARRAY_SET_DOC \
	"array, index, value\nsets the item at the nth position " \
	"within the array."

static value_t std_array_set(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	array_t* array = get_array(args[0]);
	size_t index = array_convert_index(
		array, get_number(args[1]), ctx);
	array->items[index] = args[2];
	return make_none();
}

#define STD_ARRAY_INSERT_DOC \
	"array, index, item\ninserts item into the array at the " \
	"given index."

static value_t std_array_insert(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(1, TYPE_NUMBER);
	array_t* array = get_array(args[0]);
	double index = get_number(args[1]);
	if (index < 0)
		index = array->len + 1 + index;
	if (index > array->len)
		error(ctx, ERR_INDEX(args[1]));
	array_insert(array, (size_t)index, args[2], ctx);
	return make_none();
}

#define STD_ARRAY_DEL_DOC \
	"array, index\ndeletes the item at the provided index " \
	"from the array."

static value_t std_array_del(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(1, TYPE_NUMBER);
	array_t* array = get_array(args[0]);
	size_t index = array_convert_index(
		array, get_number(args[1]), ctx);
	array_del(array, index, ctx);
	return make_none();
}

#define STD_ARRAY_POP_DOC \
	"array\ndeletes the last item from the array and " \
	"returns it."

static value_t std_array_pop(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	return array_pop(get_array(args[0]), ctx);
}

#define STD_ARRAY_HEAD_DOC \
	"array\nreturns the first item in the array."

static value_t std_array_head(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	const array_t* array = get_array(args[0]);
	if (array->len == 0)
		error(ctx, ERR_INDEX(make_number(0)));
	return array->items[0];
}

#define STD_ARRAY_TAIL_DOC \
	"array\nreturns all but the first item of the array."

static value_t std_array_tail(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	const array_t* array = get_array(args[0]);
	if (array->len == 0)
		error(ctx, ERR_INDEX(make_number(0)));
	array_t* tail = array_copy(array, false, ctx);
	array_del(tail, 0, ctx);
	return make_array(tail);
}

#define STD_ARRAY_LAST_DOC \
	"array\nreturns the last item in the array."

static value_t std_array_last(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	const array_t* array = get_array(args[0]);
	if (array->len == 0)
		error(ctx, ERR_INDEX(make_number(0)));
	return array->items[array->len - 1];
}

#define STD_ARRAY_FILL_DOC \
	"n, fn\nfills the array with n values returned from fn, " \
	"with fn taking the current count as argument."

static value_t std_array_fill(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_FREF);
	size_t n = get_number(args[0]);
	const fref_t* fref = get_fref(args[1]);
	array_t* array = array_new(ctx);
	for (size_t i = 0; i < n; i++) {
		value_t args[] = { make_number(i) };
		value_t item = lur_call_function(ctx, fref, args, 1);
		array_push(array, item, ctx);
	}
	return make_array(array);
}

#define STD_ARRAY_REPEAT_DOC \
	"rep, n\nreturns an array that contains the items " \
	"in rep repeated n times."

static value_t std_array_repeat(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(1, TYPE_NUMBER);
	const array_t* array = get_array(args[0]);
	size_t n = (size_t)get_number(args[1]);
	return make_array(array_repeat(array, n, ctx));
}

#define STD_ARRAY_ROTATE_DOC \
	"array, amount\nuse negative amounts to rotate to " \
	"the left and positive amounts to rotate to the right."

static value_t std_array_rotate(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(1, TYPE_NUMBER);
	const array_t* array = get_array(args[0]);
	int64_t n = (int64_t)get_number(args[1]);
	
	if (n < 0)
		return make_array(array_rotate_left(array, -n, ctx));
	return make_array(array_rotate_right(array, n, ctx));
}

#define STD_ARRAY_COUNT_DOC \
	"array, value\ncounts how many times a value appears " \
	"in the array."

static value_t std_array_count(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	const array_t* array = get_array(args[0]);
	size_t count = 0;
	for (size_t i = 0; i < array->len; i++)
		if (value_eq(array->items[i], args[1]))
			count++;
	return make_number(count);
}

#define STD_ARRAY_CONTAINS_DOC \
	"array, value\nreturns true if the array contains value."

static value_t std_array_contains(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	return make_bool(array_contains(
		get_array(args[0]), args[1]));
}

#define STD_ARRAY_FIND_DOC \
	"array, value, start\nfinds the first index of value in the " \
	"array after the index start."

static value_t std_array_find(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(2, TYPE_NUMBER);
	array_t* array = get_array(args[0]);
	value_t value = args[1];
	size_t start = (size_t)get_number(args[2]);
		
	if (start >= array->len) return make_none();
	for (size_t i = start; i < array->len; i++) {
		if (value_eq(array->items[i], value))
			return make_number(i);
	}
		
	return make_none();
}

#define STD_ARRAY_ITER_DOC \
	"array, fn\niterates over the array and calls fn for each " \
	"item, passing the item to fn."

static value_t std_array_iter(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(1, TYPE_FREF);
	const array_t* array = get_array(args[0]);
	const fref_t* fref = get_fref(args[1]);
	
	for (size_t i = 0; i < array->len; i++) {
		value_t args[] = { array->items[i] };
		lur_call_function(ctx, fref, args, 1);
	}
	
	return make_none();
}

#define STD_ARRAY_ITERI_DOC \
	"array, fn\niterates over the array and calls fn for each " \
	"item, passing the index and item to fn."

static value_t std_array_iteri(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(1, TYPE_FREF);
	const array_t* array = get_array(args[0]);
	const fref_t* fref = get_fref(args[1]);
	
	for (size_t i = 0; i < array->len; i++) {
		value_t args[] = { make_number(i), array->items[i] };
		lur_call_function(ctx, fref, args, 2);
	}
	
	return make_none();
}

#define STD_ARRAY_MAP_DOC \
	"array, fn\nreturns a new array constructed by " \
	"iterating over the original and passing each value " \
	"through fn."

static value_t std_array_map(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(1, TYPE_FREF);
	const array_t* array = get_array(args[0]);
	const fref_t* fref = get_fref(args[1]);
	array_t* mapped = array_new(ctx);
	
	for (size_t i = 0; i < array->len; i++) {
		value_t args[] = { array->items[i] };
		value_t result = lur_call_function(ctx, fref, args, 1);
		array_push(mapped, result, ctx);
	}
	
	return make_array(mapped);
}

#define STD_ARRAY_FILTER_DOC \
	"array, fn\npasses each value in the array to fn and " \
	"adds it to the new array if fn returns true."

static value_t std_array_filter(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(1, TYPE_FREF);
	const array_t* array = get_array(args[0]);
	const fref_t* fref = get_fref(args[1]);
	array_t* filtered = array_new(ctx);
	
	for (size_t i = 0; i < array->len; i++) {
		value_t args[] = { array->items[i] };
		value_t result = lur_call_function(ctx, fref, args, 1);
		if (result.tag == TYPE_BOOL && get_bool(result))
			array_push(filtered, array->items[i], ctx);
	}
	
	return make_array(filtered);
}

#define STD_ARRAY_FOLD_DOC \
	"array, init, fn\nreduces the list down to a single value " \
	"by passing each pair in the list, with init as the first item," \
	" to the function fn."

static value_t std_array_fold(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(2, TYPE_FREF);
	const array_t* array = get_array(args[0]);
	value_t result = args[1];
	const fref_t* fref = get_fref(args[2]);
	
	for (size_t i = 0; i < array->len; i++) {
		value_t args[] = { result, array->items[i] };
		result = lur_call_function(ctx, fref, args, 2);
	}
	
	return result;
}

#define STD_ARRAY_FLAT_DOC \
	"array\nreturns a copy of the array with any embedded " \
	"arrays flattened out."

static value_t std_array_flat(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	return make_array(array_flat(
		get_array(args[0]), ctx));
}

#define STD_ARRAY_DEDUP_DOC \
	"array\nreturns a copy of the array with any duplicates " \
	"removed."

static value_t std_array_dedup(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	array_t* array = get_array(args[0]);
	array_t* result = array_new(ctx);
	for (size_t i = 0; i < array->len; i++)
		if (!array_contains(result, array->items[i]))
			array_push(result, array->items[i], ctx);
	return make_array(result);
}

#define STD_ARRAY_SUM_DOC \
	"array\nsums all numbers in the list."

static value_t std_array_sum(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	const array_t* array = get_array(args[0]);
	
	double result = 0.0;
	for (size_t i = 0; i < array->len; i++) {
		if (array->items[i].tag != TYPE_NUMBER)
			error(ctx, ERR_TYPECHECK(array->items[i].tag,
				TYPE_NUMBER));
		result += get_number(array->items[i]);
	}
	
	return make_number(result);
}

#define STD_ARRAY_AVERAGE_DOC \
	"array\nreturns the average number in the list."

static value_t std_array_average(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	const array_t* array = get_array(args[0]);
	
	double result = 0.0;
	for (size_t i = 0; i < array->len; i++) {
		if (array->items[i].tag != TYPE_NUMBER)
			error(ctx, ERR_TYPECHECK(array->items[i].tag,
				TYPE_NUMBER));
		result += get_number(array->items[i]);
	}
	result /= array->len;
	
	return make_number(result);
}

#define STD_ARRAY_median_DOC \
	"array\nreturns the median value in the list."

static value_t std_array_median(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	const array_t* array = get_array(args[0]);
	
	double result = 0.0;
	if (array->len % 2 == 1) {
		size_t i = array->len / 2;
		if (array->items[i].tag != TYPE_NUMBER)
			error(ctx, ERR_TYPECHECK(array->items[i].tag,
				TYPE_NUMBER));
		result = get_number(array->items[i]);
	} else {
		size_t a = array->len / 2;
		size_t b = array->len / 2 - 1;
		if (array->items[a].tag != TYPE_NUMBER)
			error(ctx, ERR_TYPECHECK(array->items[a].tag,
				TYPE_NUMBER));
		if (array->items[b].tag != TYPE_NUMBER)
			error(ctx, ERR_TYPECHECK(array->items[b].tag,
				TYPE_NUMBER));
		result = (get_number(array->items[a]) +
			get_number(array->items[b])) / 2;
	}
	
	return make_number(result);
}

#define STD_ARRAY_ANY_DOC \
	"array, fn\nreturns true if the array contains any " \
	"elements that pass the predicate returned by fn."

static value_t std_array_any(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(1, TYPE_FREF);
	const array_t* array = get_array(args[0]);
	const fref_t* fref = get_fref(args[1]);
	
	for (size_t i = 0; i < array->len; i++) {
		value_t args[] = { array->items[i] };
		value_t result = lur_call_function(ctx, fref, args, 1);
		if (result.tag != TYPE_BOOL)
			error(ctx, ERR_TYPECHECK(result.tag,
				TYPE_BOOL));
		if (get_bool(result))
			return make_bool(true);
	}
	
	return make_bool(false);
}

#define STD_ARRAY_ALL_DOC \
	"array, fn\nreturns true if the array contains only " \
	"elements that pass the predicate returned by fn."

static value_t std_array_all(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(1, TYPE_FREF);
	const array_t* array = get_array(args[0]);
	const fref_t* fref = get_fref(args[1]);
	
	for (size_t i = 0; i < array->len; i++) {
		value_t args[] = { array->items[i] };
		value_t result = lur_call_function(ctx, fref, args, 1);
		if (result.tag != TYPE_BOOL)
			error(ctx, ERR_TYPECHECK(result.tag,
				TYPE_BOOL));
		if (!get_bool(result))
			return make_bool(false);
	}
	
	return make_bool(true);
}

#define STD_ARRAY_SORT_DOC \
	"array\nsorts the array using the default_sort function."

static value_t std_array_sort(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	value_t value;
	bool get = map_get(ctx->vm.globals,
		make_text(text_lit("default_sort", ctx)), &value, ctx);
	if (!get || value.tag != TYPE_FREF)
		error(ctx,  ERR_NOT_IMPLEMENTED(
			"function 'default_sort'"));
	fref_t* sorter = get_fref(value);
	return make_array(array_sort(
		get_array(args[0]), sorter, ctx));
}

#define STD_ARRAY_SORT_BY_DOC \
	"array, sort_fn\nsorts the list using a custom sort " \
	"function. the function takes a and b and should " \
	"return the smaller value."

static value_t std_array_sort_by(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(1, TYPE_FREF);
	return make_array(array_sort(
		get_array(args[0]), get_fref(args[1]), ctx));
}

#define STD_ARRAY_SWAP_DOC \
	"array, a, b\nreturns a copy of the list with elements at " \
	"indices a and b swapped."

static value_t std_array_swap(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(1, TYPE_NUMBER);
	typecheck(2, TYPE_NUMBER);
	const array_t* input = get_array(args[0]);
	array_t* output = array_copy(input, false, ctx);
	
	size_t a = array_convert_index(
		input, get_number(args[1]), ctx);
	
	size_t b = array_convert_index(
		input, get_number(args[2]), ctx);
	
	array_swap(output, a, b);
	return make_array(output);
}

#define STD_ARRAY_JOIN_DOC \
	"array\nmerges the items in the array into a single text."

static value_t std_array_join(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	return make_text(array_join(get_array(args[0]), ctx));
}

#define STD_ARRAY_ZIP_DOC \
	"zip\ntakes a list of arrays and zipper merges them."

static value_t std_array_zip(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	const array_t* arrays = get_array(args[0]);
	array_t* zip = array_new(ctx);
	
	size_t biggest = 0;
	for (size_t i = 0; i < arrays->len; i++) {
		if (arrays->items[i].tag != TYPE_ARRAY)
			error(ctx, ERR_TYPECHECK(
				arrays->items[i].tag, TYPE_ARRAY));
		
		const array_t* array = get_array(arrays->items[i]);
		biggest = (array->len > biggest) ? array->len : biggest;
	}
	
	for (size_t i = 0; i < biggest; i++) {
		for (size_t j = 0; j < arrays->len; j++) {
			const array_t* array = get_array(arrays->items[j]);
			if (i < array->len)
				array_push(zip, array->items[i], ctx);
		}
	}
	
	return make_array(zip);
}

#define STD_ARRAY_CHUNK_DOC \
	"array, size\nsplits an array into a series of smaller " \
	"chunks."

static value_t std_array_chunk(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_ARRAY);
	typecheck(1, TYPE_NUMBER);
	array_t* original = get_array(args[0]);
	size_t n = (size_t)get_number(args[1]);
	array_t* chunks = array_new(ctx);
	
	for (size_t i = 0; i < original->len; i += n) {
		array_t* chunk = array_new(ctx);
		for (size_t j = 0; j < n; j++) {
			array_push(chunk, original->items[i + j], ctx);
			if (i + j == original->len - 1)
				break;
		}
		array_push(chunks, make_array(chunk), ctx);
	}
	
	return make_array(chunks);
}

#define STD_MAP_LEN_DOC \
	"map\nreturns the length of the map."

static value_t std_map_len(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_MAP);
	return make_number(get_map(args[0])->len);
}

#define STD_MAP_GET_DOC \
	"map, key\nreturns the value associated with a key."

static value_t std_map_get(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_MAP);
	const map_t* map = get_map(args[0]);
	value_t value;
	if (!map_get(map, args[1], &value, ctx))
		error(ctx, ERR_INDEX(args[1]));
	return value;
}

#define STD_MAP_SET_DOC \
	"map, key, value\ncreates or updates the key/value pair."

static value_t std_map_set(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_MAP);
	map_set(get_map(args[0]), args[1], args[2], ctx);
	return make_none();
}

#define STD_MAP_DEL_DOC \
	"map, key\ndeletes the key/value pair associated with " \
	"the key."

static value_t std_map_del(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_MAP);
	map_del(get_map(args[0]), args[1], ctx);
	return make_none();
}

#define STD_MAP_HAS_KEY_DOC \
	"map, key\nreturns true if the map contains the " \
	"given key."

static value_t std_map_has_key(value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_MAP);
	const map_t* map = get_map(args[0]);
	
	for (size_t i = 0; i < map->len; i++) {
		const map_entry_t* entry = &map->entries[i];
		if (entry->key.tag == TYPE_NONE) continue;
		if (value_eq(entry->key, args[1]))
			return make_bool(true);
	}
	
	return make_bool(false);
}

#define STD_MAP_HAS_VALUE_DOC \
	"map, value\nreturns true if the map contains the " \
	"given value."

static value_t std_map_has_value(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_MAP);
	const map_t* map = get_map(args[0]);
	
	for (size_t i = 0; i < map->len; i++) {
		const map_entry_t* entry = &map->entries[i];
		if (entry->key.tag == TYPE_NONE) continue;
		if (value_eq(entry->value, args[1]))
			return make_bool(true);
	}
	
	return make_bool(false);
}

#define STD_MAP_KEYS_DOC \
	"map\nreturns an array containing all the keys " \
	"of the map."

static value_t std_map_keys(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_MAP);
	const map_t* map = get_map(args[0]);
	array_t* keys = array_new(ctx);
	
	for (size_t i = 0; i < map->len; i++) {
		const map_entry_t* entry = &map->entries[i];
		if (entry->key.tag == TYPE_NONE) continue;
		array_push(keys, entry->key, ctx);
	}
	
	return make_array(keys);
}

#define STD_MAP_VALUES_DOC \
	"map\nreturns an array containing all the values " \
	"of the map."

static value_t std_map_values(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_MAP);
	const map_t* map = get_map(args[0]);
	array_t* values = array_new(ctx);
	
	for (size_t i = 0; i < map->len; i++) {
		const map_entry_t* entry = &map->entries[i];
		if (entry->key.tag == TYPE_NONE) continue;
		array_push(values, entry->value, ctx);
	}
	
	return make_array(values);
}

#define STD_MAP_KV_PAIRS_DOC \
	"map\nreturns a list containing all key/value pairs."

static value_t std_map_kv_pairs(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_MAP);
	const map_t* map = get_map(args[0]);
	array_t* pairs = array_new(ctx);
	
	for (size_t i = 0; i < map->len; i++) {
		const map_entry_t* entry = &map->entries[i];
		if (entry->key.tag == TYPE_NONE) continue;
		if (entry->value.tag == TYPE_MAP &&
			map == get_map(entry->value))
			continue;
		
		array_t* pair = array_new(ctx);
		array_push(pair, entry->key, ctx);
		array_push(pair, entry->value, ctx);
		array_push(pairs, make_array(pair), ctx);
	}
	
	return make_array(pairs);
}

#define STD_MAP_FROM_KV_PAIRS_DOC \
	"kv_pairs\ncreates a map out of an array of " \
	"key/value pairs"

static value_t std_map_from_kv_pairs(
	value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_ARRAY);
	const array_t* pairs = get_array(args[0]);
	map_t* map = map_new(ctx);
	
	for (size_t i = 0; i  < pairs->len; i++) {
		if (pairs->items[i].tag != TYPE_ARRAY)
			error(ctx, ERR_TYPECHECK(
				pairs->items[i].tag, TYPE_ARRAY));
		
		const array_t* pair = get_array(pairs->items[i]);
		if (pair->len != 2)
			continue;
			
		map_set(map, pair->items[0], pair->items[1], ctx);
	}
	
	return make_map(map);
}

#define STD_MAP_ITER_DOC \
	"map, fn\niterates over all entries in the map, passing " \
	"each key and value to fn."

static value_t std_map_iter(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_MAP);
	typecheck(1, TYPE_FREF);
	const map_t* map = get_map(args[0]);
	const fref_t* fref = get_fref(args[1]);
	
	for (size_t i = 0; i < map->len; i++) {
		const map_entry_t* entry = &map->entries[i];
		if (entry->key.tag == TYPE_NONE) continue;
		value_t args[] = { entry->key, entry->value };
		lur_call_function(ctx, fref, args, 2);
	}
	
	return make_none();
}

#define STD_MAP_ITERI_DOC \
	"map\niterates over all entries in the map, passing " \
	"each key and value, alongside the current index to fn."

static value_t std_map_iteri(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_MAP);
	typecheck(1, TYPE_FREF);
	const map_t* map = get_map(args[0]);
	const fref_t* fref = get_fref(args[1]);
	
	size_t index = 0;
	for (size_t i = 0; i < map->len; i++) {
		const map_entry_t* entry = &map->entries[i];
		if (entry->key.tag == TYPE_NONE) continue;
		value_t args[] = {
			make_number(index), entry->key, entry->value };
		lur_call_function(ctx, fref, args, 3);
		index++;
	}
	
	return make_none();
}

#define STD_MAP_GET_OR_DEFAULT_DOC \
	"map, key, default\ntries to get a value from the key, " \
	"returning default if not found."

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

lur_config_t lur_default_config(void);
lur_t* lur_new(lur_config_t, int, char**);
void lur_free(lur_t*);

void* thread_spawn(void* ptr) {
	lur_t* prev_ctx = (lur_t*)ptr;
	lur_t* ctx = lur_new(prev_ctx->cfg, 0, NULL);
	
	*ctx->vm.sp++ = make_fref(prev_ctx->thread_fref);
	for (size_t i = 0; i < prev_ctx->thread_args->len; i++)
		*ctx->vm.sp++ = prev_ctx->thread_args->items[i];
	
	ctx->running = true;
	prev_ctx->thread_retval = vm_launch(&ctx->vm,
		make_fref(prev_ctx->thread_fref),
		prev_ctx->thread_args->len,
		true);
	ctx->running = false;
	
//	lur_free(ctx); TODO: fix
	return NULL;
}

#define STD_THREAD_SPAWN_DOC \
	"fn, args\nspawns a new thread that calls the function " \
	"fn with the specified arguments."

static value_t std_thread_spawn(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_FREF);
	typecheck(1, TYPE_ARRAY);

	ctx->thread_fref = get_fref(args[0]);
	ctx->thread_args = get_array(args[1]);
	
	thread_t* thread = NULL;
	ssize_t thread_index = -1;
	for (size_t i = 0; i < MAX_THREADS; i++) {
		if (ctx->vm.threads[i].is_free) {
			thread = &ctx->vm.threads[i];
			thread_index = i;
			break;
		}
	}
	
	if (thread_index == -1)
		error(ctx, ERR_LIMIT("max thread", MAX_THREADS));
	
	int result = pthread_create(
		&thread->id, NULL, thread_spawn, (void*)ctx);
	
	if (result == -1)
		error(ctx, ERR_THREAD_SPAWN_FAILED(result));
	
	thread->is_free = false;
	return make_number(thread_index);
}

#define STD_THREAD_JOIN_DOC \
	"thread\njoins the thread with the specified id to the " \
	"main thread, waiting for it to complete its work."

static value_t std_thread_join(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	size_t id = (size_t)get_number(args[0]);
	if (id > MAX_THREADS ||
		ctx->vm.threads[id].is_free)
		error(ctx, ERR_INVALID_THREAD_ID(id));
		
	thread_t* thread = &ctx->vm.threads[id];
	pthread_join(thread->id, NULL);
	thread->is_free = true;
	return ctx->thread_retval;
}

static text_t* io_read(const text_t* path, lur_t* ctx) {
	assert(path && ctx);
	FILE* fp = fopen((const char*)path->buffer, "r");
	if (!fp) error(ctx, ERR_READ_FAILED(path));
	
	fseek(fp, 0, SEEK_END);
	size_t len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	text_t* text = text_new(NULL, len, ctx);
	fread(text->buffer, 1, len + 1, fp);
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
		(const uint8_t*)line, strlen(line) + 1, ctx);
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

#define STD_IO_PRINT_DOC \
	"value\nprints the provided value to standard output."

static value_t std_io_print(value_t* args, lur_t* ctx) {
	value_print(args[0], ctx);
	lur_printf("\n");
	return make_none();
}

#define STD_IO_INPUT_DOC \
	"returns a line read from standard input as text."

static value_t std_io_input(value_t* args, lur_t* ctx) {
	return make_text(io_read_stdin(ctx));
}

#define STD_IO_READ_DOC \
	"path\nreturns text read from the provided path."

static value_t std_io_read(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	return make_text(io_read(get_text(args[0]), ctx));
}

#define STD_IO_WRITE_DOC \
	"path, contents\nwrites content to the provided path."

static value_t std_io_write(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_TEXT);
	io_write(get_text(args[0]), get_text(args[1]), ctx);
	return make_none();
}

#define STD_IO_APPEND_DOC \
	"path, contents\nappends contents to the provided path."

static value_t std_io_append(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	typecheck(1, TYPE_TEXT);
	io_append(get_text(args[0]), get_text(args[1]), ctx);
	return make_none();
}

#define STD_IO_SIZE_DOC \
	"path\nreturns the size of the file at path."

static value_t std_io_size(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	return make_number(
		io_file_size(get_text(args[0]), ctx));
}

#define STD_IO_DIR_EXISTS_DOC \
	"path\nreturns true if a directory exists at path."

static value_t std_io_dir_exists(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	const text_t* path = get_text(args[0]);
	struct stat st = {0};
	return make_bool(
		stat((const char*)path->buffer, &st) != -1);
	return make_none();
}

#define STD_IO_MAKE_DIR_DOC \
	"path\ncreates an empty directory at path."

static value_t std_io_make_dir(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	const text_t* path = get_text(args[0]);
	mkdir((const char*)path->buffer, 0700);
	return make_none();
}

#define STD_IO_LIST_DIR_DOC \
	"path\nreturns an array containing all files found in the " \
	"directory, use path '.' for current directory."

static value_t std_io_list_dir(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	const text_t* path = get_text(args[0]);
	array_t* files = array_new(ctx);
	
	DIR* dir;
	struct dirent* entry;
	if ((dir = opendir((const char*)path->buffer)) != NULL) {
		while ((entry = readdir(dir)) != NULL) {
		 	const text_t* file = text_lit(entry->d_name, ctx);
		 	array_push(files, make_text(file), ctx);
		}
		closedir(dir);
		return make_array(files);
	}
	
	error(ctx, ERR_OPEN_FAILED(
		path, text_lit("list_dir", ctx)));
	return make_none();
}

#define STD_IO_DEL_DOC \
	"path\ndeletes the file or directory at the given path."

static value_t std_io_del(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_TEXT);
	remove((char*)get_text(args[0])->buffer);
	return make_none();
}

#define STD_SOCKET_CREATE_DOC \
	"type\ncreates a new socket of the specified type."

static value_t std_socket_create(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	int type = (int)get_number(args[0]);
	int handle = socket(AF_INET,
		(type == 0) ? SOCK_STREAM : SOCK_DGRAM, 0);
	
	if (handle == -1)
		error(ctx, ERR_SOCKET_CREATE(strerror(errno)));
		
	if (setsockopt(handle,
		SOL_SOCKET,
		SO_REUSEADDR,
		&(int){1},
		sizeof(int)) == -1)
		error(ctx, ERR_SOCKET_CREATE(strerror(errno)));
	
	return make_number(handle);
}

#define STD_SOCKET_CONNECT_DOC \
	"socket, ip, port\nconnects the socket to the specified " \
	"ip:port."

static value_t std_socket_connect(value_t* args, lur_t* ctx)
{
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_TEXT);
	typecheck(2, TYPE_NUMBER);
	
	int handle = (int)get_number(args[0]);
	const text_t* ip = get_text(args[1]);
	int port = (int)get_number(args[2]);
	
	struct hostent* ent = gethostbyname(
		(const char*)ip->buffer);
	if (!ent)
		error(ctx, ERR_SOCKET_RESOLVE_FAILED(ip));
	
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	memcpy(&addr.sin_addr,
		ent->h_addr_list[0], ent->h_length);
	addr.sin_port = htons(port);

	int result = connect(handle,
		(struct sockaddr*)&addr, sizeof(addr));
	if (result == -1)
		error(ctx, ERR_SOCKET_CONNECT(ip, port,
			strerror(errno)));
	return make_number(result);
}

#define STD_SOCKET_BIND_DOC \
	"socket, ip, port\nbinds the socket to ip:port, " \
	"used for servers."

static value_t std_socket_bind(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_TEXT);
	typecheck(2, TYPE_NUMBER);
	
	int handle = (int)get_number(args[0]);
	const text_t* ip = get_text(args[1]);
	int port = (int)get_number(args[2]);
	
	struct hostent* ent = gethostbyname(
		(const char*)ip->buffer);
	if (!ent)
		error(ctx, ERR_SOCKET_RESOLVE_FAILED(ip));
	
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	memcpy(&addr.sin_addr,
		ent->h_addr_list[0], ent->h_length);
	addr.sin_port = htons(port);
	
	int result = bind(handle,
		(struct sockaddr*)&addr, sizeof(addr));
	if (result == -1)
		error(ctx, ERR_SOCKET_BIND(ip, port, 
			strerror(errno)));
	return make_none();
}

#define STD_SOCKET_LISTEN_DOC \
	"socket, backlog\nsets up a listen server on the socket, " \
	"with a backlog of max backlog connections."

static value_t std_socket_listen(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_NUMBER);
	int handle = (int)get_number(args[0]);
	double backlog = get_number(args[1]);
	int result = listen(handle, backlog);
	if (result == -1)
		error(ctx, ERR_SOCKET_LISTEN_FAILED(
			strerror(errno)));
	return make_none();
}

#define STD_SOCKET_ACCEPT_DOC \
	"socket\naccepts an incoming connection request, " \
	"returns a new socket handle used for sending data " \
	"to the client."
 
static value_t std_socket_accept(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	int server = (int)get_number(args[0]);
	
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(25565);

	int result = accept(server, NULL, NULL);
	if (result == -1)
		error(ctx, ERR_SOCKET_ACCEPT_FAILED(
			strerror(errno)));
	return make_number(result);
}

#define STD_SOCKET_SEND_DOC \
	"socket, text\nsends text over the network."

static value_t std_socket_send(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	typecheck(1, TYPE_TEXT);
	int handle = (int)get_number(args[0]);
	const text_t* data = get_text(args[1]);
	int result = send(handle, data->buffer, data->len + 1, 0);
	if (result == -1)
		error(ctx, ERR_SOCKET_SEND_FAILED(
			strerror(errno)));
	return make_none();
}

#define STD_SOCKET_RECV_DOC \
	"socket\nreceives data sent over the network as text."

static value_t std_socket_recv(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	int handle = (int)get_number(args[0]);
	
	char* buffer[8192];
	ptrdiff_t result = recv(handle, buffer, 8192, 0);
	if (result == -1)
		error(ctx, ERR_SOCKET_RECV_FAILED(
			strerror(errno)));
	return make_text(text_new(
		(const uint8_t*)buffer, result - 1, ctx));
}

#define STD_SOCKET_CLOSE_DOC \
	"socket\ncloses and disconnects a socket."

static value_t std_socket_close(value_t* args, lur_t* ctx) {
	typecheck(0, TYPE_NUMBER);
	int handle = (int)get_number(args[0]);
	close(handle);
	return make_none();
}

#undef typecheck

static const map_t* lur_new_enum(
	lur_t* ctx, const char* items[], size_t len)
{
	map_t* enum_ = map_new(ctx);
	for (size_t i = 0; i < len; i++)
		map_set(enum_,
			make_text(text_lit(items[i], ctx)),
			make_number(i),
			ctx);
	return enum_;
}

static void stdvar_add(
	lur_t* ctx,
	const char* name,
	value_t value,
	const char* help_text)
{
	if (!ctx->std_map) {
		map_set(ctx->vm.globals,
			make_text(text_lit(name, ctx)),
			value,
			ctx);
	} else {
		map_set(ctx->std_map,
			make_text(text_lit(name, ctx)),
			value,
			ctx);
	}
	
	text_t* help_key;
	if (!ctx->std_map_name)
		help_key = text_lit(name, ctx);
	else
		help_key = text_fmt(ctx, "%s.%s",
			ctx->std_map_name, name);
			
	value_t text_value;
	if (!help_text)
		text_value = make_none();
	else
		text_value = make_text(text_lit(help_text, ctx));
		
	map_set(ctx->help,
		make_text(help_key), text_value, ctx);
}

static void stdfun_add(
	lur_t* ctx, const char* name, 
	uint8_t argc, syscall_fn_t fn,
	const char* help_text)
{
	func_t* syscall = func_new(ctx);
	syscall->name = text_lit(name, ctx);
	syscall->argc = argc;
	syscall->syscall = fn;
	
	stdvar_add(ctx, name,
		make_fref(fref_new(syscall, ctx)), help_text);
}

static void std_set_module(
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
	
	stdvar_add(ctx, "NAMES",
		make_map(ctx->vm.globals),
		"map containing all global names.");
	stdfun_add(ctx, "help", 1, std_help, STD_HELP_DOC);
	stdfun_add(ctx, "load", 1, std_load, STD_LOAD_DOC);
	stdfun_add(ctx, "eval", 1, std_eval, STD_EVAL_DOC);
	stdfun_add(ctx, "error", 1, std_error, STD_ERROR_DOC);
	stdvar_add(ctx, "type",
		make_map(lur_new_enum(ctx, TYPE_NAMES, 7)),
		"type names.");
	stdfun_add(ctx, "type_of", 1, std_type_of,
		STD_TYPE_OF_DOC);
	stdfun_add(ctx, "default", 1, std_default,
		STD_DEFAULT_DOC);
	stdfun_add(ctx, "serialize", 1, std_serialize,
		STD_SERIALIZE_DOC);
	stdfun_add(ctx, "deserialize", 1, std_deserialize,
		STD_DESERIALIZE_DOC);
	stdfun_add(ctx, "as_number", 1, std_as_number,
		STD_AS_NUMBER_DOC);
	stdfun_add(ctx, "as_text", 1, std_as_text,
		STD_AS_TEXT_DOC);
	stdfun_add(ctx, "loop", 2, std_loop,
		STD_LOOP_DOC);
	stdfun_add(ctx, "while", 1, std_while,
		STD_WHILE_DOC);
	stdfun_add(ctx, "range", 2, std_range,
		STD_RANGE_DOC);
	stdfun_add(ctx, "range_inc", 2, std_range_inc,
		STD_RANGE_INC_DOC);
	stdfun_add(ctx, "default_sort", 2, std_default_sort,
		STD_DEFAULT_SORT_DOC);
	stdfun_add(ctx, "enum", 1, std_enum,
		STD_ENUM_DOC);
	stdfun_add(ctx, "assert", 1, std_assert,
		STD_ASSERT_DOC);
	
	std_set_module(ctx, "mem");
	stdfun_add(ctx, "addr", 1, std_mem_addr,
		STD_MEM_ADDR_DOC);
	stdfun_add(ctx, "copy", 1, std_mem_copy,
		STD_MEM_COPY_DOC);
	stdfun_add(ctx, "deep_copy", 1, std_mem_deep_copy,
		STD_MEM_DEEP_COPY_DOC);
	stdfun_add(ctx, "total_ram", 0, std_mem_total_ram,
		STD_MEM_TOTAL_RAM_DOC);
	stdfun_add(ctx, "free_ram", 0, std_mem_free_ram,
		STD_MEM_FREE_RAM_DOC);
	std_set_module(ctx, NULL);
	
	std_set_module(ctx, "time");
	stdfun_add(ctx, "now", 0, std_time_now,
		STD_TIME_NOW_DOC);
	stdfun_add(ctx, "year", 0, std_time_year,
		STD_TIME_YEAR_DOC);
	stdfun_add(ctx, "month", 0, std_time_month,
		STD_TIME_MONTH_DOC);
	stdfun_add(ctx, "day", 0, std_time_day,
		STD_TIME_DAY_DOC);
	stdfun_add(ctx, "hour", 0, std_time_hour,
		STD_TIME_HOUR_DOC);
	stdfun_add(ctx, "minute", 0, std_time_minute,
		STD_TIME_MINUTE_DOC);
	stdfun_add(ctx, "second", 0, std_time_second,
		STD_TIME_SECOND_DOC);
	stdfun_add(ctx, "timestamp", 0, std_time_timestamp,
		STD_TIME_TIMESTAMP_DOC);
	stdfun_add(ctx, "clock", 0, std_time_clock,
		STD_TIME_CLOCK_DOC);
	stdfun_add(ctx, "sleep", 1, std_time_sleep,
		STD_TIME_SLEEP_DOC);
	std_set_module(ctx, NULL);
	
	std_set_module(ctx, "system");
	stdvar_add(ctx, "VERSION",
		make_text(text_lit(LUR_VERSION, ctx)),
		"lur version.");
	stdvar_add(ctx, "PLATFORM",
		make_text(text_lit(system_get_platform(), ctx)),
		"operating system.");
	stdvar_add(ctx, "COMPILER_VERSION",
		make_text(text_lit(__VERSION__, ctx)),
		"compiler version.");
	stdvar_add(ctx, "BUILD_TIME",
		make_text(text_lit(__TIMESTAMP__, ctx)),
		"compile time.");
	stdvar_add(ctx, "args", make_array(ctx->args),
		"command line arguments.");
	stdfun_add(ctx, "uptime", 0, std_system_uptime,
		STD_SYSTEM_UPTIME_DOC);
	stdfun_add(ctx, "average_load", 0,
		std_system_average_load,
		STD_SYSTEM_AVERAGE_LOAD_DOC);
	stdfun_add(ctx, "process_count", 0,
		std_system_process_count,
		STD_SYSTEM_PROCESS_COUNT_DOC);
	stdfun_add(ctx, "cmd", 1, std_system_cmd,
		STD_SYSTEM_CMD_DOC);
	stdfun_add(ctx, "exit", 1, std_system_exit,
		STD_SYSTEM_EXIT_DOC);
	std_set_module(ctx, NULL);
	
	std_set_module(ctx, "math");
	stdvar_add(ctx, "PI", make_number(M_PI),
		"the value of pi (π).");
	stdvar_add(ctx, "E", make_number(M_E),
		"Euler's number.");
	stdvar_add(ctx, "INF", make_number(INFINITY),
		"a value representing infinity.");
	stdvar_add(ctx, "NAN", make_number(NAN),
		"value representing Not a Number.");
	stdfun_add(ctx, "eqf", 3, std_math_eqf,
		STD_MATH_EQF_DOC);
	stdfun_add(ctx, "bit_shl", 2, std_math_bit_shl,
		STD_MATH_BIT_SHL_DOC);
	stdfun_add(ctx, "bit_shr", 2, std_math_bit_shr,
		STD_MATH_BIT_SHR_DOC);
	stdfun_add(ctx, "bit_and", 2, std_math_bit_and,
		STD_MATH_BIT_AND_DOC);
	stdfun_add(ctx, "bit_or", 2, std_math_bit_or,
		STD_MATH_BIT_OR_DOC);
	stdfun_add(ctx, "bit_xor", 2, std_math_bit_xor,
		STD_MATH_BIT_XOR_DOC);
	stdfun_add(ctx, "bit_not", 1, std_math_bit_not,
		STD_MATH_BIT_NOT_DOC);
	stdfun_add(ctx, "log", 2, std_math_log,
		STD_MATH_LOG_DOC);
	stdfun_add(ctx, "sqrt", 1, std_math_sqrt,
		STD_MATH_SQRT_DOC);
	stdfun_add(ctx, "squared", 1, std_math_squared,
		STD_MATH_SQUARED_DOC);
	stdfun_add(ctx, "abs", 1, std_math_abs,
		STD_MATH_ABS_DOC);
	stdfun_add(ctx, "min", 2, std_math_min,
		STD_MATH_MIN_DOC);
	stdfun_add(ctx, "max", 2, std_math_max,
		STD_MATH_MAX_DOC);
	stdfun_add(ctx, "clip", 3, std_math_clip,
		STD_MATH_CLIP_DOC);
	stdfun_add(ctx, "lerp", 3, std_math_lerp,
		STD_MATH_LERP_DOC);
	stdfun_add(ctx, "interp_to", 4, std_math_interp_to,
		STD_MATH_INTERP_TO_DOC);
	stdfun_add(ctx, "floor", 1, std_math_floor,
		STD_MATH_FLOOR_DOC);
	stdfun_add(ctx, "ceil", 1, std_math_ceil,
		STD_MATH_CEIL_DOC);
	stdfun_add(ctx, "round", 1, std_math_round,
		STD_MATH_ROUND_DOC);
	stdfun_add(ctx, "rad", 1, std_math_rad,
		STD_MATH_RAD_DOC);
	stdfun_add(ctx, "deg", 1, std_math_deg,
		STD_MATH_DEG_DOC);
	stdfun_add(ctx, "sin", 1, std_math_sin,
		STD_MATH_SIN_DOC);
	stdfun_add(ctx, "cos", 1, std_math_cos,
		STD_MATH_COS_DOC);
	stdfun_add(ctx, "tan", 1, std_math_tan,
		STD_MATH_TAN_DOC);
	stdfun_add(ctx, "asin", 1, std_math_asin,
		STD_MATH_ASIN_DOC);
	stdfun_add(ctx, "acos", 1, std_math_acos,
		STD_MATH_ACOS_DOC);
	stdfun_add(ctx, "atan", 1, std_math_atan,
		STD_MATH_ATAN_DOC);
	stdfun_add(ctx, "atan2", 2, std_math_atan2,
		STD_MATH_ATAN2_DOC);
	stdfun_add(ctx, "sinh", 1, std_math_sinh,
		STD_MATH_SINH_DOC);
	stdfun_add(ctx, "cosh", 1, std_math_cosh,
		STD_MATH_COSH_DOC);
	stdfun_add(ctx, "tanh", 1, std_math_tanh,
		STD_MATH_TANH_DOC);
	stdfun_add(ctx, "asinh", 1, std_math_asinh,
		STD_MATH_ASINH_DOC);
	stdfun_add(ctx, "acosh", 1, std_math_acosh,
		STD_MATH_ACOSH_DOC);
	stdfun_add(ctx, "atanh", 1, std_math_atanh,
		STD_MATH_ATANH_DOC);
	stdfun_add(ctx, "is_even", 1, std_math_is_even,
		STD_MATH_IS_EVEN_DOC);
	stdfun_add(ctx, "is_odd", 1, std_math_is_odd,
		STD_MATH_IS_ODD_DOC);
	stdfun_add(ctx, "is_prime", 1, std_math_is_prime,
		STD_MATH_IS_PRIME_DOC);
	stdfun_add(ctx, "hex", 1, std_math_hex,
		STD_MATH_HEX_DOC);
	stdfun_add(ctx, "bin", 1, std_math_bin,
		STD_MATH_BIN_DOC);
	stdfun_add(ctx, "hash", 1, std_math_hash,
		STD_MATH_HASH_DOC);
	std_set_module(ctx, NULL);
	
	std_set_module(ctx, "vec");
	stdvar_add(ctx, "X", make_array(lur_new_num_array(
		(const double[]){1, 0, 0}, 3, ctx)), "global X axis");
	stdvar_add(ctx, "Y", make_array(lur_new_num_array(
		(const double[]){0, 1, 0}, 3, ctx)), "global Y axis");
	stdvar_add(ctx, "Z", make_array(lur_new_num_array(
		(const double[]){0, 0, 1}, 3, ctx)), "global Z axis");
	stdfun_add(ctx, "size", 1, std_vec_size,
		STD_VEC_SIZE_DOC);
	stdfun_add(ctx, "norm", 1, std_vec_norm,
		STD_VEC_NORM_DOC);
	stdfun_add(ctx, "dot", 2, std_vec_dot,
		STD_VEC_DOT_DOC);
	stdfun_add(ctx, "cross", 2, std_vec_cross,
		STD_VEC_CROSS_DOC);
	stdfun_add(ctx, "interp_to", 4, std_vec_interp_to,
		STD_VEC_INTERP_TO_DOC);
	std_set_module(ctx, NULL);
	
	std_set_module(ctx, "rand");
	stdfun_add(ctx, "seed", 1, std_rand_seed,
		STD_RAND_SEED_DOC);
	stdfun_add(ctx, "number", 2, std_rand_number,
		STD_RAND_NUMBER_DOC);
	stdfun_add(ctx, "text", 2, std_rand_text,
		STD_RAND_TEXT_DOC);
	stdfun_add(ctx, "item", 1, std_rand_item,
		STD_RAND_ITEM_DOC);
	std_set_module(ctx, NULL);
	
	std_set_module(ctx, "text");
	stdvar_add(ctx, "LETTERS", make_text(
		text_lit("abcdefghijklmnopqrstuvwxyz", ctx)),
		"all ASCII letters, in lowercase, a-z.");
	stdvar_add(ctx, "DIGITS", make_text(
		text_lit("0123456789", ctx)),
		"all ASCII digits, 0-9.");
	stdvar_add(ctx, "PUNCTUATION", make_text(
		text_lit("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~", ctx)), 
		"all ASCII punctuation characters.");
	stdvar_add(ctx, "WHITESPACE", make_text(
		text_lit(" \t\n\r", ctx)),
		"all whitespace characters.");
	stdvar_add(ctx, "NEWLINE", make_text(
		text_lit("\n", ctx)),
		"newline string for the system.");
	stdfun_add(ctx, "len", 1, std_text_len,
		STD_TEXT_LEN_DOC);
	stdfun_add(ctx, "cmp", 2, std_text_cmp,
		STD_TEXT_CMP_DOC);
	stdfun_add(ctx, "bytes", 1, std_text_bytes,
		STD_TEXT_BYTES_DOC);
	stdfun_add(ctx, "ascii", 1, std_text_ascii,
		STD_TEXT_ASCII_DOC);
	stdfun_add(ctx, "words", 2, std_text_words,
		STD_TEXT_WORDS_DOC);
	stdfun_add(ctx, "slice", 3, std_text_slice,
		STD_TEXT_SLICE_DOC);
	stdfun_add(ctx, "repeat", 2, std_text_repeat,
		STD_TEXT_REPEAT_DOC);
	stdfun_add(ctx, "as_upper", 1, std_text_as_upper,
		STD_TEXT_AS_UPPER_DOC);
	stdfun_add(ctx, "as_lower", 1, std_text_as_lower,
		STD_TEXT_AS_LOWER_DOC);
	stdfun_add(ctx, "is_upper", 1, std_text_is_upper,
		STD_TEXT_IS_UPPER_DOC);
	stdfun_add(ctx, "is_lower", 1, std_text_is_lower,
		STD_TEXT_IS_LOWER_DOC);
	stdfun_add(ctx, "starts_with", 2, std_text_starts_with,
		STD_TEXT_STARTS_WITH_DOC);
	stdfun_add(ctx, "ends_with", 2, std_text_ends_with,
		STD_TEXT_ENDS_WITH_DOC);
	stdfun_add(ctx, "trim_left", 1, std_text_trim_left,
		STD_TEXT_TRIM_LEFT_DOC);
	stdfun_add(ctx, "trim_right", 1, std_text_trim_right,
		STD_TEXT_TRIM_RIGHT_DOC);
	stdfun_add(ctx, "trim", 1,  std_text_trim,
		STD_TEXT_TRIM_DOC);
	stdfun_add(ctx, "left_pad", 3, std_text_left_pad,
		STD_TEXT_LEFT_PAD_DOC);
	stdfun_add(ctx, "right_pad", 3, std_text_right_pad,
		STD_TEXT_RIGHT_PAD_DOC);
	stdfun_add(ctx, "pad", 3,  std_text_pad,
		STD_TEXT_PAD_DOC);
	stdfun_add(ctx, "find", 3, std_text_find,
		STD_TEXT_FIND_DOC);
	stdfun_add(ctx, "replace_all", 3, std_text_replace_all,
		STD_TEXT_REPLACE_ALL_DOC);
	stdfun_add(ctx, "edit_dist", 2, std_text_edit_dist,
		STD_TEXT_EDIT_DIST_DOC);
	stdfun_add(ctx, "ratio", 2, std_text_ratio,
		STD_TEXT_RATIO_DOC);
	std_set_module(ctx, NULL);
	
	std_set_module(ctx, "array");
	stdfun_add(ctx, "len", 1, std_array_len,
		STD_ARRAY_LEN_DOC);
	stdfun_add(ctx, "get", 2, std_array_get,
		STD_ARRAY_GET_DOC);
	stdfun_add(ctx, "set", 3, std_array_set,
		STD_ARRAY_SET_DOC);
	stdfun_add(ctx, "insert", 3, std_array_insert,
		STD_ARRAY_INSERT_DOC);
	stdfun_add(ctx, "del", 2, std_array_del,
		STD_ARRAY_DEL_DOC);
	stdfun_add(ctx, "pop", 1, std_array_pop,
		STD_ARRAY_POP_DOC);
	stdfun_add(ctx, "head", 1, std_array_head,
		STD_ARRAY_HEAD_DOC);
	stdfun_add(ctx, "tail", 1, std_array_tail,
		STD_ARRAY_TAIL_DOC);
	stdfun_add(ctx, "last", 1, std_array_last,
		STD_ARRAY_LAST_DOC);
	stdfun_add(ctx, "fill", 2, std_array_fill,
		STD_ARRAY_FILL_DOC);
	stdfun_add(ctx, "repeat", 2, std_array_repeat,
		STD_ARRAY_REPEAT_DOC);
	stdfun_add(ctx, "rotate", 2, std_array_rotate,
		STD_ARRAY_ROTATE_DOC);
	stdfun_add(ctx, "count", 2, std_array_count,
		STD_ARRAY_COUNT_DOC);
	stdfun_add(ctx, "contains", 2, std_array_contains,
		STD_ARRAY_CONTAINS_DOC);
	stdfun_add(ctx, "find", 3, std_array_find,
		STD_ARRAY_FIND_DOC);
	stdfun_add(ctx, "iter", 2, std_array_iter,
		STD_ARRAY_ITER_DOC);
	stdfun_add(ctx, "iteri", 2, std_array_iteri,
		STD_ARRAY_ITERI_DOC);
	stdfun_add(ctx, "map", 2, std_array_map,
		STD_ARRAY_FILTER_DOC);
	stdfun_add(ctx, "filter", 2, std_array_filter,
		STD_ARRAY_FILTER_DOC);
	stdfun_add(ctx, "fold", 3, std_array_fold,
		STD_ARRAY_FOLD_DOC);
	stdfun_add(ctx, "flat", 1, std_array_flat,
		STD_ARRAY_FLAT_DOC);
	stdfun_add(ctx, "dedup", 1, std_array_dedup,
		STD_ARRAY_DEDUP_DOC);
	stdfun_add(ctx, "sum", 1, std_array_sum,
		STD_ARRAY_SUM_DOC);
	stdfun_add(ctx, "average", 1, std_array_average,
		STD_ARRAY_AVERAGE_DOC);
	stdfun_add(ctx, "median", 1, std_array_median,
		STD_ARRAY_median_DOC);
	stdfun_add(ctx, "any", 2, std_array_any,
		STD_ARRAY_ANY_DOC);
	stdfun_add(ctx, "all", 2, std_array_all,
		STD_ARRAY_ALL_DOC);
	stdfun_add(ctx, "sort", 1, std_array_sort,
		STD_ARRAY_SORT_DOC);
	stdfun_add(ctx, "sort_by", 2, std_array_sort_by,
		STD_ARRAY_SORT_BY_DOC);
	stdfun_add(ctx, "swap", 3, std_array_swap,
		STD_ARRAY_SWAP_DOC);
	stdfun_add(ctx, "join", 1, std_array_join,
		STD_ARRAY_JOIN_DOC);
	stdfun_add(ctx, "zip", 1, std_array_zip,
		STD_ARRAY_ZIP_DOC);
	stdfun_add(ctx, "chunk", 2, std_array_chunk,
		STD_ARRAY_CHUNK_DOC);
	std_set_module(ctx, NULL);
	
	std_set_module(ctx, "map");
	stdfun_add(ctx, "len", 1, std_map_len,
		STD_MAP_LEN_DOC);
	stdfun_add(ctx, "get", 2, std_map_get,
		STD_MAP_GET_DOC);
	stdfun_add(ctx, "set", 3, std_map_set,
		STD_MAP_SET_DOC);
	stdfun_add(ctx, "del", 2, std_map_del,
		STD_MAP_DEL_DOC);
	stdfun_add(ctx, "has_key", 2, std_map_has_key,
		STD_MAP_HAS_KEY_DOC);
	stdfun_add(ctx, "has_value", 2, std_map_has_value,
		STD_MAP_HAS_VALUE_DOC);
	stdfun_add(ctx, "keys", 1, std_map_keys,
		STD_MAP_KEYS_DOC);
	stdfun_add(ctx, "values", 1, std_map_values,
		STD_MAP_VALUES_DOC);
	stdfun_add(ctx, "kv_pairs", 1, std_map_kv_pairs,
		STD_MAP_KV_PAIRS_DOC);
	stdfun_add(ctx, "from_kv_pairs", 1,
		std_map_from_kv_pairs,
		STD_MAP_FROM_KV_PAIRS_DOC);
	stdfun_add(ctx, "iter", 2, std_map_iter,
		STD_MAP_ITER_DOC);
	stdfun_add(ctx, "iteri", 2, std_map_iteri,
		STD_MAP_ITERI_DOC);
	stdfun_add(ctx, "get_or_default", 3,
		std_map_get_or_default,
		STD_MAP_GET_OR_DEFAULT_DOC);
	std_set_module(ctx, NULL);
	
	std_set_module(ctx, "thread");
	stdfun_add(ctx, "spawn", 2, std_thread_spawn,
		STD_THREAD_SPAWN_DOC);
	stdfun_add(ctx, "join", 1, std_thread_join,
		STD_THREAD_JOIN_DOC);
	std_set_module(ctx, NULL);
	
	std_set_module(ctx, "io");
	stdfun_add(ctx, "print", 1, std_io_print,
		STD_IO_PRINT_DOC);
	stdfun_add(ctx, "input", 0, std_io_input,
		STD_IO_INPUT_DOC);
	stdfun_add(ctx, "read", 1, std_io_read,
		STD_IO_READ_DOC);
	stdfun_add(ctx, "write", 2, std_io_write,
		STD_IO_WRITE_DOC);
	stdfun_add(ctx, "append", 2, std_io_append,
		STD_IO_APPEND_DOC);
	stdfun_add(ctx, "size", 1, std_io_size,
		STD_IO_SIZE_DOC);
	stdfun_add(ctx, "dir_exists", 1, std_io_dir_exists,
		STD_IO_DIR_EXISTS_DOC);
	stdfun_add(ctx, "make_dir", 1, std_io_make_dir,
		STD_IO_MAKE_DIR_DOC);
	stdfun_add(ctx, "list_dir", 1, std_io_list_dir,
		STD_IO_LIST_DIR_DOC);
	stdfun_add(ctx, "del", 1, std_io_del,
		STD_IO_DEL_DOC);
	std_set_module(ctx, NULL);
	
	std_set_module(ctx, "socket");
	stdvar_add(ctx, "LOCALHOST",
		make_text(text_lit("127.0.0.1", ctx)),
		"the localhost address.");
	const char* socket_type[] = {"Stream", "Datagram"};
	stdvar_add(ctx, "type",
		make_map(lur_new_enum(ctx, socket_type, 2)),
		"the type of the socket.");
	stdfun_add(ctx, "create", 1, std_socket_create,
		STD_SOCKET_CREATE_DOC);
	stdfun_add(ctx, "connect", 3, std_socket_connect,
		STD_SOCKET_CONNECT_DOC);
	stdfun_add(ctx, "bind", 3, std_socket_bind,
		STD_SOCKET_BIND_DOC);
	stdfun_add(ctx, "listen", 2, std_socket_listen,
		STD_SOCKET_LISTEN_DOC);
	stdfun_add(ctx, "accept", 1, std_socket_accept,
		STD_SOCKET_ACCEPT_DOC);
	stdfun_add(ctx, "send", 2, std_socket_send,
		STD_SOCKET_SEND_DOC);
	stdfun_add(ctx, "recv", 1, std_socket_recv,
		STD_SOCKET_RECV_DOC);
	stdfun_add(ctx, "close", 1, std_socket_close,
		STD_SOCKET_CLOSE_DOC);
	std_set_module(ctx, NULL);
	
	std_set_module(ctx, "debug");
	std_set_module(ctx, NULL);
	
	gc_resume(ctx);
}

lur_config_t lur_default_config(void) {
	lur_config_t cfg;
	cfg.memory_limit = 0;
	return cfg;
}

lur_t* lur_new(lur_config_t cfg, int argc, char** argv ) {
	lur_t* ctx = mem_alloc(NULL, sizeof(lur_t));
	if (setjmp(ctx->errjmp)) return NULL;
	
	ctx->cfg = cfg;
	ctx->cur_vm = 0;
	
	ctx->mem.bytes = sizeof(lur_t);
	ctx->mem.total = ctx->mem.bytes;
	ctx->mem.next_gc = FIRST_GC_LIMIT;
	ctx->mem.objs = NULL;
	ctx->mem.gc_pause = 0;
	ctx->mem.marked = NULL;
	ctx->mem.nmarked = 0;
	ctx->mem.gc_cleaned = 0;
	ctx->mem.gc_cycles = 0;
	
	ctx->running = false;
	ctx->interpreter = false;
	
	ctx->help = map_new(ctx);
	
	ctx->std_map = NULL;
	ctx->std_map_name = NULL;
	
	ctx->thread_fref = NULL;
	ctx->thread_args = NULL;
	
	ctx->args = array_new(ctx);
	for (int i = 0; i < argc; i++)
		array_push(ctx->args,
			make_text(text_lit(argv[i], ctx)),
			ctx);
	
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
	
	lur_gc_free(ctx->mem.marked);
	
	#if LUR_DEBUG_PRINT_MEM_STATS
	lur_dprintf("memory leaked: %td bytes\n",
		ctx->mem.bytes - sizeof(lur_t));
	lur_dprintf("total allocated: %zu bytes\n",
		ctx->mem.total);
	lur_dprintf("gc objects cleaned: %zu\n",
		ctx->mem.gc_cleaned);
	lur_dprintf("gc cycles: %zu\n", ctx->mem.gc_cycles);
	#endif
	
	mem_free(ctx, ctx, sizeof(lur_t));
}

static void exec(
	lur_t* ctx, const text_t* src, const text_t* path)
{
	cl_init(&ctx->cl, src, ctx);
	cl_compile(&ctx->cl, src, path);
	cl_free(&ctx->cl);
	
	value_t fref = make_fref(fref_new(ctx->cl.func, ctx));
	*ctx->vm.sp++ = fref;
	ctx->running = true;
	vm_launch(&ctx->vm, fref, 0, false);
	ctx->running = false;
}

bool lur_xstring(lur_t* ctx, const char* src) {
	if (!ctx) return false;
	if (setjmp(ctx->errjmp)) return false;
	exec(ctx, text_lit(src, ctx), text_lit("input", ctx));
	return true;
}

bool lur_xfile(lur_t* ctx, const char* path) {
	if (!ctx) return false;
	if (setjmp(ctx->errjmp)) return false;
	exec(ctx,
		io_read(text_lit(path, ctx), ctx), text_lit(path, ctx));
	return true;
}

static void interpret(lur_t* ctx) {
	lur_printf("%s\n%s\n",
		LUR_VERSION, LUR_REPL_GREETING);
	ctx->interpreter = true;
	for (;;) {
		lur_printf(":: ");
		lur_xstring(ctx,
			(const char*)io_read_stdin(ctx)->buffer);
	}
}

int main(int argc, char* argv[]) {
	lur_config_t cfg = lur_default_config();
	lur_t* ctx = lur_new(cfg, argc, argv);
	
	bool success = true;
	if (argc == 2) success = lur_xfile(ctx, argv[1]);
	else interpret(ctx);
	
	if (!success) {
		io_write(
			text_lit("log.txt", ctx),
			text_lit(lur_get_error(ctx), ctx),
			ctx);
		exit(EXIT_FAILURE);
	}
	
	lur_free(ctx);
	return EXIT_SUCCESS;
}
