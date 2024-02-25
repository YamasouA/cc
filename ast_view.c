#include "9cc.h"

void print_ast(Node *node, int indent_level) {
    if (node == NULL)
        return;
    for (int i = 0; i < indent_level; i++) {
        printf(" ");
    }
    printf("kind: %d, type: %s, val: %d, offset: %d\n", node->kind, node->ty, node->val, node->offset);
    print_ast(node->lhs, indent_level + 2);
    print_ast(node->rhs, indent_level + 2);
}

void print_func() {
    for (Function *fn = code; fn; fn = fn->next) {
        printf("===== .%s =====\n", fn->name);
        print_ast(fn->node, 0);
        printf("\n");
    }
}