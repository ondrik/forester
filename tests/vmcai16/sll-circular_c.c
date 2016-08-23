#include <stdlib.h>
#include <verifier-builtins.h>

typedef struct TSLL
{
	struct TSLL* next;
	int data;
} SLL;

int main()
{
	// create the head
	SLL* head = malloc(sizeof(SLL));
	head->next = head;
	head->data = 0;

	SLL* x = head;
	int state = 1;

	// create an arbitrarily long tail
	while (__VERIFIER_nondet_int())
	{
		// create a node
		x->next = malloc(sizeof(SLL));
		x = x->next;
		x->next = head;

		if (__VERIFIER_nondet_int())
		{
			if (state == 1)
				state = 2;
			else if (state == 2)
				state = 3;
			else
				break;
		}

		x->data = state;

		__VERIFIER_assert(NULL != x);
	}
	x->data = state;

	x = head->next;
	__VERIFIER_assert(NULL != x);

	while (x->data != 0)
	{
		__VERIFIER_assert(x->next->data == 0 || x->data <= x->next->data);
		x = x->next;
	}

	x = head->next;
	// destroy the list
	while (x->data != 0)
	{
		head = x;
		x = x->next;
		free(head);
	}
	free(x);

	return 0;
}
