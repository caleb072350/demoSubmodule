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

void rb_insert_color(struct rb_node *node, struct rb_root *root)
{
	struct rb_node *parent, *gparent;

	while ((parent = node->rb_parent) && parent->rb_color == RB_RED)
	{
		gparent = parent->rb_parent;

		if (parent == gparent->rb_left)
		{
			{
				register struct rb_node *uncle = gparent->rb_right;
				if (uncle && uncle->rb_color == RB_RED)
				{
					uncle->rb_color = RB_BLACK;
					parent->rb_color = RB_BLACK;
					gparent->rb_color = RB_RED;
					node = gparent;
					continue;
				}
			}

			if (parent->rb_right == node)
			{
				register struct rb_node *tmp;
				__rb_rotate_left(parent, root);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			parent->rb_color = RB_BLACK;
			gparent->rb_color = RB_RED;
			__rb_rotate_right(gparent, root);
		} else {
			{
				register struct rb_node *uncle = gparent->rb_left;
				if (uncle && uncle->rb_color == RB_RED)
				{
					uncle->rb_color = RB_BLACK;
					parent->rb_color = RB_BLACK;
					gparent->rb_color = RB_RED;
					node = gparent;
					continue;
				}
			}

			if (parent->rb_left == node)
			{
				register struct rb_node *tmp;
				__rb_rotate_right(parent, root);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			parent->rb_color = RB_BLACK;
			gparent->rb_color = RB_RED;
			__rb_rotate_left(gparent, root);
		}
	}

	root->rb_node->rb_color = RB_BLACK;
}

static void __rb_erase_color(struct rb_node *node, struct rb_node *parent,
			     struct rb_root *root)
{
	struct rb_node *other;

	while ((!node || node->rb_color == RB_BLACK) && node != root->rb_node)
	{
		if (parent->rb_left == node)
		{
			other = parent->rb_right;
			if (other->rb_color == RB_RED)
			{
				other->rb_color = RB_BLACK;
				parent->rb_color = RB_RED;
				__rb_rotate_left(parent, root);
				other = parent->rb_right;
			}
			if ((!other->rb_left ||
			     other->rb_left->rb_color == RB_BLACK)
			    && (!other->rb_right ||
				other->rb_right->rb_color == RB_BLACK))
			{
				other->rb_color = RB_RED;
				node = parent;
				parent = node->rb_parent;
			}
			else
			{
				if (!other->rb_right ||
				    other->rb_right->rb_color == RB_BLACK)
				{
					register struct rb_node *o_left;
					if ((o_left = other->rb_left))
						o_left->rb_color = RB_BLACK;
					other->rb_color = RB_RED;
					__rb_rotate_right(other, root);
					other = parent->rb_right;
				}
				other->rb_color = parent->rb_color;
				parent->rb_color = RB_BLACK;
				if (other->rb_right)
					other->rb_right->rb_color = RB_BLACK;
				__rb_rotate_left(parent, root);
				node = root->rb_node;
				break;
			}
		}
		else
		{
			other = parent->rb_left;
			if (other->rb_color == RB_RED)
			{
				other->rb_color = RB_BLACK;
				parent->rb_color = RB_RED;
				__rb_rotate_right(parent, root);
				other = parent->rb_left;
			}
			if ((!other->rb_left ||
			     other->rb_left->rb_color == RB_BLACK)
			    && (!other->rb_right ||
				other->rb_right->rb_color == RB_BLACK))
			{
				other->rb_color = RB_RED;
				node = parent;
				parent = node->rb_parent;
			}
			else
			{
				if (!other->rb_left ||
				    other->rb_left->rb_color == RB_BLACK)
				{
					register struct rb_node *o_right;
					if ((o_right = other->rb_right))
						o_right->rb_color = RB_BLACK;
					other->rb_color = RB_RED;
					__rb_rotate_left(other, root);
					other = parent->rb_left;
				}
				other->rb_color = parent->rb_color;
				parent->rb_color = RB_BLACK;
				if (other->rb_left)
					other->rb_left->rb_color = RB_BLACK;
				__rb_rotate_right(parent, root);
				node = root->rb_node;
				break;
			}
		}
	}
	if (node)
		node->rb_color = RB_BLACK;
}

void rb_erase(struct rb_node *node, struct rb_root *root)
{
	struct rb_node *child, *parent;
	int color;

	if (!node->rb_left)
		child = node->rb_right;
	else if (!node->rb_right)
		child = node->rb_left;
	else
	{
		struct rb_node *old = node, *left;

		node = node->rb_right;
		while ((left = node->rb_left))
			node = left;
		child = node->rb_right;
		parent = node->rb_parent;
		color = node->rb_color;

		if (child)
			child->rb_parent = parent;
		if (parent)
		{
			if (parent->rb_left == node)
				parent->rb_left = child;
			else
				parent->rb_right = child;
		}
		else
			root->rb_node = child;

		if (node->rb_parent == old)
			parent = node;
		node->rb_parent = old->rb_parent;
		node->rb_color = old->rb_color;
		node->rb_right = old->rb_right;
		node->rb_left = old->rb_left;

		if (old->rb_parent)
		{
			if (old->rb_parent->rb_left == old)
				old->rb_parent->rb_left = node;
			else
				old->rb_parent->rb_right = node;
		} else
			root->rb_node = node;

		old->rb_left->rb_parent = node;
		if (old->rb_right)
			old->rb_right->rb_parent = node;
		goto color;
	}

	parent = node->rb_parent;
	color = node->rb_color;

	if (child)
		child->rb_parent = parent;
	if (parent)
	{
		if (parent->rb_left == node)
			parent->rb_left = child;
		else
			parent->rb_right = child;
	}
	else
		root->rb_node = child;

 color:
	if (color == RB_BLACK)
		__rb_erase_color(child, parent, root);
}

/*
 * This function returns the first node (in sort order) of the tree.
 */
struct rb_node *rb_first(struct rb_root *root)
{
	struct rb_node *n;

	n = root->rb_node;
	if (!n)
		return (struct rb_node *)0;
	while (n->rb_left)
		n = n->rb_left;
	return n;
}

struct rb_node *rb_last(struct rb_root *root)
{
	struct rb_node *n;

	n = root->rb_node;
	if (!n)
		return (struct rb_node *)0;
	while (n->rb_right)
		n = n->rb_right;
	return n;
}

struct rb_node *rb_next(struct rb_node *node)
{
	/* If we have a right-hand child, go down and then left as far
	   as we can. */
	if (node->rb_right) {
		node = node->rb_right; 
		while (node->rb_left)
			node = node->rb_left;
		return node;
	}

	/* No right-hand children.  Everything down and left is
	   smaller than us, so any 'next' node must be in the general
	   direction of our parent. Go up the tree; any time the
	   ancestor is a right-hand child of its parent, keep going
	   up. First time it's a left-hand child of its parent, said
	   parent is our 'next' node. */
	while (node->rb_parent && node == node->rb_parent->rb_right)
		node = node->rb_parent;

	return node->rb_parent;
}

struct rb_node *rb_prev(struct rb_node *node)
{
	/* If we have a left-hand child, go down and then right as far
	   as we can. */
	if (node->rb_left) {
		node = node->rb_left; 
		while (node->rb_right)
			node = node->rb_right;
		return node;
	}

	/* No left-hand children. Go up till we find an ancestor which
	   is a right-hand child of its parent */
	while (node->rb_parent && node == node->rb_parent->rb_left)
		node = node->rb_parent;

	return node->rb_parent;
}
