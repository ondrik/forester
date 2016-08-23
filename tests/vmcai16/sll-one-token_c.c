#include <stdlib.h>
#include <verifier-builtins.h>

#define WHITE 0
#define BLUE 1

typedef struct TSLL
{
	struct TSLL* next;
	int data;
} SLL;

int main()
{
	// create the head
	SLL* head = malloc(sizeof(SLL));
	head->next = NULL;
	head->data = WHITE;

	SLL* x = head;
	int marked = 0;

	// create an arbitrarily long tail
	while (__VERIFIER_nondet_int() || !marked)
	{
		// create a node
		x->next = malloc(sizeof(SLL));
		x = x->next;
		x->data = WHITE;
		x->next = NULL;

		if (__VERIFIER_nondet_int() && !marked)
		{
			x->data = BLUE;
			marked = 1;
		}
		__VERIFIER_assert(NULL != x);
	}

	x = head;
	// check the invariant
	__VERIFIER_assert(NULL != x);
	marked = 0;

	while (x != NULL)
	{
		__VERIFIER_assert(x->data == WHITE || marked == 0);
		if (x->data == BLUE)
		{
			__VERIFIER_assert(0 == marked);
			marked = 1;
		}

		x = x->next;
	}

	x = head;
	// destroy the list
	while (x != NULL)
	{
		head = x;
		x = x->next;
		free(head);
	}

	return 0;
}
