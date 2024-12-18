#include "rbtree.h"

/**                                      X                                     Y
 * @node: 要进行左旋转的节点             /  \                                  /  \
 * @root: 红黑树的根节点                A    Y         旋转后 ---->           X    C
 *                                         / \                             / \
 *                                        B   C                           A   B
 * 左旋是围绕某个节点进行的旋转操作，使该节点的右子节点成为其父节点，而原右子节点的左子节点则成为该节点的右子节点。
    具体步骤如下：
    设定旋转节点为X，其右子节点为Y。
    将Y的左子节点B设为X的右子节点。
    将X设为Y的左子节点。
    更新Y的父节点为X的父节点，并根据情况调整父节点的子指针。
    最后，将X的父节点设为Y。
    通过上述操作，节点X下移，节点Y上移，保持了二叉搜索树的性质。
 */
static void __rb_rotate_left(struct rb_node *node, struct rb_root *root)
{
    struct rb_node *right = node->rb_right; // right 为 Y
    if ((node->rb_right = right->rb_left))  // X 的右节点设为 Y的左节点
        right->rb_left->rb_parent = node;   // B 的父节点设为 X
    right->rb_left = node;                  // Y 的左节点设为 X

    if ((right->rb_parent = node->rb_parent)) {  // 如果X不是根节点, Y 的 parent 设为 X 的 parent
        if (node == node->rb_parent->rb_left) // 如果 X 为左子树, 
            node->rb_parent->rb_left = right; // X 的父节点的左子节点设为 Y
        else
            node->rb_parent->rb_right = right; // 否则右子节点设为 Y
    } else
        root->rb_node = right;             // root 设置为 Y
    node->rb_parent = right;               // X 的父节点设置为 Y
}


/**
 *                                            Y                                            X
 *                                           / \                                          / \
 *                                          X   C           --------------->             A   Y
 *                                         / \                                              / \
 *                                        A   B                                            B   C

右旋与左旋相反，是围绕某个节点进行的旋转操作，使该节点的左子节点成为其父节点，而原左子节点的右子节点则成为该节点的左子节点。
具体步骤如下：
设定旋转节点为Y，其左子节点为X。
将X的右子节点B设为Y的左子节点。
将Y设为X的右子节点。
更新X的父节点为Y的父节点，并根据情况调整父节点的子指针。
最后，将Y的父节点设为X。
 */

static void __rb_rotate_right(struct rb_node *node, struct rb_root *root) {
    struct rb_node *left = node->rb_left;   // X
    if ((node->rb_left = left->rb_right))   // Y->left = B
        left->rb_right->rb_parent = node;   // B->parent = Y
    
    left->rb_right = node; // X->right = Y
    
    if ((left->rb_parent = node->rb_parent)) {
        if (node == node->rb_parent->rb_left)
            node->rb_parent->rb_left = left;
        else
            node->rb_parent->rb_right = left;
    } else 
        root->rb_node = left;
    node->rb_parent = left;
}