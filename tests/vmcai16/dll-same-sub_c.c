/*
 */

#include <stdlib.h>
#include <verifier-builtins.h>

int main() {
	struct T {
		struct T* next;
		struct T* prev;
		struct T* data;
	};

	struct T* x = NULL;
	struct T* y = NULL;
	struct T* list = NULL;
	struct T* data = NULL;
	list = malloc(sizeof(struct T));
	list->next = x;
	list->data = NULL;
	list->prev = NULL;
	y = list;

	while (__VERIFIER_nondet_int()) {
		y->next = malloc(sizeof(struct T));
		y->next->prev = y;
		y = y->next;
		y->data = NULL;
		y->next = NULL;
	}

	x = list;
	while (x != NULL)
	{
		x->data = malloc(sizeof(struct T));
		x->data->prev = x->data;
		x->data->next = NULL;
		x->data->data = NULL;
		x = x->next;
	}

	x = list->data;
	while (__VERIFIER_nondet_int())
	{
		x->next = malloc(sizeof(struct T));
		x->prev = x;
		x = x->next;
		x->next = NULL;
		x->data = NULL;

		y = list->next;
		while (y != NULL)
		{
			data = y->data;
			__VERIFIER_assert(data != NULL);
			while (data->next != NULL)
			{
				data = data->next;
			}
			data->next = malloc(sizeof(struct T));
			data->next->prev = data;
			data = data->next;
			data->next = NULL;
			data->data = NULL;
			
			y->next;
		}

	}

	y = list->next;
	
	while(y != NULL)
	{
		x = list->data;
		data = y->data;
		while (x != NULL)
		{
			struct T* t = data;
			data = data->next;
			free(t);
			x = x->next;
		}
		y = y->next;
	}

	x = list->data;
	while (x != NULL)
	{
		y = x;
		x = x->next;
		free(y);
	}

	y = list;
	while (y != NULL)
	{
		x = y;
		y = y->next;
		free(x);
	}

	return 0;
}
