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

char *read_file(char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp)
    error("cannot open %s: %s", path, strerror(errno));

  // ファイルの長さを調べる
  if (fseek(fp, 0, SEEK_END) == -1)
    error("%s: fseek: %s", path, strerror(errno));
  size_t size = ftell(fp);
  if (fseek(fp, 0, SEEK_SET) == -1)
    error("%s: fseek: %s", path, strerror(errno));

  // ファイル内容を読み込む
  char *buf = calloc(1, size + 2);
  fread(buf, size, 1, fp);

  // ファイルを\n\0で終わらせる
  if (size == 0 || buf[size - 1] != '\n')
    buf[size++] = '\n';
  buf[size] = '\0';
  fclose(fp);
  return buf;
}

void set_offset() {
  for (Function *fn = code->fns; fn; fn = fn->next) {
    int offset = 0;
    for (LVarList *vl = fn->locals; vl; vl = vl->next) {
      offset = align_to(offset, vl->var->ty->align);  
      offset += size_of(vl->var->ty);
      vl->var->offset = offset;
    }
    fn->stack_size = offset;
  }
}

int main(int argc, char **argv) {
  if (argc != 2)
      error("引数の個数が正しくありません");

  filename = argv[1];
  user_input = read_file(argv[1]);
  tokenize();
  program();

  add_type();

  set_offset();
  
  // // コード生成
  codegen();

  return 0;
}