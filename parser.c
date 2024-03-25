#include "9cc.h"

LVarList *locals;
LVarList *globals;
LVarList *scope;
TagScope *tag_scope;
Node *current_switch;

Program *code;

static Function *function();
void global_var();
static Node *stmt();
static Node *expr();
static Node *assign();
Node *logor();
Node *logand();
static Node *equality();
static Node *relational();
static Node *add();
static Node *mul();
static Node *cast();
static Node *unary();
static Node *postfix();
static Node *primary();
Member *read_member();
Member *read_member();
Type *basetype();
Type *read_type_suffix(Type *base);
Type *struct_decl();
Type *enum_decl();
bool is_type();
Type *declarator(Type *ty, char **name);

TagScope *find_tag(Token *tok) {
  for (TagScope *sc = tag_scope; sc; sc = sc->next)
    if (strlen(sc->name) == tok->len && !memcmp(tok->str, sc->name, tok->len))
      return sc;
  return NULL;
}

void push_tag_scope(Token *tok, Type *ty) {
  TagScope *sc = calloc(1, sizeof(TagScope));
  sc->next = tag_scope;
  sc->name = strndup(tok->str, tok->len);
  sc->ty = ty;
  tag_scope = sc;
}

// 変数を名前で検索
LVar *find_lvar(Token *tok) {
  for (LVarList *vl = scope; vl; vl = vl->next) {
    LVar *var = vl->var;
    if (strlen(var->name) == tok->len && !memcmp(tok->str, var->name, tok->len)) {
      return var;
    }
    // varにnameとstatic_nameで分けるのはあまり良くないので治したい
    if (var->static_name != NULL) {
      if (strlen(var->static_name) == tok->len && !memcmp(tok->str, var->static_name, tok->len)) {
        return var;
      }
    }
  }
  return NULL;
}

LVarList *push_scope(LVar *var) {
  LVarList *sc = calloc(1, sizeof(LVarList));
  sc->var = var;
  sc->next = scope;
  scope = sc;
  return sc;
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
  } else if (ty->kind != TY_FUNC) {
    vl->next = globals;
    globals = vl;
  }

  return lvar;
}

Type *find_typedef(Token *tok) {
  if (tok->kind == TK_IDENT) {
    LVar *sc = find_lvar(tok);
    if (sc) {
      return sc->type_def;
    }
  }
  return NULL;
}

char *create_varname() {
  static int cnt = 0;
  char buf[20];

  sprintf(buf, ".LC%d", cnt++);
  return strndup(buf, 20);
}

Member *read_member() {
  char *name = NULL;
  Member *member = calloc(1, sizeof(Member));
  Type *ty = basetype();
  ty = declarator(ty, &name);
  ty = read_type_suffix(ty);
  expect(";");
  member->ty = ty;
  member->name = name;
  member->ty = ty;
  return member;
}

// basetype = "static"* ("void" | "char" | "int" | "short" | "long" | struct-decl | typedef-name) "*"*
Type *basetype() {
  Type *ty;
  bool is_static = false;
  // static は本来先頭以外でも良いのでここは修正が必要
  if (consume("static"))
    is_static = true;
  if (is_type()) {
      if (consume("int")) {
      ty = int_type();
    } else if (consume("short")) {
      ty = short_type();
    } else if (consume("long")) {
      ty = long_type();
    } else if (consume("char")) {
      ty = char_type();
    } else if (consume("struct")) {
      ty = struct_decl();
    } else if (consume("void")) {
      ty = void_type();
    } else if (consume("_Bool")) {
      ty = bool_type();
    } else if (peek("enum")) {
      ty = enum_decl();
    } else {
      ty = find_typedef(consume_ident()); // typedef
    }
  } else {
    ty = int_type();
  }
  ty->is_static = is_static;
  return ty;
}

// declarator = "*"* ("(" declarator ")" | ident) type-suffix
Type *declarator(Type *ty, char **name) {
  while (consume("*"))
    ty = pointer_to(ty);

  if (consume("(")) {
    Type *placeholder = calloc(1, sizeof(Type));
    Type *new_ty = declarator(placeholder, name);
    expect(")");
    *placeholder = *read_type_suffix(ty);
    return new_ty;
  }

  *name = expect_ident();
  return read_type_suffix(ty);
}

Type *abstract_declarator(Type *ty) {
  while (consume("*"))
    ty = pointer_to(ty);

  if (consume("(")) {
    Type *placeholder = calloc(1, sizeof(Type));
    Type *new_ty = abstract_declarator(placeholder);
    expect(")");
    *placeholder = *read_type_suffix(ty);
    return new_ty;
  }

  return read_type_suffix(ty); 
}

// enum_decl = "enum" ident
//           | "enum" ident? "{" enum-list? "}"
// enum-list = ident ("=" num)? ("," ident ("=" num)?)* ","?
Type *enum_decl() {
  expect("enum");
  Type *ty = enum_type();

  // enum tag
  Token *tag = consume_ident();
  if (tag && !peek("{")) {
    TagScope *sc = find_tag(tag); 
    if (!sc)
      error("enum error");
    if (sc->ty->kind != TY_ENUM)
      error("enum error2");
    return sc->ty; 
  }

  expect("{");

  // enum-list
  int cnt = 0;
  for (;;) {
    char *name = expect_ident();
    if (consume("="))
      cnt = expect_number();
    
    LVar *sc = push_var(name, ty, true); // typeにセットしてるが本当はセットしたくない
    sc->enum_ty = ty;
    sc->enum_val = cnt++;
    push_scope(sc);

    if (consume(",")) {
      if (consume("}"))
        break;
      continue;
    }
    expect("}");
    break;
  }

  if (tag)
    push_tag_scope(tag, ty);
  return ty;
}

// struct-decl = "struct" ident
//             | "struct" ident? "{" struct-member "}"
Type *struct_decl() {
  Token *tag = consume_ident();
  // 宣言したstructを利用するケース
  if (tag && !peek("{")) {
    TagScope *sc = find_tag(tag);
    if (!sc)
      error("struct decl error");
    if (sc->ty->kind != TY_STRUCT)
      error("struct decl error2");
    return sc->ty;
  }

  // Memberを読む
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
    offset = align_to(offset, mem->ty->align);
    mem->offset = offset;
    offset += size_of(mem->ty);

    if (ty->align < mem->ty->align)
      ty->align = mem->ty->align;
  }

  if (tag)
    push_tag_scope(tag, ty);

  return ty;
}

bool is_function() {
  Token *tok = token;
  Type *ty = basetype();
  char *name = NULL;
  declarator(ty, &name);
  bool is_func = name && consume("(");
  token = tok;
  return is_func;
}

bool is_type() {
  return peek("int") || peek("char") || peek("short") || peek("long") || peek("enum") ||
        peek("struct") || find_typedef(token) || peek("void") || peek("_Bool") || peek("static");
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
      Function *fn = function();
      if (!fn)
        continue;
      cur->next = fn;
      cur = cur->next;
      continue;
    }
    global_var();
  }
  
  Program *prog = calloc(1, sizeof(Program)); 
  prog->fns = head.next;
  prog->globals = globals;
  code = prog;
}

// type-suffix = ("[" num "]" type-suffix)?
Type *read_type_suffix(Type *base) {
  if (!consume("["))
    return base;
  int n;
  if (!consume("]")) {
    n = expect_number();
    expect("]");
  }
  base = read_type_suffix(base);
  return array_of(base, n);
}

LVarList *read_func_params() {
  if (consume(")"))
    return NULL;
  
  Type *ty = basetype();
  char *name = NULL;
  ty = declarator(ty, &name);
  ty = read_type_suffix(ty);

  // array型の引数はポインタとして扱う
  if (ty->kind == TY_ARRAY)
    ty = pointer_to(ty->base);

  LVarList *head = calloc(1, sizeof(LVarList));
  head->var = push_var(name, ty, true);
  push_scope(head->var);
  LVarList *cur = head;

  while (!consume(")")) {
    expect(",");
    ty = basetype();
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = read_type_suffix(ty);
    cur->next = calloc(1, sizeof(LVarList));
    cur->next->var = push_var(name, ty, true);
    push_scope(cur->next->var);
    cur = cur->next;
  }

  return head;
}

// global-var = basetype ident ("[" num "]")* ";"
void global_var() {
  Type *ty = basetype();
  char *name = NULL;
  ty = declarator(ty, &name);
  ty = read_type_suffix(ty);
  expect(";");
  LVar *var = push_var(name, ty, false);
  push_scope(var);
}

// declaration = basetype ident ("[" num "]")* ("=" expr) ";"
//              | basetype; // struct t {...}; のようなケース
Node *declaration() {
  Node *node = calloc(1, sizeof(Node));
  Token *tok = token;
  Type *ty = basetype();
  if (consume(";")) {
    node->kind = ND_NULL;
    return node;
  }
  char *name = NULL;
  ty = declarator(ty, &name);
  ty = read_type_suffix(ty);
  if (ty->kind == TY_VOID)
    error("変数にvoid型は使えません");
  bool is_local = true;
  if (ty->is_static) {
    is_local = false;
  }
  LVar *var = push_var(create_varname(), ty, is_local);
  var->static_name = name;
  push_scope(var);

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

// functoin = basetype ident "(" params? ")" ("{" stmt* "}" | ";")
// params = basetype ident ("," basetype ident)*
Function *function() {
  locals = NULL;
  Function *fn = calloc(1, sizeof(Function));
  Type *ty = basetype();
  char *name = NULL;
  ty = declarator(ty, &name);
  fn->name = name;
  // 関数名と型を登録
  LVar *var = push_var(fn->name, func_type(ty), false);
  push_scope(var);
  expect("(");
  fn->params = read_func_params();

  if (consume(";"))
    return NULL;

  // 関数のボディを読む
  Node head;
  head.next = NULL;
  Node *cur = &head;
  expect("{");
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
//      | "for" "(" (expr? ";" | declaration) expr? ";" expr? ")" stmt
//      | declaration
//      | expr ";"
//      | break ";"
//      | continue ";"
//      | "switch" "(" expr ")" stmt
//      | "case" num ":" stmt
//      | "default" ":" stmt
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
      LVarList *sc1 = scope;
      TagScope *sc2 = tag_scope;

      if (!consume(";")) {
        if (is_type()) {
          node->init = declaration();
        } else {
          node->init = new_node(ND_EXPR_STMT, expr(), NULL);
          expect(";");
        }
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

      scope = sc1;
      tag_scope = sc2;
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
    TagScope *sc2 = tag_scope; // ブロックに入るまでの情報を保持してブロック間の変数を一気に削除できる
    while (!consume("}")) {
      cur->next = stmt();
      cur = cur->next;
    }
    scope = sc;
    tag_scope = sc2;
    node = calloc(1, sizeof(Node));
    node->kind = ND_BLOCK;
    node->body = head.next;
    return node;
  }

  if (consume("break")) {
    expect(";");
    return new_node(ND_BREAK, NULL, NULL);
  }

  if (consume("continue")) {
    expect(";");
    return new_node(ND_CONTINUE, NULL, NULL);
  }

  if (consume("switch")) {
    Node *node = new_node(ND_SWITCH, NULL, NULL);
    expect("(");
    node->cond = expr();
    expect(")");

    Node *sw = current_switch;
    current_switch = node;
    node->then = stmt();
    current_switch = sw;
    return node;
  }

  if (consume("case")) {
    if (!current_switch)
      error("switch-case エラー");
    int val = expect_number();
    expect(":");

    Node *node = new_node(ND_CASE, stmt(), NULL);
    node->val = val;
    node->case_next = current_switch->case_next;
    current_switch->case_next = node;
    return node;
  }

  if (consume("default")) {
    if (!current_switch)
      error("switch-case エラー(default)");
    expect(":");

    Node *node = new_node(ND_CASE, stmt(), NULL);
    current_switch->default_case = node;
    return node;
  }

  if (consume("typedef")) {
    Type *ty = basetype();
    char *name = NULL; //typedef でつける名前
    ty = declarator(ty, &name);
    ty = read_type_suffix(ty);
    expect(";");
    // typedefの定義は変数と同様に扱う
    LVar *type_def = push_var(name, NULL, 1);
    push_scope(type_def);
    type_def->type_def = ty;
    return new_node(ND_NULL, NULL, NULL);
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

// assign = logor ("=" assign)?
static Node *assign() {
  Node *node = logor();
  if (consume("="))
    node = new_node(ND_ASSIGN, node, assign());
  if (consume("+="))
    node = new_node(ND_A_ADD, node, assign());
  if (consume("-="))
    node = new_node(ND_A_SUB, node, assign());
  if (consume("*="))
    node = new_node(ND_A_MUL, node, assign());
  if (consume("/="))
    node = new_node(ND_A_DIV, node, assign());
  return node;
}

// logor = logand ("||" logand)*
Node *logor() {
  Node *node = logand();
  Token *tok;
  while (consume("||"))
    node = new_node(ND_LOGOR, node, logand());
  return node;
}

// logand = equality ("&&" equality)*
Node *logand() {
  Node *node = equality();
  while (consume("&&"))
    node = new_node(ND_LOGAND, node, equality());
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

// mul = cast ("*" cast | "/" cast)*
static Node *mul() {
  Node *node = cast();
  for (;;) {
    if (consume("*"))
      node = new_node(ND_MUL, node, cast());
    else if (consume("/"))
      node = new_node(ND_DIV, node, cast());
    else
      return node;
  }
}

// cast = "(" type-name ")" cast | unary
Node *cast() {
  Token *tok = token;

  if (consume("(")) {
    if (is_type()) {
      Type *ty = basetype();
      ty = abstract_declarator(ty);
      ty = read_type_suffix(ty);
      expect(")");
      Node *node = new_node(ND_CAST, cast(), NULL);
      node->ty = ty;
      return node;
    }
    token = tok;
  }

  return unary();
}

// unary = ("+" | "-" | "*" | "&" | "!")? cast
//        | ("++" | "--") unary
//        | "sizeof" expr
//        | "sizeof" "(" type-name ")" 
//        | postfix
static Node *unary() {
  if (consume("+"))
    return cast();
  if (consume("-"))
    return new_node(ND_SUB, new_node_num(0), cast());
  if (consume("*"))
    return new_node(ND_DEREF, cast(), NULL);
  if (consume("&"))
    return new_node(ND_ADDR, cast(), NULL);
  if (consume("!"))
    return new_node(ND_NOT, cast(), NULL);
  if (consume("++"))
    return new_node(ND_PRE_INC, unary(), NULL);
  if (consume("--"))
    return new_node(ND_PRE_DEC, unary(), NULL);
  if (consume("sizeof")) {
    if (consume("(")) {
      if (is_type()) { // sizeof(int)のケース
        Type *ty = basetype();
        char *name = NULL;
        ty = abstract_declarator(ty);
        ty = read_type_suffix(ty);
        expect(")");
        return new_node_num(size_of(ty));
      } else { // sizeof(var) のケース
        Node *node = expr();
        expect(")");
        return new_node(ND_SIZEOF, node, NULL);
      }
    }
    return new_node(ND_SIZEOF, cast(), NULL);
  }
  return postfix();
}

// postfix = primary ("[" expr "]" | "." ident | "->" ident | "++" | "--")*
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
    } else if (consume("->")) {
      // x->y == (*x).y
      node = new_node(ND_DEREF, node, NULL);
      node = new_node(ND_MEMBER, node, NULL);
      node->member_name = expect_ident();
      continue;
    }
    if (consume("++")) {
      node = new_node(ND_POST_INC, node, NULL);
      continue;
    }
    if (consume("--")) {
      node = new_node(ND_POST_DEC, node, NULL);
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
  TagScope *sc2 = tag_scope;
  while (!consume("}")) {
    cur->next = stmt();
    cur = cur->next;
  }
  expect(")");
  if (cur->kind != ND_EXPR_STMT)
    error("stmt_exprエラー");
  *cur = *cur->lhs; // コード生成時にND_BLOCKと処理を揃えるために最後の処理をnode自体に移す
  scope = sc;
  tag_scope = sc2;
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

      LVar *sc = find_lvar(tok);
      if (sc) {
        if (sc->enum_ty) {
          return new_node_num(sc->enum_val);
        }
        if (sc->ty->kind != TY_FUNC)
          error("Func error");
        node->ty = sc->ty->return_ty;
      } else {
        node->ty = int_type();
      }
      return node;
    }

    // 変数処理
    node->kind = ND_LVAR;
    LVar *lvar = find_lvar(tok);
    if (lvar) {
      if (lvar->enum_ty)
        return new_node_num(lvar->enum_val);
      node->var = lvar;
      return node;
    }
    error_at(tok->str, "宣言されていません");
  }

  if (token->kind == TK_STR) {
    // + 1 \0の分
    Type *ty = array_of(char_type(), token->len + 1);
    // STRは参照する必要がないためpush_scopeが不要
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