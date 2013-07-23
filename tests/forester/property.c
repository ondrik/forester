#include <stdio.h>
#include <stdlib.h>

struct list_head {
       struct list_head *next, *prev;
};

//Macro provide in linux/stddef.h
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

struct MyProperty
{
	int n;
	struct list_head node;
};

typedef struct MyProperty Property;

///////////////////////////////////////////////////////////////////////////////
int main()
{
	Property * property = malloc(sizeof(Property));
	property->n = 1;
	property->node.next = &property->node;
	property->node.prev = &property->node;

	Property * propertyA = malloc(sizeof(Property));
	propertyA->n = 2;
	propertyA->node.next = &property->node;
	propertyA->node.prev = &property->node;
	property->node.next = &propertyA->node;
	property->node.prev = &propertyA->node;

	Property* prop;
	prop = container_of(&property->node, struct MyProperty, node);
	printf("Value = %d \n",prop->n);


	//Free all node
	struct list_head *pos,*n;
	struct list_head *head = &prop->node;
	list_for_each_safe(pos, n, head)
	{
		Property * propertyFree = container_of(pos, struct MyProperty, node);
		free(propertyFree);
		printf("Free %p \n",propertyFree);
	}
	free(prop);

	return 0;
}

