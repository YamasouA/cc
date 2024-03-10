#include "9cc.h"

Type *int_type() {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_INT;
  return ty;
}

Type *char_type() {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_CHAR;
  return ty;
}

Type *pointer_to(Type *base) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_PTR;
  ty->base = base;
  return ty;
}

Type *array_of(Type *base, int size) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_ARRAY;
  ty->base = base;
  ty->array_size = size;
  return ty;
}

Type *struct_type() {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_STRUCT;
  return ty;
}

int size_of(Type *ty) {
  if(ty->kind == TY_INT || ty->kind == TY_PTR)
    return 8;
  if (ty->kind == TY_CHAR)
    return 1;
  else if (ty->kind == TY_ARRAY)
    return size_of(ty->base) * ty->array_size;
  else if (ty->kind == TY_STRUCT) {
    Member *mem = ty->members;
    while (mem->next)
      mem = mem->next;
    return mem->offset + size_of(mem->ty);
  }
  exit(1); // エラー処理めんどいので省く
}

Member *find_member(Type *ty, char *name) {
  assert(ty->kind == TY_STRUCT);
  for (Member *mem = ty->members; mem; mem = mem->next)
    if (!strcmp(mem->name, name))
      return mem;
  return NULL;
}

void visit(Node *node) {
  if (!node)
    return;
  visit(node->lhs);
  visit(node->rhs);
  visit(node->cond);
  visit(node->then);
  visit(node->els);
  visit(node->init);
  visit(node->inc);

  for (Node *n = node->body; n; n = n->next)
    visit(n);
  for (Node *n = node->args; n; n = n->next)
    visit(n);

  switch (node->kind) {
    case ND_MUL:
    case ND_DIV:
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_FUNC:
    case ND_NUM:
      node->ty = int_type();
      return;
    case ND_LVAR:
      node->ty = node->var->ty;
      return;
    case ND_ADD:
      if (node->rhs->ty->base) {
        // Assignするときに左辺値の型を見るから
        Node *tmp = node->lhs;
        node->lhs = node->rhs;
        node->rhs = tmp;
      }
      // ポインタ同士の演算はエラー
      if (node->rhs->ty->base) {
        error_at("hoge", "不正なポインタ算術演算子です");
      }
      node->ty = node->lhs->ty;
      return;
    case ND_SUB:
      if (node->rhs->ty->base)
        error_at("fuga", "不正なポインタ算術演算子です");
      node->ty = node->lhs->ty;
      return;
    case ND_ASSIGN:
      node->ty = node->lhs->ty;
      return;
    case ND_MEMBER:
      if (node->lhs->ty->kind != TY_STRUCT)
        error("ND_MEMBER エラー");
      node->member = find_member(node->lhs->ty, node->member_name);
      if (!node->member)
        error("ND_MEMBER エラー2");
      node->ty = node->member->ty;
      return;
    case ND_ADDR:
      if (node->lhs->ty->kind == TY_ARRAY)
        node->ty = pointer_to(node->lhs->ty->base);
      else
        node->ty = pointer_to(node->lhs->ty);
      return;
    case ND_DEREF:
      if (!node->lhs->ty->base)
        error("ND_DEREF エラー");
      node->ty = node->lhs->ty->base;
      return;
    case ND_SIZEOF:
      node->kind = ND_NUM;
      node->ty = int_type();
      node->val = size_of(node->lhs->ty);
      node->lhs = NULL;
      return;
    case ND_STMT_EXPR: {
      Node *last = node->body;
      while (last->next)
        last = last->next;
      // bodyの返り値の型をこのノードの型にセットする
      node->ty = last->ty;
      return;
    }
  }
}

void add_type() {
  for (Function *fn = code->fns; fn; fn = fn->next) {
    for (Node *node = fn->node; node; node = node->next) {
      visit(node);
    }
  }
}