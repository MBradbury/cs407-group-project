#include "array-list.h"
#include "linked-list.h"
#include "unique-array.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#ifndef _MSC_VER
#	include <stdbool.h>
#else
#	define bool int
#	define true 1
#	define false 0
#endif

static char const * linked_list_test(void)
{
	linked_list_t list;

	if (!linked_list_init(&list, NULL)) return "Failed: Init";

	int values[] = { 1, 1, 2, 3, 4, 5, 9, 2, 3 };

	unsigned int i;
	for (i = 0; i != 9; ++i)
	{
		if (linked_list_length(&list) != i) return "Failed: Append Length";

		if (!linked_list_append(&list, &values[i])) return "Failed: Append";
	}

	int expected[] = { 1, 1, 2, 3, 4, 5, 9, 2, 3 };

	if (linked_list_length(&list) != 9) return "Failed: Length";

	i = 0;

	linked_list_elem_t elem;
	for (elem = linked_list_first(&list); linked_list_continue(&list, elem); elem = linked_list_next(elem))
	{
		void * data = linked_list_data(&list, elem);

		if (data == NULL) return "Failed: Data Non-NULL";

		int value = *(int *)data;

		if (value != expected[i]) return "Failed: Data Expected";

		++i;
	}

	if (!linked_list_clear(&list)) return "Failed: Clear";

	return "Succeeded";
}

static char const * array_list_test(void)
{
	array_list_t list;

	if (!array_list_init(&list, NULL)) return "Failed: Init";

	int values[] = { 1, 1, 2, 3, 4, 5, 9, 2, 3 };

	unsigned int i;
	for (i = 0; i != 9; ++i)
	{
		if (array_list_length(&list) != i) return "Failed: Append Length";

		if (!array_list_append(&list, &values[i])) return "Failed: Append";
	}

	int expected[] = { 1, 1, 2, 3, 4, 5, 9, 2, 3 };

	if (array_list_length(&list) != 9) return "Failed: Length";

	i = 0;

	array_list_elem_t elem;
	for (elem = array_list_first(&list); array_list_continue(&list, elem); elem = array_list_next(elem))
	{
		void * data = array_list_data(&list, elem);

		if (data == NULL) return "Failed: Data Non-NULL";

		int value = *(int *)data;

		if (value != expected[i]) return "Failed: Data not as Expected 1";

		++i;
	}

	if (i != 9) return "Failed: Length Check";


	// Test reallocation
	int values2[] = { 7, 8, 5, 8, 9, 4, 5, 3, 5, 4, 3, 2, 1, 6, 8, 9 };
	for (i = 0; i != 16; ++i)
	{
		if (array_list_length(&list) != i + 9) return "Failed: Realloc Append Length";

		if (!array_list_append(&list, &values2[i])) return "Failed: Realloc Append";
	}

	int expected3[] = { 1, 1, 2, 3, 4, 5, 9, 2, 3, 7, 8, 5, 8, 9, 4, 5, 3, 5, 4, 3, 2, 1, 6, 8, 9 };

	i = 0;
	for (elem = array_list_first(&list); array_list_continue(&list, elem); elem = array_list_next(elem))
	{
		void * data = array_list_data(&list, elem);

		if (data == NULL) return "Failed: Data Non-NULL";

		int value = *(int *)data;

		if (value != expected3[i]) return "Failed: Data not as Expected 3";

		++i;
	}

	if (i != sizeof(expected3) / sizeof(*expected3)) return "Failed: Length Check 3";


	if (!array_list_remove(&list, 0)) return "Failed: Remove0";
	if (!array_list_remove(&list, 7)) return "Failed: Remove1";

	int expected2[] = { 1, 2, 3, 4, 5, 9, 2, 7, 8, 5, 8, 9, 4, 5, 3, 5, 4, 3, 2, 1, 6, 8, 9 };

	i = 0;
	for (elem = array_list_first(&list); array_list_continue(&list, elem); elem = array_list_next(elem))
	{
		void * data = array_list_data(&list, elem);

		if (data == NULL) return "Failed: Data Non-NULL";

		int value = *(int *)data;

		//printf("%d: (%d, %d)\n", i, value, expected2[i]);

		if (value != expected2[i]) return "Failed: Data not as Expected 2";

		++i;
	}

	if (i != sizeof(expected2) / sizeof(*expected2)) return "Failed: Length Check 2";
	

	if (!array_list_clear(&list)) return "Failed: Clear";

	return "Succeeded";
}


static bool compare_int(void const * left, void const * right)
{
	if (left == NULL || right == NULL)
		return false;

	int l = *(int const *)left;
	int r = *(int const *)right;

	return l == r;
}

static char const * unique_array_test(void)
{
	unique_array_t list;

	if (!unique_array_init(&list, &compare_int, NULL)) return "Failed: Init";

	int values[] = { 1, 1, 2, 3, 4, 5, 9, 2, 3 };

	unsigned int i;
	for (i = 0; i != 9; ++i)
	{
		if (!unique_array_append(&list, &values[i])) return "Failed: Append";
	}

	int expected[] = { 1, 2, 3, 4, 5, 9 };
	i = 0;
	for (i = 0; i != 6; ++i)
	{
		if (!unique_array_contains(&list, &expected[i])) return "Failed: Contains";
	}

	if (unique_array_length(&list) != 6) return "Failed: Length";

	i = 0;

	unique_array_elem_t elem;
	for (elem = unique_array_first(&list); unique_array_continue(&list, elem); elem = unique_array_next(elem))
	{
		void * data = unique_array_data(&list, elem);

		if (data == NULL) return "Failed: Data Non-NULL";

		int value = *(int *)data;

		if (value != expected[i]) return "Failed: Data Expected";

		++i;
	}

	if (!unique_array_clear(&list)) return "Failed: Clear";

	return "Succeeded";
}

int main(int argc, char ** argv)
{
	printf("Linked List Test: %s\n", linked_list_test());

	printf("Array List Test: %s\n", array_list_test());

	printf("Unique Array Test: %s\n", unique_array_test());

	return 0;
}

