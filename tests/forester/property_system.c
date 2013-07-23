#include <stdio.h>
#include <stdlib.h>
#include "linux_list.h"

struct MyProperty
{
	int n;
	struct list_head node;
};

typedef struct MyProperty Property;

Property * getProperty(struct list_head *pos)
{
	return list_entry(pos, struct MyProperty, node);
}

///////////////////////////////////////////////////////////////////////////////
Property * initProperty(int value)
{
	Property * property = malloc(sizeof(Property));
	property->n = value;
	INIT_LIST_HEAD(&property->node);
	return property;
}

///////////////////////////////////////////////////////////////////////////////
void finalizeProperty(Property *list)
{
	struct list_head *pos,*n;
	struct list_head *head = &list->node;
	list_for_each_safe(pos, n, head)
	{
		Property *property = getProperty(pos);
		free(property);
		printf("Free %p \n",property);
	}
	free(list);
}

///////////////////////////////////////////////////////////////////////////////
void finaliseProperty(Property *list)
{
	while(!list_empty(&list->node))
	{
		struct list_head *pos = list->node.next;
		Property *property = getProperty(pos);
		deleteProperty(property);
		printf("Free %p \n",property);
	}
	free(list);
}

///////////////////////////////////////////////////////////////////////////////
///Return 0 if list is empty
int deleteProperty(Property *property)
{
	if(list_empty(&property->node))
	{
		free(property);
		return 0;
	}

	list_del(&property->node);
	free(property);
	return 1;
}

///////////////////////////////////////////////////////////////////////////////
Property * addProperty(Property *list,int value)
{
	Property * property = malloc(sizeof(Property));
	property->n = value;
	list_add_tail(&property->node,&list->node);
	printf("Create %p \n",property);
	return property;
}

///////////////////////////////////////////////////////////////////////////////
void DisplayProperty(Property *list)
{
	if(list==NULL)
	{
		printf("List Empty \n");
		return;
	}
	printf("Begin:%d",list->n);
	struct list_head *pos;
	struct list_head *head = &list->node;
	list_for_each(pos, head)
	{
		Property *property = getProperty(pos);
		printf("->%d",property->n);
	}
	printf("\n");
}

///////////////////////////////////////////////////////////////////////////////
int main()
{
	Property * list = initProperty(1);
	Property * node2 = addProperty(list,2);
	Property * node3 = addProperty(list,3);

	DisplayProperty(list);
	DisplayProperty(node2);
	finalizeProperty(list);
	//finaliseProperty(list);
	return 0;
}

