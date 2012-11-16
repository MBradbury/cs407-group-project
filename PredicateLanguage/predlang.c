#include "predlang.h"

#include <stdio.h>

#ifndef _MSC_VER
#	include <stdbool.h>
#endif

#define STACK_SIZE (3 * 1024)

#define MAXIMUM_FUNCTIONS 5
#define MAXIMUM_VARIABLES 5



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


static void push_stack(void * ptr, size_t size)
{
	stack_ptr -= size;
	memcpy(stack_ptr, ptr, size);
}

static void int_push_stack(int i)
{
	stack_ptr -= sizeof(int);
	*((int *)stack_ptr) = i;
}

static void float_push_stack(float i)
{
	stack_ptr -= sizeof(float);
	*((float *)stack_ptr) = i;
}

static void pop_stack(size_t size)
{
	stack_ptr += size;
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


static size_t variable_type_size(unsigned int type)
{
	switch (type)
	{
	case TYPE_INTEGER: return sizeof(int);
	case TYPE_FLOATING: return sizeof(float);
	case TYPE_USER: return data_size;
	default: 
		error = "Unknown variable type";
		return 0;
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
	variable->length = length;

	printf("Registered array with name '%s' and length %d and elem size %d\n",
		variable->name, variable->length, variable_type_size(type));

	return variable;
}

static variable_reg_t * get_variable(char const * name)
{
	size_t i = 0;
	for (; i != variable_regs_count; ++i)
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
 ** CODE GEN
 ***************************************************/

typedef int jmp_loc_t;
typedef jmp_loc_t * jmp_loc_ptr_t;
typedef unsigned char * jmp_label_t;

static unsigned char * program_start;
static unsigned char * program_end;

static unsigned char * start_gen(void)
{
	return program_start = heap_ptr;
}

static jmp_label_t gen_op(unsigned char op)
{
	unsigned char * pos = heap_ptr;

	*heap_ptr = op;
	++heap_ptr;

	return pos;
}

static void gen_int(int i)
{
	*(int *)heap_ptr = i;
	heap_ptr += sizeof(int);
}

static void gen_float(float f)
{
	*(float *)heap_ptr = f;
	heap_ptr += sizeof(float);
}

static void gen_string(char const * str)
{
	size_t len = strlen(str) + 1;

	memcpy(heap_ptr, str, len);
	heap_ptr += len;
}

static jmp_loc_ptr_t gen_jmp(void)
{
	jmp_loc_ptr_t pos = (jmp_loc_ptr_t)heap_ptr;

	*pos = 0;

	heap_ptr += sizeof(jmp_loc_t);

	return pos;
}

static void alloc_jmp(jmp_loc_ptr_t jmp, jmp_label_t label)
{
	*jmp = label - program_start;
}

static unsigned char * stop_gen(void)
{
	return program_end = heap_ptr;
}

/****************************************************
 ** CODE GEN
 ***************************************************/



/****************************************************
 ** VM START
 ***************************************************/
typedef enum {
  HALT,

  IPUSH, IPOP, FPUSH, FPOP,
  IFETCH, ISTORE, FFETCH, FSTORE,
  
  AFETCH, ALEN,

  ICASTF, FCASTI,

  JMP, JZ, JNZ,

  IADD, ISUB, IMUL, IDIV, IINC,
  IEQ, INEQ, ILT, ILEQ, IGT, IGEQ,

  FADD, FSUB, FMUL, FDIV,
  FEQ, FNEQ, FLT, FLEQ, FGT, FGEQ,

  AND, OR, XOR, NOT,
} opcode;

static const char * opcode_names[] = {
	"HALT", // Stop evaluation

	"IPUSH", "IPOP", "FPUSH", "FPOP", // Put variables onto the stack
	"IFETCH", "ISTORE", "FFETCH", "FSTORE", // Read / Write variables

	"AFETCH", "ALEN", // Arrays Ops

	"ICASTF", "FCASTI", // Casting operations

	"JMP", "JZ", "JNZ", // Jump operations

	"IADD", "ISUB", "IMUL", "IDIV", "IINC", // Aritmetic operations
	"IEQ", "INEQ", "FLT", "FLEQ", "FGT", "FGEQ", // Comparison Operations

	"FADD", "FSUB", "IMUL", "IDIV", // Aritmetic operations
	"FEQ", "FNEQ", "FLT", "FLEQ", "FGT", "FGEQ", // Comparison Operations

	"AND", "OR", "XOR", "NOT", // Logic operations
};



#define OPERATION_POP(code, op, type, store_type, format_type) \
	case code: \
		printf("Calling %s on " format_type " and " format_type "\n", opcode_names[*current], ((type *)stack_ptr)[0], ((type *)stack_ptr)[1]); \
		((store_type *)stack_ptr)[1] = ((type *)stack_ptr)[0] op ((type *)stack_ptr)[1]; stack_ptr += sizeof(type); \
		break

static void evaluate(unsigned char * start, size_t program_length)
{
	unsigned char * current = start;

	while (current - start < program_length)
	{
		printf("Executing (%d) %s\n", *current, opcode_names[*current]);

		// Ideally want this op codes in numerical order
		// so the compiler can generate a jump table
		switch (*current)
		{
		case HALT:
			printf("Halting\n");
			return;

		case IPUSH:
			printf("Pushing int %d onto the stack\n", *(int*)(current + 1));
			int_push_stack(*(int*)(current + 1));
			current += sizeof(int);
			break;

		case IPOP:
			pop_stack(sizeof(int));
			break;

		case FPUSH:
			printf("Pushing float %f onto the stack\n", *(float*)(current + 1));
			float_push_stack(*(float*)(current + 1));
			current += sizeof(float);
			break;

		case FPOP:
			pop_stack(sizeof(float));
			break;

		case IFETCH:
			int_push_stack(*get_variable_as_int((char const *)(current + 1)));
			current += strlen((char const *)(current + 1)) + 1;
			break;

		case ISTORE:
			*get_variable_as_int((char const *)(current + 1)) = *(int *)stack_ptr;
			current += strlen((char const *)(current + 1)) + 1;
			pop_stack(sizeof(int));
			break;

		case FFETCH:
			float_push_stack(*get_variable_as_float((char const *)(current + 1)));
			current += strlen((char const *)(current + 1)) + 1;
			break;

		case FSTORE:
			*get_variable_as_float((char const *)(current + 1)) = *(float *)stack_ptr;
			current += strlen((char const *)(current + 1)) + 1;
			pop_stack(sizeof(float));
			break;

		case AFETCH:
			{
				variable_reg_t * var = get_variable((char const *)(current + 1));
				int index = ((int *)stack_ptr)[0];
				pop_stack(sizeof(int));
				push_stack((char *)var->location + (index * variable_type_size(var->type)), variable_type_size(var->type));
				current += strlen((char const *)(current + 1)) + 1;
			} break;

		case ALEN:
			{
				variable_reg_t * var = get_variable((char const *)(current + 1));
				int length = var->length;
				push_stack(&length, sizeof(int));
				current += strlen((char const *)(current + 1)) + 1;
			} break;

		case ICASTF:
			((float *)stack_ptr)[0] = (float)((int *)stack_ptr)[0];
			break;

		case FCASTI:
			((int *)stack_ptr)[0] = (int)((float *)stack_ptr)[0];
			break;

		case JMP:
			current = start + *(int *)(current + 1) - 1;
			break;

		case JZ:
			if (((int *)stack_ptr)[0] == 0)
			{
				current = start + *(int *)(current + 1) - 1;
				printf("Jumping to %p\n", current + 1);
			}
			else
			{
				current += sizeof(int);
			}

			pop_stack(sizeof(int));

			break;

		case JNZ:
			if (((int *)stack_ptr)[0] != 0)
			{
				current = start + *(int *)(current + 1) - 1;
				printf("Jumping to %p\n", current + 1);
			}
			else
			{
				current += sizeof(int);
			}

			pop_stack(sizeof(int));

			break;

		// Integer operations
		OPERATION_POP(IADD, +, int, int, "%d");
		OPERATION_POP(ISUB, -, int, int, "%d");
		OPERATION_POP(IMUL, *, int, int, "%d");
		OPERATION_POP(IDIV, /, int, int, "%d");

		case IINC:
			printf("Incrementing %d\n", ((int *)stack_ptr)[0]);
			((int *)stack_ptr)[0] += 1;
			break;

		OPERATION_POP(IEQ, ==, int, int, "%d");
		OPERATION_POP(INEQ, !=, int, int, "%d");
		OPERATION_POP(ILT, <, int, int, "%d");
		OPERATION_POP(ILEQ, <=, int, int, "%d");
		OPERATION_POP(IGT, >, int, int, "%d");
		OPERATION_POP(IGEQ, >=, int, int, "%d");

		// Floating point operations
		OPERATION_POP(FADD, +, float, float, "%f");
		OPERATION_POP(FSUB, -, float, float, "%f");
		OPERATION_POP(FMUL, *, float, float, "%f");
		OPERATION_POP(FDIV, /, float, float, "%f");
		OPERATION_POP(FEQ, ==, float, int, "%f");
		OPERATION_POP(FNEQ, !=, float, int, "%f");
		OPERATION_POP(FLT, <, float, int, "%f");
		OPERATION_POP(FLEQ, <=, float, int, "%f");
		OPERATION_POP(FGT, >, float, int, "%f");
		OPERATION_POP(FGEQ, >=, float, int, "%f");

		// Logical operations
		OPERATION_POP(AND, &&, int, int, "%d");
		OPERATION_POP(OR, ||, int, int, "%d");
		OPERATION_POP(XOR, ^, int, int, "%d");

		case NOT:
			((int *)stack_ptr)[0] = ! ((int *)stack_ptr)[0];
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


int main(int argc, char * argv[])
{
	init_pred_lang(&local_node_data_fn, sizeof(user_data_t));

	// Register the data functions 
	register_function("id", &get_id_fn, TYPE_INTEGER);
	register_function("slot", &get_slot_fn, TYPE_INTEGER);
	register_function("temp", &get_temp_fn, TYPE_FLOATING);

	variable_reg_t * result = create_variable("result", strlen("result"), TYPE_INTEGER);

	*((int *)result->location) = *(int const *)call_function("slot", (*data_fn)());


	create_variable("i", strlen("i"), TYPE_INTEGER);
	variable_reg_t * var_array = create_array("1hopn", strlen("1hopn"), TYPE_INTEGER, 10);

	int * arr = (int *)var_array->location;
	arr[0] = 1;
	arr[1] = 3;
	arr[2] = 5;
	arr[3] = 9;
	arr[4] = 11;
	arr[5] = 13;
	arr[6] = 15;
	arr[7] = 17;
	arr[8] = 19;
	arr[9] = 21;

	printf("Array length %d\n", var_array->length);

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
	unsigned char * const program_start = start_gen();

	/*gen_op(IPUSH);
	gen_int(2);
	gen_op(IPUSH);
	gen_int(3);
	gen_op(IADD);
	gen_op(IFETCH);
	gen_string("result");
	gen_op(IADD);
	gen_op(ICASTF);
	gen_op(FPUSH);
	gen_float(7.5);
	gen_op(FGT);
	gen_op(NOT);
	gen_op(HALT);*/



	// Initalise loop counter
	gen_op(IPUSH);
	gen_int(0);

	gen_op(ISTORE);
	gen_string("i");


	// Perform loop termination check
	jmp_label_t label1 = 
	gen_op(IFETCH);
	gen_string("i");

	gen_op(ALEN);
	gen_string("1hopn");

	gen_op(INEQ);

	gen_op(JZ);
	jmp_loc_ptr_t jmp1 =
	gen_jmp();


	// Perform body operations
	gen_op(AFETCH);
	gen_string("1hopn");


	// Increment loop counter
	gen_op(IFETCH);
	gen_string("i");

	gen_op(IINC);

	gen_op(ISTORE);
	gen_string("i");


	// Jump to start of loop
	gen_op(JMP);
	jmp_loc_ptr_t jmp2 =
	gen_jmp();


	// Program termination
	unsigned char * end = gen_op(HALT);


	// Set jump locations
	alloc_jmp(jmp1, end);
	alloc_jmp(jmp2, label1);


	unsigned char * const program_end = stop_gen();


	printf("Program length %d\n", program_end - program_start);


	// Evaluate the program
	evaluate(program_start, program_end - program_start);

	// Print the results
	printf("Stack ptr value %d\n",
		*((int *)stack_ptr)
	);


	return 0;
}
