#define __STDC_WANT_LIB_EXT2__ 1 // strndupがうまく呼べなかったので追加(https://stackoverflow.com/questions/46013382/c-strndup-implicit-declaration)
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

typedef struct Type Type;
typedef struct Member Member;
typedef struct Initializer Initializer;

typedef enum {
  TY_SHORT,
  TY_INT,
  TY_LONG,
  TY_PTR,
  TY_ARRAY,
  TY_CHAR,
  TY_STRUCT,
  TY_FUNC,
  TY_VOID,
  TY_BOOL,
  TY_ENUM,
} TypeKind;

struct Type {
  TypeKind kind;
  Type *base;
  size_t array_size; // 配列の時のみ使う
  Member *members;
  int align;
  Type *return_ty; // 関数の戻り値の型
  bool is_static;
  bool is_incomplete;
};

typedef struct TagScope TagScope;
struct TagScope {
  TagScope *next;
  char *name;
  Type *ty;
  int depth;
};


struct Member {
  Member *next;
  Type *ty;
  char *name;
  int offset;
};

typedef struct LVar LVar;

// 変数の型
struct LVar {
  char *name; // 編数名
  int offset; // RBPからのオフセット (localの時に使う)
  Type *ty; // 型
  Type *type_def; // typedefの型
  Type *enum_ty; // enum
  int enum_val; // enumの値
  bool is_local; // local or global
  char *static_name; // local or global
  int depth;

  Initializer *initializer;
  int val;
};

typedef struct LVarList LVarList;
struct LVarList {
  LVar *var;
  LVarList *next;
  int depth;
};

struct Initializer {
  Initializer *next;

  // constant expression
  int sz;
  char *contents;
  long val;

  // 他のグローバル変数を参照する
  char *label;
  long addend;
};

// ローカル変数
extern LVarList *locals;

typedef enum {
  TK_RESERVED, // 記号
  TK_IDENT, // 識別子
  TK_NUM, // 整数トークン
  TK_STR, // 文字列トークン
  TK_EOF, // 入力終わりのトークン
} TokenKind;

typedef struct Token Token;

struct Token {
  TokenKind kind; // トークンの型
  Token *next; // 次の入力トークン
  int val; // kindがTK_NUMの場合、その数値
  char *str; // トークン文字列
  int len;  // トークンの長さ
};

// 現在着目しているトークン
extern Token *token;

// 抽象構文木のノードの種類
typedef enum {
  ND_ADD, // +
  ND_SUB, // -
  ND_MUL, // *
  ND_DIV, // /
  ND_EQ, // ==
  ND_NE, // !=
  ND_LT, // <
  ND_LE, // <=
  ND_NUM, // 整数
  ND_ASSIGN, // =
  ND_LVAR, // ローカル変数
  ND_STMT_EXPR, // Statement expression
  ND_EXPR_STMT, // 代入の右辺
  ND_RETURN, // return
  ND_IF, // if
  ND_FOR, // for
  ND_WHILE, // while
  ND_BLOCK, // { ... }
  ND_FUNC, // 関数
  ND_ADDR, // unary &
  ND_DEREF, // unuary *
  ND_NULL, // 空の値
  ND_SIZEOF, // "sizeof"
  ND_MEMBER, //struct メンバー変数
  ND_CAST, // 型変換
  ND_PRE_INC, // pre ++
  ND_PRE_DEC, // pre --
  ND_POST_INC, // post ++
  ND_POST_DEC, // post --
  ND_A_ADD, // +=
  ND_A_SUB, // -=
  ND_A_MUL, // *=
  ND_A_DIV, // /=
  ND_NOT, // !
  ND_LOGAND, // &&
  ND_LOGOR, // ||
  ND_BREAK, // break
  ND_CONTINUE, // continue
  ND_SWITCH, // switch 
  ND_CASE, // case
  ND_TERNARY, // ?:
} NodeKind;

typedef struct Node Node;


struct Node {
  NodeKind kind; // ノードの型
  Type *ty; // 型
  Node *next;
  Node *lhs; // 左辺
  Node *rhs; // 右辺

  // "if" or "while" or "for" statement
  Node *cond;
  Node *then;
  Node *els;
  Node *init;
  Node *inc;

  // Block or statement expression
  Node *body;

  Node *case_next;
  Node *default_case;
  int case_label;
  int case_end_label;

  // 関数名
  char *funcname;
  // argsは式として処理するのでNode型
  Node *args;

  LVar *var;

  int val; // kindがND_NUMの時に使う

  char *member_name; // a.b <-のbを保存するやつ
  Member *member;
};

typedef struct Designator Designator;
struct Designator {
  Designator *next;
  int idx; // arrray
  Member *mem; // struct
};

typedef struct Function Function;
struct Function {
  Function *next;
  char *name;
  LVarList *params;
  bool is_static;
  Node *node;
  LVarList *locals;
  int stack_size;
};

typedef struct {
  LVarList *globals;
  Function *fns;
} Program;

extern Program *code;
extern LVarList *globals;

// 入力プログラム
extern char *filename;
extern char *user_input;


// Tokenizer
bool consume(char *op);
void expect(char *op);
int expect_number();
char *expect_ident();
char *expect_type();
Token *consume_ident();
Token *tokenize();
bool at_eof();
bool peek(char *op);

// Parser
void program();

// Generator
void codegen(); 

// main
void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);

// Type
Type *void_type();
Type *bool_type();
Type *short_type();
Type *int_type();
Type *long_type();
Type *enum_type();
Type *func_type();
Type *char_type();
Type *struct_type();
Type *pointer_to();
Type *array_of(Type *base, int size);
int align_to(int n, int align);
void add_type(); 
int size_of(Type *ty);
