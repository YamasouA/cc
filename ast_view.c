#include "9cc.h"

char *kind[] = {
    "ND_ADD",
    "ND_SUB",
    "ND_MUL",
    "ND_DIV",
    "ND_EQ",
    "ND_NE",
    "ND_LT",
    "ND_LE",
    "ND_NUM",
    "ND_ASSIGN",
    "ND_LVAR",
    "ND_RETURN",
    "ND_IF",
    "ND_FOR",
    "ND_WHILE",
    "ND_BLOCK",
    "ND_FUNC",
    "ND_ADDR",
    "ND_DEREF",
    "ND_NULL"
};

void print_ast(Node *node, int indent_level) {
    if (node == NULL)
        return;
    for (int i = 0; i < indent_level; i++) {
        printf(" ");
    }
    printf("kind: %s, type: %d, val: %d, offset: %d\n", kind[node->kind], node->ty, node->val, node->offset);
    print_ast(node->lhs, indent_level + 2);
    print_ast(node->rhs, indent_level + 2);
    print_ast(node->next, indent_level);
}

void print_func() {
    for (Function *fn = code; fn; fn = fn->next) {
        printf("===== .%s =====\n", fn->name);
        print_ast(fn->node, 0);
        printf("\n");
    }
}