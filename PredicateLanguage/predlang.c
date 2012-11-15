#include "predlang.h"

#include <stdio.h>
#include <stdbool.h>


#define MAXIMUM_FUNCTIONS 5
#define MAXIMUM_VARIABLES 2



static unsigned char stack[3 * 1024];

static unsigned char * stack_ptr = NULL;
static unsigned char * heap_ptr = NULL;


// Function that gets data on this node
static node_data_fn data_fn = NULL;
static size_t data_size = 0;


/****************************************************
 ** ERROR MANAGEMENT START
 ***************************************************/

static char const * error;

char const * error_message(void)
{
	return error;
}

void reset_error(void)
{
	error = NULL;
}

/****************************************************
 ** ERROR MANAGEMENT START
 ***************************************************/



/****************************************************
 ** MEMORY MANAGEMENT
 ***************************************************/
static void * heap_alloc(size_t size)
{
	if (heap_ptr - size < stack_ptr)
	{
		error = "Heap overwriting stack";
		return NULL;
	}

	heap_ptr -= size;

	return heap_ptr;
}
/****************************************************
 ** MEMORY MANAGEMENT
 ***************************************************/


/****************************************************
 ** VARIABLE MANAGEMENT START
 ***************************************************/

typedef struct
{
	char * name;
	void * location;

	unsigned int type : 2;
	unsigned int is_array : 1;
	unsigned int length : 13;

} variable_reg_t;


static size_t variable_type_size(variable_type_t type)
{
	switch (type)
	{
	case TYPE_INTEGER: return sizeof(int);
	case TYPE_FLOATING: return sizeof(float);
	case TYPE_USER: return data_size;
	default: return 0;
	}
}


static variable_reg_t * variable_regs = NULL;
static size_t variable_regs_count = 0;

static variable_reg_t * create_variable(char const * name, size_t name_length, variable_type_t type)
{
	if (variable_regs_count == MAXIMUM_VARIABLES)
	{
		error = "Created maximum number of variables";
		return NULL;
	}

	variable_reg_t * variable = &variable_regs[variable_regs_count];

	variable->name = (char *)heap_alloc(name_length + 1);
	strncpy(variable->name, name, name_length);

	// Lets create some space in the heap to store the variable
	variable->location = heap_alloc(variable_type_size(type));
	memset(variable->location, 0, variable_type_size(type));

	variable->type = type;

	return variable;
}

static variable_reg_t * create_array(char const * name, size_t name_length, variable_type_t type, size_t length)
{
	if (variable_regs_count == MAXIMUM_VARIABLES)
	{
		error = "Created maximum number of variables";
		return NULL;
	}

	variable_reg_t * variable = &variable_regs[variable_regs_count];

	variable->name = (char *)heap_alloc(name_length + 1);
	strncpy(variable->name, name, name_length);

	// Lets create some space in the heap to store the variable
	// We allocate enough space of `length' `data_size'ed items
	// So we can store `length' user data items
	variable->location = heap_alloc(variable_type_size(type) * length);
	memset(variable->location, 0, variable_type_size(type) * length);

	variable->type = type;
	variable->is_array = true;

	return variable;
}

static variable_reg_t * get_variable(char const * name)
{
	int i;
	for (i = 0; i != MAXIMUM_VARIABLES; ++i)
	{
		variable_reg_t * variable = &variable_regs[i];

		if (strcmp(name, variable->name) == 0)
		{
			return variable;
		}
	}

	return NULL;
}

static int * get_variable_as_int(char const * name)
{
	return (int *)get_variable(name)->location;
}

static float * get_variable_as_float(char const * name)
{
	return (float *)get_variable(name)->location;
}

/****************************************************
 ** VARIABLE MANAGEMENT END
 ***************************************************/




/****************************************************
 ** FUNCTION MANAGEMENT START
 ***************************************************/

typedef struct
{
	char const * name;
	data_access_fn fn;
	uint8_t type;
} function_reg_t;

static function_reg_t * functions_regs = NULL;
static size_t function_regs_count = 0;

int register_function(char const * name, data_access_fn fn, variable_type_t type)
{
	if (function_regs_count == MAXIMUM_FUNCTIONS)
	{
		error = "Already registered maximum number of functions";
		return 1;
	}

	functions_regs[function_regs_count].name = name;
	functions_regs[function_regs_count].fn = fn;
	functions_regs[function_regs_count].type = type;

	// Record that we have another function
	++function_regs_count;

	return 0;
}

static void const * call_function(char const * name, void * data)
{
	size_t i = 0;
	for (; i != function_regs_count; ++i)
	{
		if (strcmp(functions_regs[i].name, name) == 0)
		{
			return functions_regs[i].fn(data);
		}
	}

	error = "Unknown function name";

	return NULL;
}


/****************************************************
 ** FUNCTION MANAGEMENT END
 ***************************************************/




/****************************************************
 ** PARSING START
 ***************************************************/

/****************************************************
 ** PARSING END
 ***************************************************/



/****************************************************
 ** VM START
 ***************************************************/
typedef enum {
  HALT,
  PUSH, POP,
  FETCH, STORE,
  NEXT,
  ADD, SUB, MUL, DIV, MOD,
  EQ, NEQ, LT, LEQ, GT, GEQ,
  AND, OR, XOR, NOT,
} opcode;

static const char * opcode_names[] = {
	"HALT", // Stop evaluation
	"PUSH", "POP", // Put variables onto the staccall_functionk
	"FETCH", "STORE", // Read / Write variables
	"NEXT", // List Iteration
	"ADD", "SUB", "MUL", "DIV", "MOD", // Aritmetic operations
	"EQ", "NEQ", "LT", "LEQ", "GT", "GEQ", // Comparison Operations
	"AND", "OR", "XOR", "NOT" // Logic operations
};


/****************************************************
 ** VM END
 ***************************************************/




/****************************************************
 ** INIT MANAGEMENT END
 ***************************************************/

void init_pred_lang(node_data_fn given_data_fn, size_t given_data_size)
{
	// Record the user's data access function
	data_fn = given_data_fn;
	data_size = given_data_size;

	// Reset the stack and heap positions
	stack_ptr = stack;
	heap_ptr = &stack[sizeof(stack)];


	// Allocate some space for function registrations
	functions_regs = (function_reg_t *)heap_alloc(sizeof(function_reg_t) * MAXIMUM_FUNCTIONS);

	// Allocate space for variable registrations
	variable_regs = (variable_reg_t *)heap_alloc(sizeof(variable_reg_t) * MAXIMUM_VARIABLES);
}


/****************************************************
 ** INIT MANAGEMENT END
 ***************************************************/





/****************************************************
 ** USER CODE FROM HERE ON
 ***************************************************/

typedef struct
{
	int id;
	int slot;
	double temp;
} user_data_t;



static void * local_node_data_fn(void)
{
	static user_data_t node_data;

	node_data.id = 1;
	node_data.slot = 2;
	node_data.temp = 20.0;

	return &node_data;
}


static void const * get_id_fn(void const * ptr)
{
	return &((user_data_t const *)ptr)->id;
}

static void const * get_slot_fn(void const * ptr)
{
	return &((user_data_t const *)ptr)->slot;
}

static void const * get_temp_fn(void const * ptr)
{
	return &((user_data_t const *)ptr)->temp;
}


int main(int argc, char ** argv)
{
	init_pred_lang(&local_node_data_fn, sizeof(user_data_t));

	// Register the data functions 
	register_function("id", &get_id_fn, TYPE_INTEGER);
	register_function("slot", &get_slot_fn, TYPE_INTEGER);
	register_function("temp", &get_temp_fn, TYPE_FLOATING);

	variable_reg_t * result = create_variable("result", strlen("result"), TYPE_INTEGER);

	*((int *)result->location) = *(int const *)call_function("slot", (*data_fn)());

	printf("sizeof(void *): %u\n", sizeof(void *));
	printf("sizeof(int): %u\n", sizeof(int));
	printf("sizeof(float): %u\n", sizeof(float));
	printf("sizeof(variable_reg_t): %u\n", sizeof(variable_reg_t));
	printf("sizeof(function_reg_t): %u\n", sizeof(function_reg_t));

	printf("Data %d should equal %d\n",
		*get_variable_as_int("result"),
		((user_data_t const *) (*data_fn)())->slot
	);


	return 0;
}

