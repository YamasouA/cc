#include "9cc.h"

// 入力プログラム
char *user_input;

// エラーを報告するための関数
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// エラー箇所を報告する
void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int pos = loc - user_input;
  fprintf(stderr, "%s\n", user_input);
  fprintf(stderr, "%*s", pos, " "); // pos個の空白出力
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

void set_offset() {
  for (Function *fn = code; fn; fn = fn->next) {
    int offset = 0;
    for (LVar *var = fn->locals; var; var = var->next) {
      offset += 8;
    }
    fn->stack_size = offset;
  }
}

int main(int argc, char **argv) {
  if (argc != 2)
      error("引数の個数が正しくありません");

  user_input = argv[1];
  tokenize();
  program();

  set_offset();

  // コード生成
  codegen();

  return 0;
}