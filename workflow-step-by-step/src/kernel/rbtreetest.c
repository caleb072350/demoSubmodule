#include <stdio.h>
#include <stdlib.h>
#include "rbtree.h"

typedef struct my_data my_data_t;

struct my_data {
    int key;
    struct rb_node node;    // 红黑树节点
};

// 实现插入数据到红黑树中
void insert_my_data(struct rb_root * root, my_data_t *data) {
    struct rb_node **node = &(root->rb_node), *parent = NULL;

    // 找到插入位置
    while (*node) {{
        my_data_t * dataptr = rb_entry(*node, my_data_t, node);

        parent = *node;
        if (data->key < dataptr->key) {
            node = &((*node)->rb_left);
        } else if (data->key > dataptr->key) {
            node = &((*node)->rb_right);
        } else {
            return; // key 已存在，返回
        }
    }}

    // 插入新节点
    rb_link_node(&data->node, parent, node);
    // 调整红黑树平衡
    rb_insert_color(&data->node, root);
}

// 从红黑树删除节点
void delete_my_data(struct rb_root * root, my_data_t *data) {
    rb_erase(&data->node, root);
    free(data);
}

void inorder_traverse(struct rb_node *node) {
    if (!node) return;
    inorder_traverse(node->rb_left);
    my_data_t *dataptr = rb_entry(node, my_data_t, node);
    printf("%d ", dataptr->key);
    inorder_traverse(node->rb_right);
}

int main() {
    struct rb_root tree = RB_ROOT;
    int keys[] = {10, 5, 15, 3, 7, 12, 18, 1, 4, 6, 8, 11, 14, 16, 19};

    int n = sizeof(keys) / sizeof(keys[0]);
    my_data_t **base = (my_data_t**) malloc(n * sizeof(my_data_t*));
    for (int i = 0; i < n; i++) {
        my_data_t *data = (my_data_t *)malloc(sizeof(my_data_t));
        base[i] = data;
        data->key = keys[i];
        insert_my_data(&tree, data);
    }

    // 中序遍历
    printf("Inorder traversal: \n");
    inorder_traverse(tree.rb_node);
    printf("\n");

    delete_my_data(&tree, base[0]);
    inorder_traverse(tree.rb_node);
    printf("\n");


    return 0;
}