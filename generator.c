#include "9cc.h"

void gen(Node *node);

void inc(Type *ty) {
  printf("  pop rax\n");
  // ポインタなら型のサイズに合わせて動かす
  printf("  add rax, %d\n", ty->base ? size_of(ty->base) : 1);
  printf("  push rax\n");
}

void dec(Type *ty) {
  printf("  pop rax\n");
  printf("  sub rax, %d\n", ty->base ? size_of(ty->base) : 1);
  printf("  push rax\n");
}

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
    gen_lval(node->lhs); // 構造体がlhsにいる
    printf("  pop rax\n");
    printf("  add rax, %d\n", node->member->offset);
    printf("  push rax\n");
    return;
  }
  error("代入の左辺値が変数ではない");
}

void truncate(Type *ty) {
  printf("  pop rax\n");

  if (ty->kind == TY_BOOL) {
    printf("  cmp rax, 0\n");
    printf("  setne al\n");
  }

  int sz = size_of(ty);
  if (sz == 1) {
    printf("  movsx rax, al\n");
  } else if (sz == 2) {
    printf("  movsx rax, ax\n");
  } else if (sz == 4) {
    printf("  movsxd rax, eax\n");
  }
  printf("  push rax\n");
}

int labelseq;
int brkseq;
int contseq;
char *funcname;
// charのような1バイトでは上位のビットが0にリセットされない
char *argreg1[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
char *argreg2[] = {"di", "si", "dx", "cx", "r8w", "r9w"};
char *argreg4[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
char *argreg8[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

void gen(Node *node) {
  switch (node->kind) {
    case ND_RETURN:
      gen(node->lhs);
      printf("  pop rax\n");
      printf("  jmp .Lreturn.%s\n", funcname);
      return;
    case ND_CAST:
      gen(node->lhs);
      truncate(node->ty);
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
      else if (size_of(node->ty) == 2)
        printf("  movsx rax, word ptr [rax]\n"); // raxのアドレスの値をraxにセットする
      else if (size_of(node->ty) == 4)
        printf("  movsxd rax, dword ptr [rax]\n"); // raxのアドレスの値をraxにセットする
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

      if (node->ty->kind == TY_BOOL) {
        printf("  cmp rdi, 0\n");
        printf("  setne dil\n");
        printf("  movzb rdi, dil\n");
      }

      if (size_of(node->ty) == 1)
        printf("  mov [rax], dil\n"); // 右辺を左辺値へ入れる
      else if (size_of(node->ty) == 2)
        printf("  mov [rax], di\n"); // raxのアドレスの値をraxにセットする
      else if (size_of(node->ty) == 4)
        printf("  mov [rax], edi\n"); // raxのアドレスの値をraxにセットする
      else
        printf("  mov [rax], rdi\n"); // 右辺を左辺値へ入れる
      printf("  push rdi\n");
      return;
    case ND_TERNARY: {
      int seq = labelseq++;
      gen(node->cond);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  je .Lelse%d\n", seq);
      gen(node->then);
      printf("  jmp .Lend%d\n", seq);
      printf(".Lelse%d:\n", seq);
      gen(node->els);
      printf(".Lend%d:\n", seq);
      return;
    }
    case ND_PRE_INC:
      gen_lval(node->lhs);
      printf("  push [rsp]\n");
      printf("  pop rax\n");
      int sz = size_of(node->ty);
      if (sz == 1) {
        printf("  movsx rax, byte ptr [rax]\n");
      } else if (sz == 2) {
        printf("  movsx rax, word ptr [rax]\n");
      } else if (sz == 4) {
        printf("  movsxd rax, dword ptr [rax]\n");
      } else {
        assert(sz == 8);
        printf("  mov rax, [rax]\n");
      }
      printf("  push rax\n");
      inc(node->ty);
      printf("  pop rdi\n"); // 右辺値がstackのtopに入る
      printf("  pop rax\n");

      if (node->ty->kind == TY_BOOL) {
        printf("  cmp rdi, 0\n");
        printf("  setne dil\n");
        printf("  movzb rdi, dil\n");
      }

      if (size_of(node->ty) == 1)
        printf("  mov [rax], dil\n"); // 右辺を左辺値へ入れる
      else if (size_of(node->ty) == 2)
        printf("  mov [rax], di\n"); // raxのアドレスの値をraxにセットする
      else if (size_of(node->ty) == 4)
        printf("  mov [rax], edi\n"); // raxのアドレスの値をraxにセットする
      else
        printf("  mov [rax], rdi\n"); // 右辺を左辺値へ入れる
      printf("  push rdi\n");
      return; 
    case ND_PRE_DEC: {
      gen_lval(node->lhs);
      printf("  push [rsp]\n");
      printf("  pop rax\n");
      int sz = size_of(node->ty);
      if (sz == 1) {
        printf("  movsx rax, byte ptr [rax]\n");
      } else if (sz == 2) {
        printf("  movsx rax, word ptr [rax]\n");
      } else if (sz == 4) {
        printf("  movsxd rax, dword ptr [rax]\n");
      } else {
        assert(sz == 8);
        printf("  mov rax, [rax]\n");
      }
      printf("  push rax\n");
      dec(node->ty);
      printf("  pop rdi\n"); // 右辺値がstackのtopに入る
      printf("  pop rax\n");

      if (node->ty->kind == TY_BOOL) {
        printf("  cmp rdi, 0\n");
        printf("  setne dil\n");
        printf("  movzb rdi, dil\n");
      }

      if (size_of(node->ty) == 1)
        printf("  mov [rax], dil\n"); // 右辺を左辺値へ入れる
      else if (size_of(node->ty) == 2)
        printf("  mov [rax], di\n"); // raxのアドレスの値をraxにセットする
      else if (size_of(node->ty) == 4)
        printf("  mov [rax], edi\n"); // raxのアドレスの値をraxにセットする
      else
        printf("  mov [rax], rdi\n"); // 右辺を左辺値へ入れる
      printf("  push rdi\n");
      return;
    }
    case ND_POST_INC: {
      gen_lval(node->lhs);
      printf("  push [rsp]\n");
      printf("  pop rax\n");
      int sz = size_of(node->ty);
      if (sz == 1) {
        printf("  movsx rax, byte ptr [rax]\n");
      } else if (sz == 2) {
        printf("  movsx rax, word ptr [rax]\n");
      } else if (sz == 4) {
        printf("  movsxd rax, dword ptr [rax]\n");
      } else {
        assert(sz == 8);
        printf("  mov rax, [rax]\n");
      }
      printf("  push rax\n");
      inc(node->ty);
      printf("  pop rdi\n"); // 右辺値がstackのtopに入る
      printf("  pop rax\n");

      if (node->ty->kind == TY_BOOL) {
        printf("  cmp rdi, 0\n");
        printf("  setne dil\n");
        printf("  movzb rdi, dil\n");
      }

      if (size_of(node->ty) == 1)
        printf("  mov [rax], dil\n"); // 右辺を左辺値へ入れる
      else if (size_of(node->ty) == 2)
        printf("  mov [rax], di\n"); // raxのアドレスの値をraxにセットする
      else if (size_of(node->ty) == 4)
        printf("  mov [rax], edi\n"); // raxのアドレスの値をraxにセットする
      else
        printf("  mov [rax], rdi\n"); // 右辺を左辺値へ入れる
      printf("  push rdi\n");
      dec(node->ty); // 　incした値をstoreした後にdecすることでinc前の値で評価を行う
      return;
    }
    case ND_POST_DEC: {
      gen_lval(node->lhs);
      printf("  push [rsp]\n");
      printf("  pop rax\n");
      int sz = size_of(node->ty);
      if (sz == 1) {
        printf("  movsx rax, byte ptr [rax]\n");
      } else if (sz == 2) {
        printf("  movsx rax, word ptr [rax]\n");
      } else if (sz == 4) {
        printf("  movsxd rax, dword ptr [rax]\n");
      } else {
        assert(sz == 8);
        printf("  mov rax, [rax]\n");
      }
      printf("  push rax\n");
      dec(node->ty);
      printf("  pop rdi\n"); // 右辺値がstackのtopに入る
      printf("  pop rax\n");

      if (node->ty->kind == TY_BOOL) {
        printf("  cmp rdi, 0\n");
        printf("  setne dil\n");
        printf("  movzb rdi, dil\n");
      }

      if (size_of(node->ty) == 1)
        printf("  mov [rax], dil\n"); // 右辺を左辺値へ入れる
      else if (size_of(node->ty) == 2)
        printf("  mov [rax], di\n"); // raxのアドレスの値をraxにセットする
      else if (size_of(node->ty) == 4)
        printf("  mov [rax], edi\n"); // raxのアドレスの値をraxにセットする
      else
        printf("  mov [rax], rdi\n"); // 右辺を左辺値へ入れる
      printf("  push rdi\n");
      inc(node->ty);
      return;
    }
    case ND_A_ADD:
    case ND_A_SUB:
    case ND_A_MUL:
    case ND_A_DIV: {
      gen_lval(node->lhs);
      printf("  push [rsp]\n");

      printf("  pop rax\n");
      int sz = size_of(node->ty);
      if (sz == 1) {
        printf("  movsx rax, byte ptr [rax]\n");
      } else if (sz == 2) {
        printf("  movsx rax, word ptr [rax]\n");
      } else if (sz == 4) {
        printf("  movsxd rax, dword ptr [rax]\n");
      } else {
        assert(sz == 8);
        printf("  mov rax, [rax]\n");
      }
      printf("  push rax\n");

      gen(node->rhs);
      printf("  pop rdi\n");
      printf("  pop rax\n");

      switch(node->kind) {
        case ND_A_ADD:
          if (node->ty->base)
            printf("  imul rdi, %d\n", size_of(node->ty->base));
          printf("  add rax, rdi\n");
          break;
        case ND_A_SUB:
          if (node->ty->base)
            printf("  imul rdi, %d\n", size_of(node->ty->base));
          printf("  sub rax, rdi\n");
          break;
        case ND_A_MUL:
          printf("  imul rax, rdi\n");
          break;
        case ND_A_DIV:
          printf("  cqo\n");
          printf("  idiv rdi\n");
          break;
      }

      printf("  push rax\n");
      printf("  pop rdi\n");
      printf("  pop rax\n");

      if (node->ty->kind == TY_BOOL) {
        printf("  cmp rdi, 0\n");
        printf("  setne dil\n");
        printf("  movzb rdi, dil\n");
      }

      sz = size_of(node->ty);
      if (sz == 1) {
        printf("  mov [rax], dil\n");
      } else if (sz == 2) {
        printf("  mov [rax], di\n");
      } else if (sz == 4) {
        printf("  mov [rax], edi\n");
      } else {
        assert(sz == 8);
        printf("  mov [rax], rdi\n");
      }

      printf("  push rdi\n");
      return;
    }
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
      else if (size_of(node->ty) == 2)
        printf("  movsx rax, word ptr [rax]\n"); // raxのアドレスの値をraxにセットする
      else if (size_of(node->ty) == 4)
        printf("  movsxd rax, dword ptr [rax]\n"); // raxのアドレスの値をraxにセットする
      else
        printf("  mov rax, [rax]\n"); // raxのアドレスの値をraxにセットする
      printf("  push rax\n");
      return;
    case ND_NOT: {
      gen(node->lhs);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  sete al\n");
      printf("  movzb rax, al\n");
      printf("  push rax\n");
      return;
    }
    case ND_LOGAND: {
      int seq = labelseq++;
      gen(node->lhs);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  je .Lfalse%d\n", seq); // 左辺値がfalseの時点で終了
      gen(node->rhs);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  je .Lfalse%d\n", seq);
      printf("  push 1\n");
      printf("  jmp .Lend%d\n", seq);
      printf(".Lfalse%d:\n", seq);
      printf("  push 0\n");
      printf(".Lend%d:\n", seq);
      return;
    }
    case ND_LOGOR: {
      int seq = labelseq++;
      gen(node->lhs);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  jne .Ltrue%d\n", seq);
      gen(node->rhs);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  jne .Ltrue%d\n", seq);
      printf("  push 0\n");
      printf("  jmp .Lend%d\n", seq);
      printf(".Ltrue%d:\n", seq);
      printf("  push 1\n");
      printf(".Lend%d:\n", seq);
      return;
    }
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
      int brk = brkseq;
      int cont = contseq;
      brkseq = seq;
      contseq = seq;
      printf(".L.continue.%d:\n", seq);
      gen(node->cond);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  je  .L.break.%d\n", seq);
      gen(node->then);
      printf("  jmp .L.continue.%d\n", seq);
      printf(".L.break.%d:\n", seq);

      brkseq = brk;
      contseq = cont;
      return;
    }
    case ND_FOR: {
      int seq = labelseq++;
      int brk = brkseq;
      int cont = contseq;
      brkseq = seq;
      contseq = seq;

      if (node->init)
        gen(node->init);
      printf(".Lbegin%d:\n", seq);
      if (node->cond) {
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .L.break.%d\n", seq);
      }
      gen(node->then);
      printf(".L.continue.%d:\n", seq);
      if (node->inc)
        gen(node->inc);
      printf("  jmp .Lbegin%d\n", seq);
      printf(".L.break.%d:\n", seq);

      brkseq = brk;
      contseq = cont;
      return;
    }
    case ND_SWITCH: {
      int seq = labelseq++;
      int brk = brkseq;
      brkseq = seq;
      node->case_label = seq;

      gen(node->cond);
      printf("  pop rax\n");

      for (Node *n = node->case_next; n; n = n->case_next) {
        n->case_label = labelseq++;
        n->case_end_label = seq;
        printf("  cmp rax, %ld\n", n->val);
        printf("  je .L.case.%d\n", n->case_label);
      }

      if (node->default_case) {
        int i = labelseq++;
        node->default_case->case_end_label = seq;
        node->default_case->case_label = i;
        printf("  jmp .L.case.%d\n", i);
      }

      printf("  jmp .L.break.%d\n", seq);
      gen(node->then);
      printf(".L.break.%d:\n", seq);

      brkseq = brk;
      return;
    }
    case ND_CASE:
      printf(".L.case.%d:\n", node->case_label);
      gen(node->lhs);
      printf("  jmp .L.break.%d\n", node->case_end_label);
      return;
    case ND_BLOCK:
    case ND_STMT_EXPR:
      for (Node *n = node->body; n; n = n->next)
        gen(n);
      return;
    case ND_BREAK:
      if (brkseq == 0)
        error("breakエラー");
      printf("  jmp .L.break.%d\n", brkseq);
      return;
    case ND_CONTINUE:
      if (contseq == 0)
        error("continueエラー");
      printf("  jmp .L.continue.%d\n", contseq);
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

      if (node->ty->kind != TY_VOID)
        truncate(node->ty);
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
    if (!fn->is_static)
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
      } else if (size == 2) {
        printf("  mov [rbp-%d], %s\n", vl->var->offset, argreg2[i++]);
      } else if (size == 4) {
        printf("  mov [rbp-%d], %s\n", vl->var->offset, argreg4[i++]);
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
    if (!var->initializer) {
      printf("  .zero %d\n", size_of(var->ty));
    }
    
    for (Initializer *init = var->initializer; init; init = init->next) {
      if (init->label) {
        printf("  .quad \"%s\"%+ld\n", init->label, init->addend);
      } else if (init->contents) {
        printf("  .string \"%s\"\n", init->contents);
      } else if (init->sz == 1) {
        printf("  .byte %ld\n", init->val);
      } else {
        printf("  .%dbyte %ld\n", init->sz, init->val);
      }
    }
  }
}

void codegen() {
  printf(".intel_syntax noprefix\n");
  emit_data();
  emit_text();
}