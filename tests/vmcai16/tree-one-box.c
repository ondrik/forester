/*
 * Tree constructor
 *
 * boxes:
 */

#include <stdlib.h>
#include <verifier-builtins.h>

#define WHITE 0
#define BLUE 1

int main() {

	struct TreeNode {
		struct TreeNode* left;
		struct TreeNode* right;
		struct TreeNode* parent;
		int colour;
	};

	struct TreeNode* root = malloc(sizeof(struct TreeNode));
	struct TreeNode* n = root;
	root->left = NULL;
	root->right = NULL;
	root->colour = WHITE;

	int inserted = 0;

	while (__VERIFIER_nondet_int() || inserted == 0)
	{
		n = root;
		__VERIFIER_assert(root != NULL);
		__VERIFIER_assert(n != NULL);

		while (n->left != NULL || n->right != NULL)
		{
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

		struct TreeNode* tmp = NULL;
		if (__VERIFIER_nondet_int())
		{
			n->left = malloc(sizeof(*n));
			tmp = n->left;
		}
		else
		{
			n->right = malloc(sizeof(*n));
			tmp = n->right;
		}
		tmp->left = NULL;
		tmp->right = NULL;
		tmp->parent = n;
		if (inserted == 0 && __VERIFIER_nondet_int())
		{
			tmp->colour = BLUE;
			inserted = 1;
		}
		else
		{
			tmp->colour = WHITE;
		}

		/*
		if (__VERIFIER_nondet_int())
		{
			n->left = malloc(sizeof(*n));
			n->left->left = NULL;
			n->left->right = NULL;
		}
		else
		{
			n->right = malloc(sizeof(*n));
			n->right->left = NULL;
			n->right->right = NULL;
		}
		if (inserted == 0 && __VERIFIER_nondet_int())
		{
			if (n->left != NULL)
			{
				n->left->colour = BLUE;
			}
			else if (n->right != NULL)
			{
				n->right->colour = BLUE;
			}
			inserted = 1;
		}
		else
		{
			if (n->left != NULL)
			{
				n->left->colour = WHITE;
			}
			else if (n->right != NULL)
			{
				n->right->colour = WHITE;
			}
		}
		*/
	}

	while (__VERIFIER_nondet_int())
	{	// go throught the tree
		int count = 0;
		n = root;
		__VERIFIER_assert(n != NULL);
		while (n != NULL)
		{
			__VERIFIER_assert(n != NULL);
			__VERIFIER_assert(root != NULL);

			__VERIFIER_assert(n->colour != BLUE || count == 0);
			if (n->colour == BLUE)
			{
				count = 1;
			}

			if (n->left != NULL && __VERIFIER_nondet_int())
			{
				n = n->left;
			}
			else if (n->right != NULL)
			{
				n = n->right;
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
		while (n->left != NULL || n->right != NULL)
		{
			pred = n;
			if (n->left != NULL && __VERIFIER_nondet_int())
			{
				n = n->left;
			}
			else if (n->right != NULL)
			{
				n = n->right;
			}
			else if (n->left != NULL)
			{
				n = n->left;
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
