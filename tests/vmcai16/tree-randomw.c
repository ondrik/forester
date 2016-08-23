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
	Tree* parent = NULL;
	Tree** tr = NULL;

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
		(*tr)->left = NULL;
		(*tr)->parent = parent;
		// (*tr)->left = malloc(sizeof(**tr));
		// (*tr)->left->left = NULL;
		// (*tr)->left->right = NULL;
	}
	
	tr = &tree;
	while (__VERIFIER_nondet_int() && *tr != NULL)
	{	// go throught the tree
		tr = &tree;
		while ((*tr)->left != NULL || (*tr)->right != NULL)
		{
			__VERIFIER_assert((*tr)->left != NULL || ((*tr)->left == NULL && (*tr)->right == NULL));
			__VERIFIER_assert((*tr) != NULL);

			if (NULL != (*tr)->left && __VERIFIER_nondet_int())
			{
				tr = &((*tr)->left);
			}
			else if (NULL != (*tr)->right)
			{
				// Tree* t = (*tr)->left->left;
				__VERIFIER_assert((*tr)->left != NULL || (*tr)->right == NULL);
				tr = &((*tr)->right);
			}
		}
	}

	while (NULL != tree)
	{	// while there are still some remains of the tree
		tr = &tree;
		while ((*tr)->left != NULL || (*tr)->right != NULL)
		{
			if (NULL != (*tr)->left)
			{
				tr = &((*tr)->left);
			}
			else
			{
				tr = &((*tr)->right);
			}
		}

		free(*tr);
		*tr = NULL;
	}

	return 0;
}
