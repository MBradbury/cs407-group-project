#include "predlang.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifndef _MSC_VER
#	include <stdbool.h>
#else
#	define snprintf _snprintf
#endif

#ifndef NDEBUG
#	define DEBUG_PRINT(...) do{ printf(__VA_ARGS__ ); } while(false)
#else
#	define DEBUG_PRINT(...) (void)0
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
		DEBUG_PRINT("========%s========\n", error);
		return NULL;
	}

	void * ptr = heap_ptr;

	heap_ptr += size;

	return ptr;
}


static void push_stack(void const * ptr, size_t size)
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
	if (stack_ptr + size > (stack + STACK_SIZE))
	{
		error = "STACK UNDERFLOW";
		DEBUG_PRINT("========%s==== %p < %p ==\n", error, stack_ptr + size, stack + STACK_SIZE);
	}

	stack_ptr += size;
}

static void inspect_stack(void)
{
#ifndef NDEGBUG
	printf("Stack values:\n");
	unsigned char * ptr;
	for (ptr = stack_ptr; ptr < (stack + STACK_SIZE); ++ptr)
	{
		printf("\tStack %p %d\n", ptr, *ptr);
	}
#endif
}

static size_t stack_size(void)
{
	return (stack + STACK_SIZE) - stack_ptr;
}

static void require_stack_size(size_t size)
{
	if (stack_size() < size)
	{
		DEBUG_PRINT("Stack too small is %d bytes needed %d bytes\n", (stack + STACK_SIZE) - stack_ptr, size);
	}
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
		DEBUG_PRINT("========%s=======Neighbours=\n", error);
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
		DEBUG_PRINT("========%s========\n", error);
		return NULL;
	}

	if (name_length == 0)
	{
		error = "Need to provide a name for variable";
		DEBUG_PRINT("========%s========\n", error);
		return NULL;
	}

	variable_reg_t * variable = &variable_regs[variable_regs_count++];

	variable->name = (char *)heap_alloc(name_length + 1);
	snprintf(variable->name, name_length + 1, "%s", name);

	DEBUG_PRINT("Registered variable with name '%s'\n", variable->name);

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
		DEBUG_PRINT("========%s========\n", error);
		return NULL;
	}

	if (name_length == 0)
	{
		error = "Need to provide a name for variable";
		DEBUG_PRINT("========%s========\n", error);
		return NULL;
	}


	variable_reg_t * variable = &variable_regs[variable_regs_count++];

	variable->name = (char *)heap_alloc(name_length + 1);
	snprintf(variable->name, name_length + 1, "%s", name);

	// Lets create some space in the heap to store the variable
	// We allocate enough space of `length' `data_size'ed items
	// So we can store `length' user data items
	variable->location = heap_alloc(variable_type_size(type) * length);
	memset(variable->location, 0, variable_type_size(type) * length);

	variable->type = type;
	variable->is_array = true;
	variable->length = length;

	DEBUG_PRINT("Registered array with name '%s' and length %d and elem size %d\n",
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
	DEBUG_PRINT("========%s========\n", error);

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
		DEBUG_PRINT("========%s========\n", error);
		return 1;
	}

	functions_regs[function_regs_count].name = name;
	functions_regs[function_regs_count].fn = fn;
	functions_regs[function_regs_count].type = type;

	// Record that we have another function
	++function_regs_count;

	return 0;
}

// Type is an optional output variable
// pass NULL to it if you don't wait to know the type
static void const * call_function(char const * name, void * data, variable_type_t * type)
{
	size_t i = 0;
	for (; i != function_regs_count; ++i)
	{
		if (strcmp(functions_regs[i].name, name) == 0)
		{
			if (type != NULL)
				*type = functions_regs[i].type;

			return functions_regs[i].fn(data);
		}
	}

	error = "Unknown function name";
	DEBUG_PRINT("========%s========\n", error);

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
	heap_ptr += sizeof(unsigned char);

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

  CALL,

  ICASTF, FCASTI,

  JMP, JZ, JNZ,

  IADD, ISUB, IMUL, IDIV1, IDIV2, IINC,
  IEQ, INEQ, ILT, ILEQ, IGT, IGEQ,

  FADD, FSUB, FMUL, FDIV1, FDIV2,
  FEQ, FNEQ, FLT, FLEQ, FGT, FGEQ,

  AND, OR, XOR, NOT,
} opcode;

#ifndef NDEBUG
static const char * opcode_names[] = {
	"HALT", // Stop evaluation

	"IPUSH", "IPOP", "FPUSH", "FPOP", // Put variables onto the stack
	"IFETCH", "ISTORE", "FFETCH", "FSTORE", // Read / Write variables

	"AFETCH", "ALEN", // Arrays Ops

	"CALL",

	"ICASTF", "FCASTI", // Casting operations

	"JMP", "JZ", "JNZ", // Jump operations

	"IADD", "ISUB", "IMUL", "IDIV1", "IDIV2", "IINC", // Aritmetic operations
	"IEQ", "INEQ", "ILT", "ILEQ", "IGT", "IGEQ", // Comparison Operations

	"FADD", "FSUB", "FMUL", "FDIV1", "FDIV2", // Aritmetic operations
	"FEQ", "FNEQ", "FLT", "FLEQ", "FGT", "FGEQ", // Comparison Operations

	"AND", "OR", "XOR", "NOT", // Logic operations
};
#endif



#define OPERATION_POP(code, op, type, store_type, format_type, idx1, idx2) \
	case code: \
		{ \
			DEBUG_PRINT("Calling %s on " format_type " and " format_type "\n", opcode_names[*current], ((type *)stack_ptr)[idx1], ((type *)stack_ptr)[idx2]); \
			require_stack_size(sizeof(type) * 2); \
			store_type res = ((type *)stack_ptr)[idx1] op ((type *)stack_ptr)[idx2]; \
			pop_stack(sizeof(type) * 2); \
			push_stack(&res, sizeof(store_type)); \
		} break

static void evaluate(unsigned char * start, size_t program_length)
{
	unsigned char * current = start;

	while (current - start < program_length)
	{
		DEBUG_PRINT("Executing %s\n", opcode_names[*current]);

		// Ideally want this op codes in numerical order
		// so the compiler can generate a jump table
		switch (*current)
		{
		case HALT:
			DEBUG_PRINT("Halting\n");
			return;

		case IPUSH:
			DEBUG_PRINT("Pushing int %d onto the stack\n", *(int*)(current + 1));
			int_push_stack(*(int*)(current + 1));
			current += sizeof(int);
			break;

		case IPOP:
			pop_stack(sizeof(int));
			break;

		case FPUSH:
			DEBUG_PRINT("Pushing float %f onto the stack\n", *(float*)(current + 1));
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
			require_stack_size(sizeof(int));
			*get_variable_as_int((char const *)(current + 1)) = *(int *)stack_ptr;
			current += strlen((char const *)(current + 1)) + 1;
			break;

		case FFETCH:
			float_push_stack(*get_variable_as_float((char const *)(current + 1)));
			current += strlen((char const *)(current + 1)) + 1;
			break;

		case FSTORE:
			require_stack_size(sizeof(float));
			*get_variable_as_float((char const *)(current + 1)) = *(float *)stack_ptr;
			current += strlen((char const *)(current + 1)) + 1;
			break;

		case AFETCH:
			{
				require_stack_size(sizeof(int));
				variable_reg_t * var = get_variable((char const *)(current + 1));
				int i = ((int *)stack_ptr)[0];
				pop_stack(sizeof(int));
				push_stack((char *)var->location + (i * variable_type_size(var->type)), variable_type_size(var->type));
				current += strlen((char const *)(current + 1)) + 1;
			} break;

		case ALEN:
			{
				variable_reg_t * var = get_variable((char const *)(current + 1));
				int length = var->length;
				push_stack(&length, sizeof(int));
				current += strlen((char const *)(current + 1)) + 1;
			} break;

		case CALL:
			{
				variable_type_t type;

				void const * data = call_function((char const *)(current + 1), stack_ptr, &type);

				pop_stack(data_size);

				push_stack(data, variable_type_size(type));

				current += strlen((char const *)(current + 1)) + 1;
			} break;

		case ICASTF:
			{
				require_stack_size(sizeof(int));
				float val = (float)((int *)stack_ptr)[0];
				pop_stack(sizeof(int));
				push_stack(&val, sizeof(float));
			} break;

		case FCASTI:
			{
				require_stack_size(sizeof(float));
				int val = (int)((float *)stack_ptr)[0];
				pop_stack(sizeof(float));
				push_stack(&val, sizeof(int));
			} break;

		case JMP:
			current = start + *(int *)(current + 1) - 1;
			break;

		case JZ:
			require_stack_size(sizeof(int));
			if (((int *)stack_ptr)[0] == 0)
			{
				current = start + *(int *)(current + 1) - 1;
				DEBUG_PRINT("Jumping to %p\n", current + 1);
			}
			else
			{
				current += sizeof(int);
			}

			pop_stack(sizeof(int));

			break;

		case JNZ:
			require_stack_size(sizeof(int));
			if (((int *)stack_ptr)[0] != 0)
			{
				current = start + *(int *)(current + 1) - 1;
				DEBUG_PRINT("Jumping to %p\n", current + 1);
			}
			else
			{
				current += sizeof(int);
			}

			pop_stack(sizeof(int));

			break;

		// Integer operations
		OPERATION_POP(IADD, +, int, int, "%d", 0, 1);
		OPERATION_POP(ISUB, -, int, int, "%d", 0, 1);
		OPERATION_POP(IMUL, *, int, int, "%d", 0, 1);
		OPERATION_POP(IDIV1, /, int, int, "%d", 0, 1);
		OPERATION_POP(IDIV2, /, int, int, "%d", 1, 0);

		case IINC:
			require_stack_size(sizeof(int));
			DEBUG_PRINT("Incrementing %d\n", ((int *)stack_ptr)[0]);
			((int *)stack_ptr)[0] += 1;
			break;

		OPERATION_POP(IEQ, ==, int, int, "%d", 0, 1);
		OPERATION_POP(INEQ, !=, int, int, "%d", 0, 1);
		OPERATION_POP(ILT, <, int, int, "%d", 0, 1);
		OPERATION_POP(ILEQ, <=, int, int, "%d", 0, 1);
		OPERATION_POP(IGT, >, int, int, "%d", 0, 1);
		OPERATION_POP(IGEQ, >=, int, int, "%d", 0, 1);

		// Floating point operations
		OPERATION_POP(FADD, +, float, float, "%f", 0, 1);
		OPERATION_POP(FSUB, -, float, float, "%f", 0, 1);
		OPERATION_POP(FMUL, *, float, float, "%f", 0, 1);
		OPERATION_POP(FDIV1, /, float, float, "%f", 0, 1);
		OPERATION_POP(FDIV2, /, float, float, "%f", 1, 0);
		OPERATION_POP(FEQ, ==, float, int, "%f", 0, 1);
		OPERATION_POP(FNEQ, !=, float, int, "%f", 0, 1);
		OPERATION_POP(FLT, <, float, int, "%f", 0, 1);
		OPERATION_POP(FLEQ, <=, float, int, "%f", 0, 1);
		OPERATION_POP(FGT, >, float, int, "%f", 0, 1);
		OPERATION_POP(FGEQ, >=, float, int, "%f", 0, 1);

		// Logical operations
		OPERATION_POP(AND, &&, int, int, "%d", 0, 1);
		OPERATION_POP(OR, ||, int, int, "%d", 0, 1);
		OPERATION_POP(XOR, ^, int, int, "%d", 0, 1);

		case NOT:
			require_stack_size(sizeof(int));
			((int *)stack_ptr)[0] = ! ((int *)stack_ptr)[0];
			break;

		default:
			DEBUG_PRINT("Unknown OP CODE %d\n", *current);
			break;
		}

		//inspect_stack();

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
	stack_ptr = &stack[STACK_SIZE];
	heap_ptr = stack;

	// Lets memset the stack to a certain pattern
	// This makes if obvious if we have memory issues
	memset(stack, 0xEE, STACK_SIZE);


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
	float temp;
	float humidity;
} user_data_t;

static void set_user_data(user_data_t * data, int id, int slot, float temp)
{
	if (data != NULL)
	{
		data->id = id;
		data->slot = slot;
		data->temp = temp;
	}
}

static void * local_node_data_fn(void)
{
	static user_data_t node_data;

	set_user_data(&node_data, 1, 2, 20.0);

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

static void const * get_humidity_fn(void const * ptr)
{
	return &((user_data_t const *)ptr)->humidity;
}


int main(int argc, char * argv[])
{
	init_pred_lang(&local_node_data_fn, sizeof(user_data_t));

	// Register the data functions 
	register_function("id", &get_id_fn, TYPE_INTEGER);
	register_function("slot", &get_slot_fn, TYPE_INTEGER);
	register_function("temp", &get_temp_fn, TYPE_FLOATING);
	register_function("humidity", &get_humidity_fn, TYPE_FLOATING);

	variable_reg_t * result = create_variable("result", strlen("result"), TYPE_INTEGER);

	*((int *)result->location) = *(int const *)call_function("slot", (*data_fn)(), NULL);


	create_variable("i", strlen("i"), TYPE_INTEGER);
	variable_reg_t * var_array = create_array("n1", strlen("n1"), TYPE_USER, 10);

	user_data_t * arr = (user_data_t *)var_array->location;
	set_user_data(&arr[0], 0, 1, 25);
	set_user_data(&arr[1], 1, 3, 26);
	set_user_data(&arr[2], 2, 5, 27);
	set_user_data(&arr[3], 3, 7, 26);
	set_user_data(&arr[4], 4, 9, 25);
	set_user_data(&arr[5], 5, 11, 26);
	set_user_data(&arr[6], 6, 13, 27);
	set_user_data(&arr[7], 7, 15, 26);
	set_user_data(&arr[8], 8, 17, 25);
	set_user_data(&arr[9], 9, 19, 26);

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
	unsigned char * const start = start_gen();

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


	// Initial Code
	gen_op(FPUSH);
	gen_float(0);


	// Initalise loop counter
	gen_op(IPUSH);
	gen_int(0);

	gen_op(ISTORE);
	gen_string("i");

	//gen_op(IPOP);


	// Perform loop termination check
	jmp_label_t label1 = 
	//gen_op(IFETCH);
	//gen_string("i");

	gen_op(ALEN);
	gen_string("n1");

	gen_op(INEQ);

	gen_op(JZ);
	jmp_loc_ptr_t jmp1 =
	gen_jmp();


	// Perform body operations
	gen_op(IFETCH);
	gen_string("i");

	gen_op(AFETCH);
	gen_string("n1");

	gen_op(CALL);
	gen_string("slot");

	gen_op(ICASTF);

	gen_op(FADD);

	gen_op(FPUSH);
	gen_float(2);

	gen_op(FDIV2);

	// Increment loop counter
	gen_op(IFETCH);
	gen_string("i");

	gen_op(IINC);

	gen_op(ISTORE);
	gen_string("i");

	//gen_op(IPOP);


	// Jump to start of loop
	gen_op(JMP);
	jmp_loc_ptr_t jmp2 =
	gen_jmp();


	// Program termination
	unsigned char * last = gen_op(HALT);


	// Set jump locations
	// We do this here because when we add
	// the jump in the code we may not know whereresolve (jump);
	// we are jumping to because the destination
	// may be after the current jump is added.
	alloc_jmp(jmp1, last);
	alloc_jmp(jmp2, label1);


	unsigned char * const end = stop_gen();


	printf("Program length %d\n", end - start);


	// Evaluate the program
	evaluate(start, end - start);

	// Print the results
	printf("Stack ptr value %f\n",
		*((float *)stack_ptr)
	);

	inspect_stack();


	return 0;
}

