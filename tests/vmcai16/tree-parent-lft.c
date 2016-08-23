/*
 * Tree with parent pointers, destruction using a stack
 *
 * boxes: treewpp.boxes
 */

#include <stdlib.h>
#include <verifier-builtins.h>

int main() {

	struct TreeNode {
		struct TreeNode* left;
		struct TreeNode* right;
		struct TreeNode* parent;
	};

	struct StackItem {
		struct StackItem* next;
		struct TreeNode* node;
	};

	struct TreeNode* root = malloc(sizeof(*root)), *n;
	root->right = NULL;
	root->parent = NULL;
	root->left = malloc(sizeof(*n));
	root->left->parent = root;
	root->left->left = NULL;
	root->left->right = NULL;

	while (__VERIFIER_nondet_int()) {
		n = root;
		while (n->left && n->right) {
			if (__VERIFIER_nondet_int())
				n = n->left;
			else
				n = n->right;
		}
		if (!n->left && __VERIFIER_nondet_int()) {
			n->left = malloc(sizeof(*n));
			n->left->right = NULL;
			n->left->parent = n;
			n->left->left = malloc(sizeof(*n));
			n->left->left->parent = n->left;
			n->left->left->left = NULL;
			n->left->left->right = NULL;
		}
		if (!n->right && __VERIFIER_nondet_int()) {
			n->right = malloc(sizeof(*n));
			n->right->right = NULL;
			n->right->parent = n;
			n->right->left = malloc(sizeof(*n));
			n->right->left->parent = n->right;
			n->right->left->left = NULL;
			n->right->left->right = NULL;
		}
	}

	n = NULL;

	struct StackItem* s = malloc(sizeof(*s)), *st;
	s->next = NULL;
	s->node = root;

	while (s != NULL) {
		st = s;
		s = s->next;
		n = st->node;
		free(st);
		if (n->left) {
			st = malloc(sizeof(*st));
			st->next = s;
			st->node = n->left;
			s = st;
			__VERIFIER_assert(st->node->left != NULL || (st->node->left == NULL && st->node->right == NULL));
		}
		if (n->right) {
			st = malloc(sizeof(*st));
			st->next = s;
			st->node = n->right;
			s = st;
		}
		free(n);
	}

	return 0;
}
