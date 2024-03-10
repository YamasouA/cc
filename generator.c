#include "9cc.h"

void gen(Node *node);

// 左辺値の変数のアドレスを計算しスタックにプッシュする
void gen_lval(Node *node) {
  switch (node->kind) {
  case ND_LVAR:
    if (node->var->is_local) {
      printf("  mov rax, rbp\n");
      printf("  sub rax, %d\n", node->var->offset);
      printf("  push rax\n");
    } else {
      printf("  push offset %s\n", node->var->name);
    }
    return;
  case ND_DEREF:
    gen(node->lhs);
    return;
  case ND_MEMBER:
    gen_lval(node->lhs);
    printf("  pop rax\n");
    printf("  add rax, %d\n", node->member->offset);
    printf("  push rax\n");
    return;
  }
  error("代入の左辺値が変数ではない");
}

int labelseq = 0;
char *funcname;
// charのような1バイトでは上位のビットが0にリセットされない
char *argreg1[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
char *argreg8[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

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
    case ND_EXPR_STMT:
      gen(node->lhs);
      printf("  add rsp, 8\n"); // 副作用の結果をスタックから取り除く
      return;
    case ND_NULL:
      return;
    case ND_LVAR:
    case ND_MEMBER:
      // raxに左辺値のアドレスをセットする
      gen_lval(node);
      // 配列ではアドレスが知れればいいので早期リターン
      if (node->ty->kind == TY_ARRAY)
        return;
      printf("  pop rax\n");
      if (size_of(node->ty) == 1)
        printf("  movsx rax, byte ptr [rax]\n"); // raxのアドレスの値をraxにセットする
      else
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
      if (size_of(node->ty) == 1)
        printf("  mov [rax], dil\n"); // 右辺を左辺値へ入れる
      else
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
      if (size_of(node->ty) == 1)
        printf("  movsx rax, byte ptr [rax]\n"); // raxのアドレスの値をraxにセットする
      else
        printf("  mov rax, [rax]\n"); // raxのアドレスの値をraxにセットする
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
    case ND_BLOCK:
    case ND_STMT_EXPR:
      for (Node *n = node->body; n; n = n->next)
        gen(n);
      return;
    case ND_FUNC: {
      Node *cur = node->args;
      int len = 0;
      while (cur) {
        gen(cur);
        len++;
        cur = cur->next;
      }
      for (int i = len - 1; i >= 0; i--) {
        printf("  pop %s\n", argreg8[i]);
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

void emit_text() {
  printf(".text\n");
  for (Function *fn = code->fns; fn; fn = fn->next) {
    funcname = fn->name;
    printf(".global %s\n", funcname);
    printf("%s:\n", funcname);

    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", fn->stack_size);

    // 関数の引数をスタックにpushする
    int i = 0;
    for (LVarList *vl = fn->params; vl; vl = vl->next) {
      int size = size_of(vl->var->ty);
      if (size == 1) {
        printf("  mov [rbp-%d], %s\n", vl->var->offset, argreg1[i++]);
      } else {
        assert(size == 8);
        printf("  mov [rbp-%d], %s\n", vl->var->offset, argreg8[i++]);
      }
    }

    for (Node *node = fn->node; node; node = node->next)
      gen(node);
    // 変数分の拡張していた領域を解放する
    printf(".Lreturn.%s:\n", fn->name);
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    // スタックトップに式全体の結果が入っている
    printf("  ret\n");
  }
}

// global変数
// x:
//   .zero 4
// 的なやつ
void emit_data() {
  printf(".data\n");

  for (LVarList *vl = code->globals; vl; vl = vl->next) {
    LVar *var = vl->var;
    printf("%s:\n", var->name);
    if (!var->contents) {
      printf("  .zero %d\n", size_of(var->ty));
    } else {
      printf("  .string \"%s\"\n", var->contents);
    }
    
  }
}

void codegen() {
  printf(".intel_syntax noprefix\n");
  emit_data();
  emit_text();
}