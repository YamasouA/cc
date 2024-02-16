#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  TK_RESERVED, // 記号
  TK_IDENT, // 識別子
  TK_NUM, // 整数トークン
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
} NodeKind;

typedef struct Node Node;

struct Node {
  NodeKind kind; // ノードの型
  Node *lhs; // 左辺
  Node *rhs; // 右辺
  int val; // kindがND_NUMの時に使う
  int offset; // kindがND_LVARの場合のみ使う
};

extern Node *code[100];

// 入力プログラム
extern char *user_input;


// Tokenizer
bool consume(char *op);
void expect(char *op);
int expect_number();
Token *consume_ident();
Token *tokenize();
bool at_eof();

// Parser
void program();

// Generator
void gen(Node *node);

// main
void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);