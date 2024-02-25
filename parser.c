#include "9cc.h"

LVar *locals;

Function *code;

// 変数を名前で検索
LVar *find_lvar(Token *tok) {
  for (LVar *var = locals; var; var = var->next) {
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len)) {
      return var;
    }
  }
  return NULL;
}

LVar *push_var(char *var_name, Type *ty) {
  LVar *lvar = calloc(1, sizeof(LVar));
  lvar->name = var_name;
  lvar->len = strlen(var_name);
  lvar->next = locals;
  // chibiccではmainで最後の変数から順にoffsetを割り当てていく
  lvar->offset = locals? locals->offset + 8: 8;
  lvar->ty = ty;
  // 関数宣言の引数はnodeに値をセットしない
  locals = lvar;
  return lvar;
}

// basetype = "int"
Type *basetype() {
  expect("int");
  Type *ty = int_type();
  while (consume("*"))
    ty = pointer_to(ty);
  return ty;
}

static Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node *new_node_num(int val) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_NUM;
  node->val = val;
  return node;
}

static Function *function();
static Node *stmt();
static Node *expr();
static Node *assign();
static Node *equality();
static Node *relational();
static Node *add();
static Node *mul();
static Node *unary();
static Node *primary();

// program = function*
void program() {
  Function head;
  head.next == NULL;
  Function *cur = &head;

  while (!at_eof()) {
    cur->next = function();
    cur = cur->next;
  }
  code = head.next;
}

LVar *read_func_params() {
  if (consume(")"))
    return NULL;
  
  Type *ty = basetype();
  push_var(expect_ident(), ty);

  while (!consume(")")) {
    expect(",");
    ty = basetype();
    push_var(expect_ident(), ty);
  }
  LVar *head = locals;

  return head;
}

// declaration = basetype ident ("=" expr) ";"
Node *declaration() {
  Token *tok = token;
  Type *ty = basetype();
  LVar *var = push_var(expect_ident(), ty);

  Node *node = calloc(1, sizeof(Node));
  if (consume(";")) {
    node->kind = ND_NULL;
    node->offset = var->offset;
    return  node;
  }
  expect("=");
  node->kind = ND_ASSIGN;
  node->lhs = calloc(1, sizeof(Node));
  node->lhs->kind = ND_LVAR;
  node->lhs->offset = var->offset;
  node->rhs = expr();
  expect(";");
  return node;
}

// functoin = basetype ident "(" params? ")" "{" stmt* "}"
// params = basetype ident ("," basetype ident)*
Function *function() {
  locals = NULL;
  Function *fn = calloc(1, sizeof(Function));
  basetype();
  fn->name = expect_ident();
  expect("(");
  fn->params = read_func_params();
  expect("{");

  Node head;
  head.next = NULL;
  Node *cur = &head;

  while (!consume("}")) {
    cur->next = stmt();
    cur = cur->next;
  }
  fn->node = head.next;
  fn->locals = locals;
  return fn;
}

// stmt = expr ";"
//      | "{" stmt* "}"
//      | return expr ";"
//      | "if" "(" expr ")" stmt ("else" stmt)?
//      | "while" "(" expr ")" stmt
//      | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//      | declaration
static Node *stmt() {
  Node *node;

  if (consume("return")) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_RETURN;
    node->lhs = expr();
    expect(";");
    return node;
  } else if (consume("if")) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_IF;
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();
    if (consume("else"))
      node->els = stmt();
      return node;
  } else if (consume("for")) {
      node = calloc(1, sizeof(Node));
      node->kind = ND_FOR;
      expect("(");
      if (!consume(";")) {
        node->init = expr();
        expect(";");
      }
      if (!consume(";")) {
        node->cond = expr();
        expect(";");
      }
      if (!consume(")")) {
        node->inc = expr();
        expect(")");
      }
      node->then = stmt();
      return node;
  } else if (consume("while")) {
      node = calloc(1, sizeof(Node));
      node->kind = ND_WHILE;
      expect("(");
      node->cond = expr();
      expect(")");
      node->then = stmt();
      return node;
  }
  if (consume("{")) {
    Node head;
    head.next = NULL;
    Node *cur = &head;
    while (!consume("}")) {
      cur->next = stmt();
      cur = cur->next;
    }
    node = calloc(1, sizeof(Node));
    node->kind = ND_BLOCK;
    node->body = head.next;
    return node;
  }
  if (peek("int"))
    return declaration();
  node = expr();
  expect(";");
  return node;
}

// expr = assign
static Node *expr() {
  return assign();
}

// assign = equality ("=" assign)?
static Node *assign() {
  Node *node = equality();
  if (consume("="))
    node = new_node(ND_ASSIGN, node, assign());
  return node;
}

// equality = relational ("==" relational | "!=" relational)*
static Node *equality() {
  Node *node = relational();
  for (;;) {
    if (consume("=="))
      node = new_node(ND_EQ, node, relational());
    else if (consume("!="))
      node = new_node(ND_NE, node, relational());
    else
      return node;
  }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational() {
  Node *node = add();
  for (;;) {
    if (consume("<"))
      node = new_node(ND_LT, node, add());
    else if (consume("<="))
      node = new_node(ND_LE, node, add());
    else if (consume(">"))
      node = new_node(ND_LT, add(), node);
    else if (consume(">="))
      node = new_node(ND_LE, add(), node);
    else
      return node;
  }
}

// add = mul ("+" mul | "-" mul)*
static Node *add() {
  Node *node = mul();
  for (;;) {
    if (consume("+"))
      node = new_node(ND_ADD, node, mul());
    else if (consume("-"))
      node = new_node(ND_SUB, node, mul());
    else
      return node;
  }
}

// mul = unary ("*" unary | "/" unary)*
static Node *mul() {
  Node *node = unary();
  for (;;) {
    if (consume("*"))
      node = new_node(ND_MUL, node, unary());
    else if (consume("/"))
      node = new_node(ND_DIV, node, unary());
    else
      return node;
  }
}

// unary = ("+" | "-" | "*" | "&")? unary
//        | primary
static Node *unary() {
  if (consume("+"))
    return unary();
  if (consume("-"))
    return new_node(ND_SUB, new_node_num(0), unary());
  if (consume("*"))
    return new_node(ND_DEREF, unary(), NULL);
  if (consume("&"))
    return new_node(ND_ADDR, unary(), NULL);
  return primary();
}

// func-args = "(" (assign ("," assign)*)? ")"
Node *func_args() {
  if (consume(")"))
    return NULL;
  Node *head = assign();
  Node *cur = head;
  while (consume(",")) {
    cur->next = assign();
    cur = cur->next;
  }
  expect(")");
  return head;
}

// primary = num | int ident func-args? | "(" expr ")"
static Node *primary() {
  if (consume("(")) {
    Node *node = expr();
    expect(")");
    return node;
  }

  Token *tok = consume_ident();
  if (tok) {
    Node *node = calloc(1, sizeof(Node));
    // 関数処理
    if (consume("(")) {
      //expect(")");
      node->kind = ND_FUNC;
      node->funcname = strndup(tok->str, tok->len);
      node->args = func_args();
      return node;
    }

    // 変数処理
    node->kind = ND_LVAR;
    LVar *lvar = find_lvar(tok);
    if (lvar) {
      node->offset = lvar->offset;
    } else {
        error_at(tok->str, "宣言されていません");
    }
    return node;
  }

  return new_node_num(expect_number());
}