#include "predlang.h"

#include <stdio.h>

#ifndef _MSC_VER
#	include <stdbool.h>
#endif

#define STACK_SIZE (3 * 1024)

#define MAXIMUM_FUNCTIONS 5
#define MAXIMUM_VARIABLES 2



static unsigned char stack[STACK_SIZE];

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
	if (heap_ptr + size > stack_ptr)
	{
		error = "Heap overwriting stack";
		return NULL;
	}

	void * ptr = heap_ptr;

	heap_ptr += size;

	return ptr;
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

	variable_reg_t * variable = &variable_regs[variable_regs_count++];

	variable->name = (char *)heap_alloc(name_length + 1);
	strncpy(variable->name, name, name_length);

	printf("Registered variable with name '%s'\n", variable->name);

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

	variable_reg_t * variable = &variable_regs[variable_regs_count++];

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
	for (i = 0; i != variable_regs_count; ++i)
	{
		variable_reg_t * variable = &variable_regs[i];

		if (strcmp(name, variable->name) == 0)
		{
			return variable;
		}
	}

	error = "No variable with the given name exists";

	return NULL;
}

static int * get_variable_as_int(char const * name)
{
	variable_reg_t * var = get_variable(name);

	return (var != NULL) ? (int *)var->location : NULL;
}

static float * get_variable_as_float(char const * name)
{
	variable_reg_t * var = get_variable(name);

	return (var != NULL) ? (float *)var->location : NULL;
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
	variable_type_t type;
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
  IPUSH, IPOP,
  FETCH, STORE,
  NEXT,

  IADD, ISUB, IMUL, IDIV,
  IEQ, INEQ, ILT, ILEQ, IGT, IGEQ,

  FADD, FSUB, FMUL, FDIV,
  FEQ, FNEQ, FLT, FLEQ, FGT, FGEQ,

  AND, OR, XOR, NOT,
} opcode;

static const char * opcode_names[] = {
	"HALT", // Stop evaluation
	"IPUSH", "IPOP", // Put variables onto the stack
	"FETCH", "STORE", // Read / Write variables
	"NEXT", // List Iteration

	"IADD", "ISUB", "IMUL", "IDIV", // Aritmetic operations
	"IEQ", "INEQ", "FLT", "FLEQ", "FGT", "FGEQ", // Comparison Operations

	"FADD", "FSUB", "IMUL", "IDIV", // Aritmetic operations
	"FEQ", "FNEQ", "FLT", "FLEQ", "FGT", "FGEQ", // Comparison Operations

	"AND", "OR", "XOR", "NOT", // Logic operations
};



#define OPERATION_POP(code, op, type) \
	case code: \
		printf("Calling %s on %d and %d\n", opcode_names[*current], ((type *)stack_ptr)[0], ((type *)stack_ptr)[1]); \
		((type *)stack_ptr)[1] = ((type *)stack_ptr)[0] op ((type *)stack_ptr)[1]; stack_ptr += sizeof(type); \
		break

static void evaluate(unsigned char * start, size_t program_length)
{
	unsigned char * current = start;

	while (current - start < program_length)
	{
		printf("Executing (%d) %s\n", *current, opcode_names[*current]);

		switch (*current)
		{
		case HALT:
			printf("Halting\n");
			return;

		case IPUSH:
			printf("Pushing %d onto the stack\n", *(int*)current);
			stack_ptr -= sizeof(int); *((int *)stack_ptr) = *(int*)current;
			current += sizeof(int);
			break;

		case IPOP:
			stack_ptr += sizeof(int);
			break;

		// Integer operations
		OPERATION_POP(IADD, +, int);
		OPERATION_POP(IMUL, *, int);
		OPERATION_POP(IDIV, /, int);
		OPERATION_POP(IEQ, ==, int);
		OPERATION_POP(INEQ, !=, int);
		OPERATION_POP(ILT, <, int);
		OPERATION_POP(ILEQ, <=, int);
		OPERATION_POP(IGT, >, int);
		OPERATION_POP(IGEQ, >=, int);

		// Floating point operations
		OPERATION_POP(FADD, +, float);
		OPERATION_POP(FMUL, *, float);
		OPERATION_POP(FDIV, /, float);
		OPERATION_POP(FEQ, ==, float);
		OPERATION_POP(FNEQ, !=, float);
		OPERATION_POP(FLT, <, float);
		OPERATION_POP(FLEQ, <=, float);
		OPERATION_POP(FGT, >, float);
		OPERATION_POP(FGEQ, >=, float);

		// Logical operations
		OPERATION_POP(AND, &&, int);
		OPERATION_POP(OR, ||, int);
		OPERATION_POP(XOR, ^, int);

		case NOT:
			((int *)stack_ptr)[0] = ((int *)stack_ptr)[0] ? 0 : 1;
			break;

		default:
			printf("Unknown OP CODE %d\n", *current);
			break;
		}

		++current;
	}
}


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
	stack_ptr = &stack[sizeof(stack)];
	heap_ptr = stack;


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

	int * data = get_variable_as_int("result");

	user_data_t const * user_data = (user_data_t const *) (*data_fn)();

	printf("Data %d should equal %d\n",
		*data,
		user_data->slot
	);

	// Load a small program into memory
	unsigned char * program_end = stack_ptr;

	stack_ptr -= 1; *stack_ptr = HALT;
	stack_ptr -= 1; *stack_ptr = IADD;
	stack_ptr -= sizeof(int); *((int *)stack_ptr) = 3;
	stack_ptr -= 1; *stack_ptr = IPUSH;
	stack_ptr -= sizeof(int); *((int *)stack_ptr) = 2;
	stack_ptr -= 1; *stack_ptr = IPUSH;

	// Evaluate the program
	evaluate(stack_ptr, program_end - stack_ptr);

	// Print the results
	printf("Stack ptr value %d\n",
		*((int *)stack_ptr)
	);


	return 0;
}
