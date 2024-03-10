#include "9cc.h"

LVarList *locals;
LVarList *globals;
LVarList *scope;

Program *code;

static Function *function();
void global_var();
static Node *stmt();
static Node *expr();
static Node *assign();
static Node *equality();
static Node *relational();
static Node *add();
static Node *mul();
static Node *unary();
static Node *postfix();
static Node *primary();
Member *read_member();
Member *read_member();
Type *basetype();
Type *read_type_suffix(Type *base);
Type *struct_decl();

// 変数を名前で検索
LVar *find_lvar(Token *tok) {
  for (LVarList *vl = scope; vl; vl = vl->next) {
    LVar *var = vl->var;
    if (strlen(var->name) == tok->len && !memcmp(tok->str, var->name, tok->len)) {
      return var;
    }
  }
  return NULL;
}

LVar *push_var(char *var_name, Type *ty, bool is_local) {
  LVar *lvar = calloc(1, sizeof(LVar));
  lvar->name = var_name;
  lvar->offset = locals? locals->var->offset + 8: 8;
  lvar->ty = ty;
  lvar->is_local = is_local;
  LVarList *vl = calloc(1, sizeof(LVarList));
  vl->var = lvar;
  if (is_local) {
    vl->next = locals;
    locals = vl;
  } else {
    vl->next = globals;
    globals = vl;
  }

  LVarList *sc = calloc(1, sizeof(LVarList));
  sc->var = lvar;
  sc->next = scope;
  scope = sc;

  return lvar;
}

char *create_varname() {
  static int cnt = 0;
  char buf[20];

  sprintf(buf, ".LC%d", cnt++);
  return strndup(buf, 20);
}

Member *read_member() {
  Member *member = calloc(1, sizeof(Member));
  member->ty = basetype();
  member->name = expect_ident();
  member->ty = read_type_suffix(member->ty);
  expect(";");
  return member;
}

// basetype = ("char" | "int") "*"*
Type *basetype() {
  Type *ty;
  if (consume("int")) {
    ty = int_type();
  } else if (consume("char")) {
    ty = char_type();
  } else if (consume("struct")) {
    ty = struct_decl();
  } else {
    error_at(token->str, "type error");
  }
  while (consume("*"))
    ty = pointer_to(ty);
  return ty;
}

Type *struct_decl() {
  expect("{");

  Member head;
  head.next = NULL;

  Member *cur = &head;

  while (!consume("}")) {
    cur->next = read_member();
    cur = cur->next;
  }

  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_STRUCT;
  ty->members = head.next;

  int offset = 0;
  for (Member *mem = ty->members; mem; mem = mem->next) {
    mem->offset = offset;
    offset += size_of(mem->ty);
  }

  return ty;
}

bool is_function() {
  Token *tok = token;
  basetype();
  bool is_func = consume_ident() && consume("(");
  token = tok;
  return is_func;
}

bool is_type() {
  return peek("int") || peek("char") || peek("struct");
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

// program = function*
void program() {
  Function head;
  head.next == NULL;
  Function *cur = &head;
  globals = NULL;

  while (!at_eof()) {
    if (is_function()) {
      cur->next = function();
      cur = cur->next;
    } else {
      global_var();
    }
  }
  
  Program *prog = calloc(1, sizeof(Program)); 
  prog->fns = head.next;
  prog->globals = globals;
  code = prog;
}

Type *read_type_suffix(Type *base) {
  if (!consume("["))
    return base;
  size_t n = expect_number();
  expect("]");
  base = read_type_suffix(base);
  return array_of(base, n);
}

LVarList *read_func_params() {
  if (consume(")"))
    return NULL;
  
  Type *ty = basetype();
  char *name = expect_ident();
  ty = read_type_suffix(ty);
  LVarList *head = calloc(1, sizeof(LVarList));
  head->var = push_var(name, ty, true);
  LVarList *cur = head;

  while (!consume(")")) {
    expect(",");
    ty = basetype();
    char *name = expect_ident();
    ty = read_type_suffix(ty);
    cur->next = calloc(1, sizeof(LVarList));
    cur->next->var = push_var(name, ty, true);
    cur = cur->next;
  }

  return head;
}

// global-var = basetype ident ("[" num "]")* ";"
void global_var() {
  Type *ty = basetype();
  char *name = expect_ident();
  ty = read_type_suffix(ty);
  expect(";");
  push_var(name, ty, false);
}

// declaration = basetype ident ("[" num "]")* ("=" expr) ";"
Node *declaration() {
  Token *tok = token;
  Type *ty = basetype();
  char *name = expect_ident();
  ty = read_type_suffix(ty);
  LVar *var = push_var(name, ty, true);

  Node *node = calloc(1, sizeof(Node));
  if (consume(";")) {
    node->kind = ND_NULL;
    return  node;
  }
  expect("=");
  node->kind = ND_ASSIGN;
  node->lhs = calloc(1, sizeof(Node));
  node->lhs->kind = ND_LVAR;
  node->lhs->var = var;
  node->rhs = expr();
  expect(";");
  node = new_node(ND_EXPR_STMT, node, NULL);
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
        node->init = new_node(ND_EXPR_STMT, expr(), NULL);
        expect(";");
      }
      if (!consume(";")) {
        node->cond = expr();
        expect(";");
      }
      if (!consume(")")) {
        node->inc = new_node(ND_EXPR_STMT, expr(), NULL);
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
    LVarList *sc = scope; // ブロックに入るまでの情報を保持してブロック間の変数を一気に削除できる
    while (!consume("}")) {
      cur->next = stmt();
      cur = cur->next;
    }
    scope = sc;
    node = calloc(1, sizeof(Node));
    node->kind = ND_BLOCK;
    node->body = head.next;
    return node;
  }
  if (is_type())
    return declaration();
  node = new_node(ND_EXPR_STMT, expr(), NULL);
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
    if (consume("+")) {
      node = new_node(ND_ADD, node, mul());
    }
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
//        | "sizeof" unary
//        | postfix
static Node *unary() {
  if (consume("+"))
    return unary();
  if (consume("-"))
    return new_node(ND_SUB, new_node_num(0), unary());
  if (consume("*"))
    return new_node(ND_DEREF, unary(), NULL);
  if (consume("&"))
    return new_node(ND_ADDR, unary(), NULL);
  if (consume("sizeof"))
    return new_node(ND_SIZEOF, unary(), NULL);
  return postfix();
}

// postfix = primary ("[" expr "]" | "." ident)*
Node *postfix() {
  Node *node = primary();

  for (;;) {
    if (consume("[")) {
      // x[y] == *(x+y)
      node = new_node(ND_ADD, node, expr());
      expect("]");
      node = new_node(ND_DEREF, node, NULL);
      continue;
    } else if (consume(".")) {
      node = new_node(ND_MEMBER, node, NULL);
      node->member_name = expect_ident();
      continue;
    }
    return node;
  }
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

// stmt = "(" "{" stmt stmt* "}" ")"
Node *stmt_expr() {
  Node * node = calloc(1, sizeof(Node));
  // stmtがからの時にstmt()呼び出しないでexpect_numberに引っかかってエラーで止まる
  node->body = stmt();
  node->kind = ND_STMT_EXPR;
  Node *cur = node->body;
  LVarList *sc = scope;
  while (!consume("}")) {
    cur->next = stmt();
    cur = cur->next;
  }
  expect(")");
  *cur = *cur->lhs;
  scope = sc;
  return node;
}

// primary = num | ident func-args? | "(" expr ")" | str | "(" "{" stmt-expr-tail "}" ")"
static Node *primary() {
  if (consume("(")) {
    if (consume("{"))
      return stmt_expr();
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
      node->var = lvar;
    } else {
        error_at(tok->str, "宣言されていません");
    }
    return node;
  }

  if (token->kind == TK_STR) {
    // + 1 \0の分
    Type *ty = array_of(char_type(), token->len + 1);
    LVar *var = push_var(create_varname(), ty, false);
    var->contents = strndup(token->str, token->len);
    var->cont_len = token->len;
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_LVAR;
    node->var = var;
    token = token->next;
    return node;
  }

  return new_node_num(expect_number());
}