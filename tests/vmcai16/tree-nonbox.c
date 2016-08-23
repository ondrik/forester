/**
 * A tree with root pointers
 */

#include <stdlib.h>
#include <verifier-builtins.h>

typedef struct TTree
{
	struct TTree* left;
	struct TTree* right;
} Tree;

int main()
{
	Tree* tree = NULL;
	Tree** tr = NULL;

	while (__VERIFIER_nondet_int())
	{	// create arbitrary tree
		tr = &tree;

		while (NULL != *tr)
		{	// find any leaf
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
				}
				tr = &(*tr)->right;
			}
		}

		*tr = malloc(sizeof(**tr));
		(*tr)->right = NULL;
		(*tr)->left = NULL;
		// (*tr)->left = malloc(sizeof(**tr));
		// (*tr)->left->left = NULL;
		// (*tr)->left->right = NULL;
	}


	while (NULL != tree)
	{	// while there are still some remains of the tree
		tr = &tree;
		while ((*tr)->left != NULL || (*tr)->right != NULL)
		{
			__VERIFIER_assert((*tr)->left != NULL || ((*tr)->left == NULL && (*tr)->right == NULL));
			__VERIFIER_assert((*tr) != NULL);

			if (NULL != (*tr)->right)
			{
				tr = &((*tr)->right);
			}
			else
			{
				// Tree* t = (*tr)->left->left;
				__VERIFIER_assert((*tr)->left != NULL || (*tr)->right == NULL);
				tr = &((*tr)->left);
			}
		}

		free(*tr);
		*tr = NULL;
	}

	return 0;
}

