/*
 * Tree constructor
 *
 * boxes:
 */

#include <stdlib.h>
#include <verifier-builtins.h>

int main() {

	struct TreeNode {
		struct TreeNode* left;
		struct TreeNode* right;
		int num;
	};

	struct TreeNode* root = malloc(sizeof(struct TreeNode));
	struct TreeNode* n = root;
	root->left = NULL;
	root->right = NULL;
	root->num = 0;

	while (__VERIFIER_nondet_int()) {
		n = root;
		__VERIFIER_assert(root != NULL);
		__VERIFIER_assert(n != NULL);

		while (n->left != NULL || n->right != NULL) {
			__VERIFIER_assert(n != NULL);
			if (n->left != NULL && n->right != NULL && __VERIFIER_nondet_int())
				n = n->left;
			else if (n->right != NULL)
				n = n->right;
			else if (n->left != NULL)
				n = n->left;
			else
				__VERIFIER_assert(0);
		}

		if (__VERIFIER_nondet_int()) {
			n->left = malloc(sizeof(*n));
			n->left->left = NULL;
			n->left->right = NULL;
			n->left->num = 0;
			n->right = malloc(sizeof(*n));
			n->right->left = NULL;
			n->right->right = NULL;
			n->right->num = 0;
			n->num = 2;
		}
		else if (__VERIFIER_nondet_int()) {
			n->right = malloc(sizeof(*n));
			n->right->left = NULL;
			n->right->right = NULL;
			n->right->num = 0;
			n->num = 1;
			n->left = NULL;
		}
		else if (__VERIFIER_nondet_int()) {
			n->left = malloc(sizeof(*n));
			n->left->left = NULL;
			n->left->right = NULL;
			n->left->num = 0;
			n->num = 1;
			n->right = NULL;
		}
		else
		{
			n->num = 0;
			n->left = NULL;
			n->right = NULL;
		}
	}

	while (__VERIFIER_nondet_int())
	{	// go throught the tree
		n = root;
		__VERIFIER_assert(n != NULL);
		while (n != NULL)
		{
			__VERIFIER_assert((n->num == 2 && (n->left != NULL && n->right != NULL))
					|| (n->num == 1 && (n->left != NULL || n->right != NULL))
					|| (n->num == 0 && (n->left == NULL && n->right == NULL)));
			__VERIFIER_assert(n != NULL);
			__VERIFIER_assert(root != NULL);

			if (n->num == 2 && __VERIFIER_nondet_int())
			{
				__VERIFIER_assert(n->right != NULL);
				if (n->left->left != NULL)
				{
					struct TreeNode* t = n->left;
					n->left = n->left->left;
					t->left = t->left->left;
					n->left->left = t;
					/*
					struct TreeNode* t = n->left;
					struct TreeNode* tt = n->left->left->left;
					n->left = n->left->left;
					//t->left->left = t;
					n->left->left = t; // this should be equivalent to the line above
					t->left = tt;
					*/

					n->num = (n->left != NULL) + (n->right != NULL);
					n->left->num = (n->left->left != NULL) + (n->left->right != NULL);
				}
				n = n->left;
			}
			else if (n->num == 2)
			{
				__VERIFIER_assert(n->left != NULL);
				n = n->right;
			}
			else if (n->num == 1 && NULL == n->right)
			{
				n = n->left;
			}
			else if (n->num == 1 && NULL == n->left)
			{
				n = n->right;
			}
			else if (n->num == 0)
			{
				__VERIFIER_assert(n->right == NULL && n->left == NULL);
				n = n->right;
			}
			else
			{
				__VERIFIER_assert(0);
			}
		}
	}

	n = NULL;
	struct TreeNode* pred;
	__VERIFIER_assert(root != NULL);

	while (root)
	{
		pred = NULL;
		n = root;
		while (n->num > 0)
		{
			pred = n;
			if (n->left)
			{
				n = n->left;
			}
			else
			{
				n = n->right;
			}
		}
		if (pred) {
			if (n == pred->left)
				pred->left = NULL;
			else
				pred->right = NULL;
			if (pred->num > 1)
				pred->num = 1;
			else
				pred->num = 0;
		} else
			root = NULL;
		free(n);
	}

	return 0;

}
