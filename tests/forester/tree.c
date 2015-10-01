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
	Tree** tr;
	int isInLeft = 0;

	while (__VERIFIER_nondet_int())
	{	// create arbitrary tree
		tr = &tree;

		while (NULL != *tr)
		{	// find any leaf
			if (__VERIFIER_nondet_int())
			{
				tr = &(*tr)->left;
				isInLeft = 1;
			}
			else
			{
				tr = &(*tr)->right;
			}
		}

		*tr = malloc(sizeof(**tr));
		(*tr)->left = NULL;
		if (isInLeft)
		{
			(*tr)->right = malloc(sizeof(**tr));
			(*tr)->right->left = NULL;
			(*tr)->right->right = NULL;
		}
		else
			(*tr)->right = NULL;
		isInLeft = 0;
	}

	while (NULL != tree)
	{	// while there are still some remains of the tree
		tr = &tree;

		while ((NULL != (*tr)->left) || (NULL != (*tr)->right))
		{
			if (NULL != (*tr)->left)
			{
				tr = &(*tr)->left;
			}
			else
			{
				tr = &(*tr)->right;
			}
		}

		free(*tr);
		*tr = NULL;
	}

	return 0;
}

