#include <stdlib.h>
#include <verifier-builtins.h>

typedef struct TSLL
{
	struct TSLL* next;
	struct TSLL* prev;
	int data;
} SLL;


typedef struct TBCK
{
	struct TBCK* next;
	SLL* list;
	int data;
} BCK;

int main()
{
	// create the head
	BCK* bucket = malloc(sizeof(BCK));
	bucket->data = 0;
	bucket->list = NULL;
	
	bucket->next = malloc(sizeof(BCK));
	BCK* bcki = bucket->next;
	bcki->data = 1;
	bcki->list = NULL;
	
	bcki->next = malloc(sizeof(BCK));
	bcki = bcki->next;
	bcki->data = 2;
	bcki->list = NULL;
	bcki->next = NULL;

	struct TSLL* item = NULL;
	struct TSLL* itr = NULL;
	while (__VERIFIER_nondet_int())
	{
		item = malloc(sizeof(SLL));
		item->next = NULL;
		item->prev = NULL;
		if (__VERIFIER_nondet_int())
			item->data = 0;
		else if (__VERIFIER_nondet_int())
			item->data = 1;
		else
			item->data = 2;

		bcki = bucket;

		__VERIFIER_assert(bcki != NULL);
		__VERIFIER_assert(item != NULL);
		while (bcki->data != item->data)
			bcki = bcki->next;
		__VERIFIER_assert(bcki != NULL);

		if (bcki->list == NULL)
			bcki->list = item;
		else
		{
			itr = bcki->list;
			while (itr->next != NULL)
				itr = itr->next;
			itr->next = item;
			item->prev = itr;
		}
	}

	/*
	bcki = bucket;
	while(bcki != NULL)
	{
		item = bcki->list;
		while(item != NULL)
		{
			__VERIFIER_assert(item->data == bcki->data);
			item = item->next;
		}
		__VERIFIER_assert(item == NULL);
		bcki = bcki->next;
	}
	*/

	bcki = bucket;
	while(bcki != NULL)
	{
		item = bcki->list;
		bcki->list = NULL;
		while(item != NULL)
		{
			__VERIFIER_assert(item->data == bcki->data);
			itr = item;
			item = item->next;
			free(itr);
		}
		__VERIFIER_assert(item == NULL);
		bucket = bcki;
		bcki = bcki->next;
		free(bucket);
		bucket = NULL;
	}

	return 0;
}
