/**
 * A tree with root pointers
 */

#include <stdlib.h>
#include <verifier-builtins.h>

typedef struct TTree
{
	struct TTree* left;
	struct TTree* right;
	struct TTree* parent;
} Tree;

int main()
{
	Tree* tree = NULL;
	Tree* tmp = NULL;
	Tree* parent = NULL;
	Tree** tr;

	while (__VERIFIER_nondet_int())
	{	// create arbitrary tree
		tr = &tree;

		while (NULL != *tr)
		{	// find any leaf
			parent = *tr;
			if (__VERIFIER_nondet_int())
			{
				tr = &(*tr)->left;
			}
			else
			{
				if ((*tr)->left == NULL)
				{
					(*tr)->left = malloc(sizeof(**tr));
					(*tr)->left->left = NULL;
					(*tr)->left->right = NULL;
					(*tr)->left->parent = *tr;
				}
				tr = &(*tr)->right;
			}
		}

		*tr = malloc(sizeof(**tr));
		(*tr)->right = NULL;
		(*tr)->left = malloc(sizeof(**tr));
		(*tr)->left->left = NULL;
		(*tr)->left->parent = *tr;
		(*tr)->left->right = NULL;
		(*tr)->parent = parent;
		parent = NULL;
	}

	tr = NULL;

	while (NULL != tree)
	{	// while there are still some remains of the tree

		tmp = tree;
		while (tmp->left != NULL || tmp->right != NULL)
		{
			__VERIFIER_assert(tmp->left != NULL || (tmp->left == NULL && tmp->right == NULL));
			__VERIFIER_assert(tmp != NULL);
			if (NULL != tmp->left)
			{
				tmp = tmp->left;
			}
			else
			{
				// Tree* t = tmp->left->left;
				__VERIFIER_assert(tmp->left != NULL || tmp->right == NULL);
				tmp = tmp->right;
			}
		}

		free(tmp);
		tmp = NULL;
	}

	return 0;
}

