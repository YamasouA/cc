#include "9cc.h"

// 現在着目しているトークン
Token *token;
char *filename;

bool at_eof() {
  return token->kind == TK_EOF;
}

static bool startswith(char *p, char *q) {
  return memcmp(p, q, strlen(q)) == 0;
}

static bool is_alnum(char c) {
  return ('a' <= c && c <= 'z') ||
        ('A' <= c && c <= 'Z') ||
        ('0' <= c && c <= '9') ||
        (c == '_');
}

static Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  cur->next = tok;
  return tok;
}

// 次のトークンが期待している記号の時はトークンを1つ読み進めてtrueを返す
bool consume(char *op) {
  if (token->kind != TK_RESERVED || 
      strlen(op) != token->len ||
      memcmp(token->str, op, token->len))
    return false;
  token = token->next;
  return true;
}

bool peek(char *op) {
  if  (strlen(op) != token->len ||
    memcmp(token->str, op, token->len))
    return false;
  return true;
}

// 次のトークンが期待している記号の時はトークンを1つ読み進めてtrueを返す
Token *consume_ident() {
  if (token->kind != TK_IDENT)
    return NULL;
  Token *tmp = token;
  token = token->next;
  return tmp;
}

// 次のトークンが期待している記号の時はトークンを1つ読み進める
// それ以外ならエラーを報告する
void expect(char *op) {
  if (token-> kind != TK_RESERVED || 
      strlen(op) != token->len ||
      memcmp(token->str, op, token->len))
    error_at(token->str, "'%s'ではありません", op);
  token = token->next;
}

// 次のトークンが数値の場合トークンを1つ読み進めてその数値を返す
// それ以外の場合にはエラーを報告する
int expect_number() {
  if (token->kind != TK_NUM)
    error_at(token->str, "数ではありません");
  int val = token->val;
  token = token->next;
  return val;
}

// 次のトークンが変数の場合トークンを1つ読み進めてその変数名を返す
// それ以外の場合にはエラーを報告する
char *expect_ident() {
  if (token->kind != TK_IDENT)
    error_at(token->str, "識別子ではありません");
  char *s = strndup(token->str, token->len);
  token= token->next;
  return s;
}

char *expect_type() {
  if (token->kind != TK_RESERVED)
    return NULL;
  char *s = strndup(token->str, token->len);
  token = token->next;
  return s;
}

unsigned int escape_char(char c) {
  switch (c) {
    case 'a': return '\a';
    case 'b': return '\b';
    case 't': return '\t';
    case 'n': return '\n';
    case 'v': return '\v';
    case 'f': return '\f';
    case 'r': return '\r';
    case 'e': return 27;
    case '0': return 0;
    default: return c;
  }
}

Token *read_string_literal(Token *cur, char **start) {
  char *p = *start + 1;
  char buf[1024];
  int len = 0;

  while (1) {
    if (len == sizeof(buf))
      error_at(*start, "文字列が長すぎます");
    if (*p == '\0')
      error_at(*start, "文字列リテラルが閉じられていません");
    if (*p == '"') {
      p++;
      break;
    }

    if (*p == '\\') {
      p++;
      if (*p == '"' || *p == 'n') {
        buf[len++] = '\\';
        buf[len++] = *p;
        p++;
      } else
        buf[len++] = escape_char(*p++);
    } else {
      buf[len++] = *p++;
    }
  }
  buf[len] = '\0';

  Token *tok = new_token(TK_STR, cur, strdup(buf), strlen(buf));
  *start = p;
  return tok;
}

Token *read_char_literal(Token *cur, char **start) {
  char *p = *start + 1;
  if (*p == '\0')
    error_at(*start, "文字リテラルが閉じられていません");
  
  char c;
  if (*p == '\\') {
    p++;
    c = escape_char(*p++);
  } else {
    c = *p++;
  }

  if (*p != '\'')
    error_at(*start, "文字リテラルが閉じられていません");
  p++;

  Token *tok = new_token(TK_NUM, cur, *start, p - *start);
  tok->val = c;
  *start = p;
  return tok;
}

static char *starts_with_reserved(char *p) {
  static char *kw[] = {"return", "if", "else", "while", "for",
                      "sizeof", "struct", "typedef", "enum",
                      "static", "break", "continue", "switch", "case", "default"};

  for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++) {
    int len = strlen(kw[i]);
    if (startswith(p, kw[i]) && !is_alnum(p[len]))
      return kw[i];
  }

  static char *ops[] = {"==", "!=", "<=", ">=", "->", "++", "--",
                        "+=", "-=", "*=", "/=", "&&", "||"};

  for (int i = 0; i < sizeof(ops) / sizeof(*ops); i++) {
    if (startswith(p, ops[i]))
      return ops[i];
  }

  static char *types[] = {"int", "char", "short", "long", "void", "_Bool"};
  for (int i = 0; i < sizeof(types) / sizeof(*types); i++) {
    int len = strlen(types[i]);
    if (startswith(p, types[i]) && !is_alnum(p[len]))
      return types[i];
  }
  return NULL;
}

Token *tokenize() {
  char *p = user_input;
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while (*p) {
    // 空白文字をスキップ
    if (isspace(*p)) {
      p++;
      continue;
    }

    // Skip line comments.
    if (startswith(p, "//")) {
      p += 2;
      while (*p != '\n')
        p++;
      continue;
    }

    // Skip block comments.
    if (startswith(p, "/*")) {
      char *q = strstr(p + 2, "*/");
      if (!q)
        error_at(p, "unclosed block comment");
      p = q + 2;
      continue;
    }

    char *kw = starts_with_reserved(p);
    if (kw) {
      int len = strlen(kw);
      cur = new_token(TK_RESERVED, cur, p, len);
      p += len;
      continue;
    }

    if (*p == '"') {
      cur = read_string_literal(cur, &p);
      continue;
    }

    if (*p == '\'') {
      cur = read_char_literal(cur, &p);
      continue;
    }


    if (strchr("+-*/()<>=;{},&[].!:", *p)) {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }

    if ('a' <= *p && *p <= 'z' || *p == '_') {
      cur = new_token(TK_IDENT, cur, p, 0);
      char *q = p;
      while (is_alnum(*p))
        p++;
      cur->len = p - q;
      continue;
    }

    if (isdigit(*p)) {
      cur = new_token(TK_NUM, cur, p, 0);
      char *q = p;
      cur->val = strtol(p, &p, 10);
      cur->len = p - q;
      continue;
    }

    error_at(p, "トークナイズできません");
  }

  new_token(TK_EOF, cur, p, 0);
  token = head.next;
  return head.next;
}