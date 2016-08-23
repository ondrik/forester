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
	};

	struct TreeNode* root = malloc(sizeof(struct TreeNode));
	struct TreeNode* n = root;
	root->left = NULL;
	root->right = NULL;

	while (__VERIFIER_nondet_int()) {
		n = root;
		__VERIFIER_assert(root != NULL);
		__VERIFIER_assert(n != NULL);

		while (n->left != NULL || n->right != NULL) {
			__VERIFIER_assert(n != NULL);
			__VERIFIER_assert(n->left != NULL || n->right != NULL);
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
			n->right = NULL;
		}
		else
		{
			n->left = malloc(sizeof(*n));
			n->left->left = NULL;
			n->left->right = NULL;
			n->right = malloc(sizeof(*n));
			n->right->left = NULL;
			n->right->right = NULL;
		}
	}

	while (__VERIFIER_nondet_int())
	{	// go throught the tree
		n = root;
		__VERIFIER_assert(n != NULL);
		while (n != NULL)
		{
			__VERIFIER_assert(n->left != NULL || (n->left == NULL && n->right == NULL));
			__VERIFIER_assert(n != NULL);
			__VERIFIER_assert(root != NULL);

			if (NULL != n->right)
			{
				if (__VERIFIER_nondet_int())
				{
					n = n->right;
				}
				else
				{
					n = n->left;
				}
			}
			else if (NULL != n->left)
			{
				n = n->left;
			}
			else
			{
				n = NULL;
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
		while (n->left || n->right)
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
		} else
			root = NULL;
		free(n);
	}

	return 0;

}
