#include "9cc.h"

Type *int_type() {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_INT;
  return ty;
}

Type *pointer_to(Type *base) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_PTR;
  ty->base = base;
  return ty;
}

// void visit(Node *node) {
//   if (!node)
//     return;
//   visit(node->lhs);
//   visit(node->rhs);
//   visit(node->cond);
//   visit(node->then);
//   visit(node->els);
//   visit(node->init);
//   visit(node->inc);

//   for (Node *n = node->body; n; n = n->next)
//     visit(n);
//   for (Node *n = node->args; n; n = n->next)
//     visit(n);

//   switch (node->kind) {
//     case ND_MUL:
//     case ND_DIV:
//     case ND_EQ:
//     case ND_NE:
//     case ND_LT:
//     case ND_LE:
//     case ND_LVAR:
//     case ND_FUNC:
//     case ND_NUM:
//       node->ty = int_type();
//     case ND_ADD:
//       if (node->rhs->ty->kind == TY_PTR) {
//         // 
//         Node *tmp = node->lhs;
//         node->lhs = node->rhs;
//         node->rhs = tmp;
//       }
//       // ポインタ同士の演算はエラー
//       if (node->rhs->ty->kind == TY_PTR) {
//         error_tok("hoge", "不正なポインタ算術演算子です");
//       }
//       node->ty = node->lhs->ty;
//       return;
//     case ND_SUB:
//       if (node->rhs->ty->kind == TY_PTR)
//         error_tok();
//       node->ty = node->lhs->ty;
//       return;
//   }
// }

// void add_type(Function *prog) {
//   for (Function *fn = prog; fn; fn = fn->next) {
//     for (Node *node = fn->node; node; node = node->next) {
//       visit(node);
//     }
//   }
// }