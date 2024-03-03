#include "9cc.h"

void gen(Node *node);

// 左辺値の変数のアドレスを計算しスタックにプッシュする
void gen_lval(Node *node) {
  switch (node->kind) {
  case ND_LVAR:
    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", node->var->offset);
    printf("  push rax\n");
    return;
  case ND_DEREF:
    gen(node->lhs);
    return;
  }
  error("代入の左辺値が変数ではない");
}

int labelseq = 0;
char *funcname;
char *argreg[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

void gen(Node *node) {
  switch (node->kind) {
    case ND_RETURN:
      gen(node->lhs);
      printf("  pop rax\n");
      printf("  jmp .Lreturn.%s\n", funcname);
      return;
    case ND_NUM:
      printf("  push %d\n", node->val);
      return;
    case ND_NULL:
      return;
    case ND_LVAR:
      // raxに左辺値のアドレスをセットする
      gen_lval(node);
      // 配列ではアドレスが知れればいいので早期リターン
      if (node->ty->kind == TY_ARRAY)
        return;
      printf("  pop rax\n");
      printf("  mov rax, [rax]\n"); // raxのアドレスの値をraxにセットする
      printf("  push rax\n");
      return;
    case ND_ASSIGN:
      if (node->ty->kind == TY_ARRAY)
        error("左辺値にポインタは使えない");
      gen_lval(node->lhs);
      gen(node->rhs);

      printf("  pop rdi\n"); // 右辺値がstackのtopに入る
      printf("  pop rax\n");
      printf("  mov [rax], rdi\n"); // 右辺を左辺値へ入れる
      printf("  push rdi\n");
      return;
    case ND_ADDR:
      gen_lval(node->lhs);
      return;
    case ND_DEREF:
      gen(node->lhs);
      if (node->ty->kind == TY_ARRAY)
        return;
      printf("  pop rax\n");
      printf("  mov rax, [rax]\n");
      printf("  push rax\n");
      return;
    case ND_IF: {
      int seq = labelseq++;
      if (node->els) {
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .Lelse%d\n", seq);
        gen(node->then);
        printf("  jmp .Lend%d\n", seq);
        printf(".Lelse%d:\n", seq);
        gen(node->els);
        printf(".Lend%d:\n", seq);
      } else {
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .Lend%d\n", seq);
        gen(node->then);
        printf(".Lend%d:\n", seq);
      }
      return;
    }
    case ND_WHILE: {
      int seq = labelseq++;
      printf(".Lbegin%d:\n", seq);
      gen(node->cond);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  je  .Lend%d\n", seq);
      gen(node->then);
      printf("  jmp .Lbegin%d\n", seq);
      printf(".Lend%d:\n", seq);
      return;
    }
    case ND_FOR: {
      int seq = labelseq++;
      if (node->init)
        gen(node->init);
      printf(".Lbegin%d:\n", seq);
      if (node->cond) {
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .Lend%d\n", seq);
      }
      gen(node->then);
      if (node->inc)
        gen(node->inc);
      printf("  jmp .Lbegin%d\n", seq);
      printf(".Lend%d:\n", seq);
      return;
    }
    case ND_BLOCK: {
      for (Node *n = node->body; n; n = n->next)
        gen(n);
      return;
    }
    case ND_FUNC: {
      Node *cur = node->args;
      int len = 0;
      while (cur) {
        gen(cur);
        len++;
        cur = cur->next;
      }
      for (int i = len - 1; i >= 0; i--) {
        printf("  pop %s\n", argreg[i]);
      }
      // 関数呼び出し時にRSPは16の倍数にアラインされる必要があるa
      int seq = labelseq++;
      printf("  mov rax, rsp\n");
      printf("  and rax, 15\n");
      printf("  jnz .Lcall%d\n", seq);
      printf("  mov rax, 0\n");
      printf("  call %s\n", node->funcname);
      printf("  jmp .Lend%d\n", seq);
      printf(".Lcall%d:\n", seq);
      printf("  sub rsp, 8\n");
      printf("  mov rax, 0\n");
      printf("  call %s\n", node->funcname);
      printf("  add rsp, 8\n");
      printf(".Lend%d:\n", seq);
      printf("  push rax\n");
      return;
    }
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch (node->kind) {
    case ND_ADD:
      if (node->ty->base)
        printf("  imul rdi, %d\n", size_of(node->ty->base));
      printf("  add rax, rdi\n");
      break;
    case ND_SUB:
      if (node->ty->base)
        printf("  imul rdi, %d\n", size_of(node->ty->base));
      printf("  sub rax, rdi\n");
      break;
    case ND_MUL:
      printf("  imul rax, rdi\n");
      break;
    case ND_DIV:
      printf("  cqo\n");
      printf("  idiv rax, rdi\n");
      break;
    case ND_EQ:
      printf("  cmp rax, rdi\n");
      printf("  sete al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_NE:
      printf("  cmp rax, rdi\n");
      printf("  setne al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_LT:
      printf("  cmp rax, rdi\n");
      printf("  setl al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_LE:
      printf("  cmp rax, rdi\n");
      printf("  setle al\n");
      printf("  movzb rax, al\n");
      break;
  }

  printf("  push rax\n");
}

void codegen() {
  printf(".intel_syntax noprefix\n");

  for (Function *fn = code; fn; fn = fn->next) {
    funcname = fn->name;
    printf(".global %s\n", funcname);
    printf("%s:\n", funcname);

    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", fn->stack_size);

    // 関数の引数をスタックにpushする
    int i = 0;
    for (LVarList *vl = fn->params; vl; vl = vl->next) {
      printf("  mov [rbp-%d], %s\n", vl->var->offset, argreg[i++]);
    }

    for (Node *node = fn->node; node; node = node->next)
      gen(node);
    printf("  pop rax\n");
    // 変数分の拡張していた領域を解放する
    printf(".Lreturn.%s:\n", fn->name);
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    // スタックトップに式全体の結果が入っている
    printf("  ret\n");
  }
}