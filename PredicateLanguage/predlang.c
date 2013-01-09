#include "predlang.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _MSC_VER
#	include <stdbool.h>
#else
#	define snprintf _snprintf
#endif

//#define MAIN_FUNC
//#define ENABLE_CODE_GEN
//#define NDEBUG

#ifndef NDEBUG
#	define DEBUG_PRINT(...) do { printf(__VA_ARGS__); } while(false)
#else
#	define DEBUG_PRINT(...) (void)0
#endif


#define STACK_SIZE (2 * 1024)

#define MAXIMUM_FUNCTIONS 5
#define MAXIMUM_VARIABLES 5




static ubyte stack[STACK_SIZE];

static ubyte * stack_ptr = NULL;
static ubyte * heap_ptr = NULL;


// Function that gets data on this node
static nuint data_size = 0;


/****************************************************
 ** ERROR MANAGEMENT START
 ***************************************************/

static char const * error;

char const * error_message(void)
{
	return error;
}

/****************************************************
 ** ERROR MANAGEMENT START
 ***************************************************/



/****************************************************
 ** MEMORY MANAGEMENT
 ***************************************************/
static inline void * heap_alloc(nuint size)
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


static inline bool push_stack(void const * ptr, nuint size)
{
	if (stack_ptr - size < heap_ptr)
	{
		error = "Stack overwriting heap";
		DEBUG_PRINT("========%s========\n", error);
		return false;
	}

	stack_ptr -= size;
	memcpy(stack_ptr, ptr, size);

	return true;
}

static inline bool int_push_stack(nint i)
{
	if (stack_ptr - sizeof(nint) < heap_ptr)
	{
		error = "Stack overwriting heap";
		DEBUG_PRINT("========%s========\n", error);
		return false;
	}

	stack_ptr -= sizeof(nint);
	*((nint *)stack_ptr) = i;

	return true;
}

static inline bool float_push_stack(nfloat f)
{
	if (stack_ptr - sizeof(nfloat) < heap_ptr)
	{
		error = "Stack overwriting heap";
		DEBUG_PRINT("========%s========\n", error);
		return false;
	}

	stack_ptr -= sizeof(nfloat);
	*((nfloat *)stack_ptr) = f;

	return true;
}

static inline nuint stack_size(void)
{
	return (stack + STACK_SIZE) - stack_ptr;
}

static bool require_stack_size(nuint size)
{
	if (stack_size() < size)
	{
		error = "STACK UNDERFLOW";
		DEBUG_PRINT("Stack too small is %d bytes needed %d bytes\n", (stack + STACK_SIZE) - stack_ptr, size);
		return false;
	}
	return true;
}

static inline bool pop_stack(nuint size)
{
	if (!require_stack_size(sizeof(nuint)))
		return false;

	stack_ptr += size;

	return true;
}

static void inspect_stack(void)
{
#ifndef NDEGBUG
	printf("Stack values:\n");
	ubyte * ptr;
	for (ptr = stack_ptr; ptr < (stack + STACK_SIZE); ++ptr)
	{
		printf("\tStack %p %d\n", ptr, *ptr);
	}
#endif
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

	nuint type : 2;
	nuint is_array : 1;
	nuint length : 13;

} variable_reg_t;


static nuint variable_type_size(nuint type)
{
	switch (type)
	{
	case TYPE_INTEGER: return sizeof(nint);
	case TYPE_FLOATING: return sizeof(nfloat);
	case TYPE_USER: return data_size;
	default: 
		error = "Unknown variable type";
		DEBUG_PRINT("========%s=======Neighbours=\n", error);
		return 0;
	}
}


static variable_reg_t * variable_regs = NULL;
static nuint variable_regs_count = 0;

static variable_reg_t * create_variable(char const * name, nuint name_length, variable_type_t type)
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

	variable_reg_t * variable = &variable_regs[variable_regs_count];

	variable->name = (char *)heap_alloc(name_length + 1);

	if (variable->name == NULL)
	{
		error = "Failed to allocate enough space on heap for variable name";
		DEBUG_PRINT("========%s=====%d===\n", error, name_length + 1);
		return NULL;
	}

	snprintf(variable->name, name_length + 1, "%s", name);

	DEBUG_PRINT("Registered variable with name '%s'\n", variable->name);

	// Lets create some space in the heap to store the variable
	variable->location = heap_alloc(variable_type_size(type));

	if (variable->location == NULL)
	{
		error = "Failed to allocate enough space on heap for variable";
		DEBUG_PRINT("========%s=====%d===\n", error, variable_type_size(type));
		return NULL;
	}

	memset(variable->location, 0, variable_type_size(type));

	variable->type = type;
	variable->is_array = false;
	variable->length = 0;

	variable_regs_count += 1;

	return variable;
}

static variable_reg_t * create_array(char const * name, nuint name_length, variable_type_t type, nuint length)
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


	variable_reg_t * variable = &variable_regs[variable_regs_count];

	variable->name = (char *)heap_alloc(name_length + 1);

	if (variable->name == NULL)
	{
		error = "Failed to allocate enough space on heap for variable name";
		DEBUG_PRINT("========%s=====%d===\n", error, name_length + 1);
		return NULL;
	}

	snprintf(variable->name, name_length + 1, "%s", name);

	// Lets create some space in the heap to store the variable
	// We allocate enough space of `length' `data_size'ed items
	// So we can store `length' user data items
	variable->location = heap_alloc(variable_type_size(type) * length);

	if (variable->location == NULL)
	{
		error = "Failed to allocate enough space on heap for variable";
		DEBUG_PRINT("========%s=====%d===\n", error, variable_type_size(type) * length);
		return NULL;
	}

	memset(variable->location, 0, variable_type_size(type) * length);

	variable->type = type;
	variable->is_array = true;
	variable->length = length;

	DEBUG_PRINT("Registered array with name '%s' and length %d and elem size %d\n",
		variable->name, variable->length, variable_type_size(type));

	variable_regs_count += 1;

	return variable;
}

static variable_reg_t * get_variable(char const * name)
{
	nuint i = 0;
	for (; i != variable_regs_count; ++i)
	{
		variable_reg_t * variable = &variable_regs[i];

		if (strcmp(name, variable->name) == 0)
		{
			return variable;
		}
	}

	error = "No variable with the given name exists";
	DEBUG_PRINT("========%s=====%s===\n", error, name);

	return NULL;
}

static inline nint * get_variable_as_int(char const * name)
{
	variable_reg_t * var = get_variable(name);

	return (var != NULL) ? (nint *)var->location : NULL;
}

static inline nfloat * get_variable_as_float(char const * name)
{
	variable_reg_t * var = get_variable(name);

	return (var != NULL) ? (nfloat *)var->location : NULL;
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
static nuint function_regs_count = 0;

bool register_function(char const * name, data_access_fn fn, variable_type_t type)
{
	if (function_regs_count == MAXIMUM_FUNCTIONS)
	{
		error = "Already registered maximum number of functions";
		DEBUG_PRINT("========%s=====%s===\n", error, name);
		return false;
	}

	functions_regs[function_regs_count].name = name;
	functions_regs[function_regs_count].fn = fn;
	functions_regs[function_regs_count].type = type;

	// Record that we have another function
	++function_regs_count;

	return true;
}

static function_reg_t * get_function(char const * name)
{
	nuint i = 0;
	for (; i != function_regs_count; ++i)
	{
		if (strcmp(functions_regs[i].name, name) == 0)
		{
			return &functions_regs[i];
		}
	}

	error = "Unknown function name";
	DEBUG_PRINT("========%s======%s==\n", error, name);

	return NULL;
}

// Type is an optional output variable
// pass NULL to it if you don't wait to know the type
static void const * call_function(char const * name, void * data, variable_type_t * type)
{
	function_reg_t * reg = get_function(name);

	if (reg != NULL)
	{
		if (type != NULL)
			*type = reg->type;

		return reg->fn(data);
	}

	error = "Function with given name doesn't exist";
	DEBUG_PRINT("========%s======%s==\n", error, name);

	return NULL;
}


/****************************************************
 ** FUNCTION MANAGEMENT END
 ***************************************************/

#ifdef ENABLE_CODE_GEN
/****************************************************
 ** CODE GEN
 ***************************************************/

typedef nint jmp_loc_t;
typedef jmp_loc_t * jmp_loc_ptr_t;
typedef ubyte * jmp_label_t;

static ubyte * program_start;
static ubyte * program_end;

static inline ubyte * start_gen(void)
{
	return program_start = heap_ptr;
}

static inline jmp_label_t gen_op(ubyte op)
{
	ubyte * pos = heap_ptr;

	*heap_ptr = op;
	heap_ptr += sizeof(ubyte);

	return pos;
}

static inline void gen_int(nint i)
{
	*(nint *)heap_ptr = i;
	heap_ptr += sizeof(nint);
}

static inline void gen_float(nfloat f)
{
	*(nfloat *)heap_ptr = f;
	heap_ptr += sizeof(nfloat);
}

static inline void gen_string(char const * str)
{
	nuint len = strlen(str) + 1;

	memcpy(heap_ptr, str, len);
	heap_ptr += len;
}

static inline jmp_loc_ptr_t gen_jmp(void)
{
	jmp_loc_ptr_t pos = (jmp_loc_ptr_t)heap_ptr;

	*pos = 0;

	heap_ptr += sizeof(jmp_loc_t);

	return pos;
}

static inline void alloc_jmp(jmp_loc_ptr_t jmp, jmp_label_t label)
{
	*jmp = label - program_start;
}

static inline ubyte * stop_gen(void)
{
	return program_end = heap_ptr;
}

/****************************************************
 ** CODE GEN
 ***************************************************/
#endif


/****************************************************
 ** VM START
 ***************************************************/
typedef enum {
  HALT,

  IPUSH, IPOP, FPUSH, FPOP,
  IFETCH, ISTORE, FFETCH, FSTORE,
  
  AFETCH, ALEN,

  ASUM,

  CALL,

  ICASTF, FCASTI,

  JMP, JZ, JNZ,

  IADD, ISUB, IMUL, IDIV1, IDIV2, IINC,
  IEQ, INEQ, ILT, ILEQ, IGT, IGEQ,

  FADD, FSUB, FMUL, FDIV1, FDIV2,
  FEQ, FNEQ, FLT, FLEQ, FGT, FGEQ,

  AND, OR, XOR, NOT,

  IVAR, FVAR,
} opcode;

#ifndef NDEBUG
static const char * opcode_names[] = {
	"HALT", // Stop evaluation

	"IPUSH", "IPOP", "FPUSH", "FPOP", // Put variables onto the stack
	"IFETCH", "ISTORE", "FFETCH", "FSTORE", // Read / Write variables

	"AFETCH", "ALEN", // Arrays Ops

	"ASUM", // Custom user data array ops

	"CALL",

	"ICASTF", "FCASTI", // Casting operations

	"JMP", "JZ", "JNZ", // Jump operations

	"IADD", "ISUB", "IMUL", "IDIV1", "IDIV2", "IINC", // Aritmetic operations
	"IEQ", "INEQ", "ILT", "ILEQ", "IGT", "IGEQ", // Comparison Operations

	"FADD", "FSUB", "FMUL", "FDIV1", "FDIV2", // Aritmetic operations
	"FEQ", "FNEQ", "FLT", "FLEQ", "FGT", "FGEQ", // Comparison Operations

	"AND", "OR", "XOR", "NOT", // Logic operations

	"IVAR", "FVAR", // Variable creation
};
#endif

#define OPERATION_POP(code, op, type, store_type, format_type, idx1, idx2) \
	case code: \
		{ \
			DEBUG_PRINT("Calling %s on " format_type " and " format_type "\n", opcode_names[*current], ((type *)stack_ptr)[idx1], ((type *)stack_ptr)[idx2]); \
			if (!require_stack_size(sizeof(type) * 2)) \
				return false; \
			store_type res = ((type *)stack_ptr)[idx1] op ((type *)stack_ptr)[idx2]; \
			if (!pop_stack(sizeof(type) * 2)) \
				return false; \
			if (!push_stack(&res, sizeof(store_type))) \
				return false; \
		} break

nbool evaluate(ubyte * start, nuint program_length)
{
	ubyte * current = start;

	while (current - start < program_length)
	{
		DEBUG_PRINT("Executing %s at %p\n", opcode_names[*current], current);

		// Ideally want this op codes in numerical order
		// so the compiler can generate a jump table
		switch (*current)
		{
		case HALT:
			DEBUG_PRINT("Halting\n");
			if (!require_stack_size(sizeof(nbool)))
				return false;

			return *(int *)stack_ptr;

		case IPUSH:
			DEBUG_PRINT("Pushing int %d onto the stack\n", *(nint*)(current + 1));
			if (!int_push_stack(*(nint*)(current + 1)))
				return false;
			current += sizeof(nint);
			break;

		case IPOP:
			pop_stack(sizeof(nint));
			break;

		case FPUSH:
			DEBUG_PRINT("Pushing float %f onto the stack\n", *(nfloat*)(current + 1));
			if (!float_push_stack(*(nfloat*)(current + 1)))
				return false;
			current += sizeof(nfloat);
			break;

		case FPOP:
			if (!pop_stack(sizeof(nfloat)))
				return false;
			break;

		case IFETCH:
			{
				nint * var = get_variable_as_int((char const *)(current + 1));

				if (var == NULL)
					return false;

				if (!int_push_stack(*var))
					return false;

				current += strlen((char const *)(current + 1)) + 1;
			} break;

		case ISTORE:
			{
				if (!require_stack_size(sizeof(nint)))
					return false;

				nint * var = get_variable_as_int((char const *)(current + 1));

				if (var == NULL)
					return false;
				
				*var = *(nint *)stack_ptr;

				current += strlen((char const *)(current + 1)) + 1;
			} break;

		case FFETCH:
			{
				nfloat * var = get_variable_as_float((char const *)(current + 1));

				if (var == NULL)
					return false;

				if (!float_push_stack(*var))
					return false;

				current += strlen((char const *)(current + 1)) + 1;
			} break;

		case FSTORE:
			{
				if (!require_stack_size(sizeof(nfloat)))
					return false;

				nfloat * var = get_variable_as_float((char const *)(current + 1));

				if (var == NULL)
					return false;
				
				*var = *(nfloat *)stack_ptr;

				current += strlen((char const *)(current + 1)) + 1;
			} break;

		case AFETCH:
			{
				if (!require_stack_size(sizeof(nint)))
					return false;

				variable_reg_t * var = get_variable((char const *)(current + 1));

				nint i = ((nint *)stack_ptr)[0];

				if (!pop_stack(sizeof(nint)))
					return false;

				if (!push_stack((char *)var->location + (i * variable_type_size(var->type)), variable_type_size(var->type)))
					return false;

				current += strlen((char const *)(current + 1)) + 1;
			} break;

		case ALEN:
			{
				variable_reg_t * var = get_variable((char const *)(current + 1));

				if (var == NULL)
					return false;

				if (!int_push_stack(var->length))
					return false;

				current += strlen((char const *)(current + 1)) + 1;
			} break;

		case ASUM:
			{
				char const * array_name = (char const *)(current + 1);
				current += strlen(array_name) + 1;
				DEBUG_PRINT("Array name %s\n", array_name);

				char const * fn_name = (char const *)(current + 1);
				current += strlen(fn_name) + 1;
				DEBUG_PRINT("FN name %s\n", fn_name);

				variable_reg_t const * var_reg = get_variable(array_name);

				if (var_reg == NULL)
					return false;

				function_reg_t const * fn_reg = get_function(fn_name);

				if (fn_reg == NULL)
					return false;

				if (var_reg->type == TYPE_USER)
				{
					nfloat op_result = 0;
					nuint size = variable_type_size(TYPE_USER);
					byte const * data = (byte *)var_reg->location;
					byte const * const end = data + (size * var_reg->length);

					for (; data != end; data += size)
					{
						void const * result = fn_reg->fn(data);

						if (result == NULL)
						{
							error = "User defined function returns NULL";
							DEBUG_PRINT("==========%s==========\n", error);
							return false;
						}

						if (fn_reg->type == TYPE_INTEGER)
							op_result += (nfloat)*(nint const *)result;
						else
							op_result += *(nfloat const *)result;
					}

					if (!float_push_stack(op_result))
						return false;
				}
				else
				{
					error = "Variable not an array of user types!";
					DEBUG_PRINT("==========%s==========\n", error);
					return false;
				}
			} break;

		case CALL:
			{
				if (!require_stack_size(data_size))
					return false;

				variable_type_t type;
				void const * data = call_function((char const *)(current + 1), stack_ptr, &type);
				current += strlen((char const *)(current + 1)) + 1;

				if (data == NULL)
				{
					return false;
				}

				if (!pop_stack(data_size))
					return false;

				if (!push_stack(data, variable_type_size(type)))
					return false;

			} break;

		case ICASTF:
			{
				if (!require_stack_size(sizeof(nint)))
					return false;

				nfloat val = (nfloat)((nint *)stack_ptr)[0];

				if (!pop_stack(sizeof(nint)))
					return false;

				if (!float_push_stack(val))
					return false;
			} break;

		case FCASTI:
			{
				if (!require_stack_size(sizeof(nfloat)))
					return false;

				nint val = (nint)((nfloat *)stack_ptr)[0];

				if (!pop_stack(sizeof(nfloat)))
					return false;

				if (!int_push_stack(val))
					return false;
			} break;

		case JMP:
			current = start + *(nint *)(current + 1) - 1;
			DEBUG_PRINT("Jumping to %p\n", current + 1);
			break;

		case JZ:
			if (!require_stack_size(sizeof(nint)))
				return false;

			if (((nint *)stack_ptr)[0] == 0)
			{
				current = start + *(nint *)(current + 1) - 1;
				DEBUG_PRINT("Jumping to %p\n", current + 1);
			}
			else
			{
				current += sizeof(nint);
			}

			if (!pop_stack(sizeof(nint)))
				return false;

			break;

		case JNZ:
			if (!require_stack_size(sizeof(nint)))
				return false;

			if (((nint *)stack_ptr)[0] != 0)
			{
				current = start + *(nint *)(current + 1) - 1;
				DEBUG_PRINT("Jumping to %p\n", current + 1);
			}
			else
			{
				current += sizeof(nint);
			}

			if (!pop_stack(sizeof(nint)))
				return false;

			break;

		// Integer operations
		OPERATION_POP(IADD, +, nint, nint, "%d", 0, 1);
		OPERATION_POP(ISUB, -, nint, nint, "%d", 0, 1);
		OPERATION_POP(IMUL, *, nint, nint, "%d", 0, 1);
		OPERATION_POP(IDIV1, /, nint, nint, "%d", 0, 1);
		OPERATION_POP(IDIV2, /, nint, nint, "%d", 1, 0);

		case IINC:
			if (!require_stack_size(sizeof(nint)))
				return false;

			DEBUG_PRINT("Incrementing %d\n", ((nint *)stack_ptr)[0]);
			((nint *)stack_ptr)[0] += 1;

			break;

		OPERATION_POP(IEQ, ==, nint, nbool, "%d", 0, 1);
		OPERATION_POP(INEQ, !=, nint, nbool, "%d", 0, 1);
		OPERATION_POP(ILT, <, nint, nbool, "%d", 0, 1);
		OPERATION_POP(ILEQ, <=, nint, nbool, "%d", 0, 1);
		OPERATION_POP(IGT, >, nint, nbool, "%d", 0, 1);
		OPERATION_POP(IGEQ, >=, nint, nbool, "%d", 0, 1);

		// Floating point operations
		OPERATION_POP(FADD, +, nfloat, nfloat, "%f", 0, 1);
		OPERATION_POP(FSUB, -, nfloat, nfloat, "%f", 0, 1);
		OPERATION_POP(FMUL, *, nfloat, nfloat, "%f", 0, 1);
		OPERATION_POP(FDIV1, /, nfloat, nfloat, "%f", 0, 1);
		OPERATION_POP(FDIV2, /, nfloat, nfloat, "%f", 1, 0);
		OPERATION_POP(FEQ, ==, nfloat, nbool, "%f", 0, 1);
		OPERATION_POP(FNEQ, !=, nfloat, nbool, "%f", 0, 1);
		OPERATION_POP(FLT, <, nfloat, nbool, "%f", 0, 1);
		OPERATION_POP(FLEQ, <=, nfloat, nbool, "%f", 0, 1);
		OPERATION_POP(FGT, >, nfloat, nbool, "%f", 0, 1);
		OPERATION_POP(FGEQ, >=, nfloat, nbool, "%f", 0, 1);

		// Logical operations
		OPERATION_POP(AND, &&, nbool, nbool, "%d", 0, 1);
		OPERATION_POP(OR, ||, nbool, nbool, "%d", 0, 1);
		OPERATION_POP(XOR, ^, nbool, nbool, "%d", 0, 1);

		case NOT:
			if (!require_stack_size(sizeof(nbool)))
				return false;
			((nbool *)stack_ptr)[0] = ! ((nbool *)stack_ptr)[0];
			break;

		case IVAR:
			{
				const char * name = (char const *)(current + 1);
				nuint name_Length = strlen((char const *)(current + 1));

				if (create_variable(name, name_Length, TYPE_INTEGER) == NULL)
				{
					return false;
				}

				current += name_Length + 1;
			} break;

		case FVAR:
			{
				const char * name = (char const *)(current + 1);
				nuint name_Length = strlen((char const *)(current + 1));

				if (create_variable(name, name_Length, TYPE_FLOATING) == NULL)
				{
					return false;
				}

				current += name_Length + 1;
			} break;

		default:
			DEBUG_PRINT("Unknown OP CODE %d\n", *current);
			break;
		}

		//inspect_stack();

		++current;
	}

	require_stack_size(sizeof(nbool));

	return *(nbool *)stack_ptr;
}


/****************************************************
 ** VM END
 ***************************************************/




/****************************************************
 ** INIT MANAGEMENT END
 ***************************************************/

bool init_pred_lang(node_data_fn given_data_fn, nuint given_data_size)
{
	// Make sure wqe are given valid functions
	if (given_data_fn == NULL)
		return false;

	if (given_data_size == 0)
		return false;

	// Reset the stack and heap positions
	stack_ptr = &stack[STACK_SIZE];
	heap_ptr = stack;

	// Lets memset the stack to a certain pattern
	// This makes if obvious if we have memory issues
	memset(stack, 0xEE, STACK_SIZE);


	// Allocate some space for function registrations
	functions_regs = (function_reg_t *)heap_alloc(sizeof(function_reg_t) * MAXIMUM_FUNCTIONS);

	if (functions_regs == NULL)
	{
		return false;
	}

	// Allocate space for variable registrations
	variable_regs = (variable_reg_t *)heap_alloc(sizeof(variable_reg_t) * MAXIMUM_VARIABLES);

	if (variable_regs == NULL)
	{
		return false;
	}

	// Put values in the `this' variable
	variable_reg_t * thisvar = create_variable("this", given_data_size, TYPE_USER);
	given_data_fn(thisvar->location);

	// Reset the error message variable
	error = NULL;

	return true;
}


/****************************************************
 ** INIT MANAGEMENT END
 ***************************************************/




#ifdef MAIN_FUNC
/****************************************************
 ** USER CODE FROM HERE ON
 ***************************************************/

typedef struct
{
	nint id;
	nint slot;
	nfloat temp;
	nfloat humidity;
} user_data_t;

static void set_user_data(user_data_t * data, nint id, nint slot, nfloat temp, nfloat humidity)
{
	if (data != NULL)
	{
		data->id = id;
		data->slot = slot;
		data->temp = temp;
		data->humidity = humidity;
	}
}

static void local_node_data_fn(void * data)
{
	user_data_t * node_data = (user_data_t *)data;

	set_user_data(node_data, 1, 2, 20.0, 122);
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
#endif

#ifdef ENABLE_CODE_GEN
static void gen_example1(void)
{
	start_gen();

	gen_op(IVAR); gen_string("result");

	gen_op(IPUSH); gen_int(2);
	gen_op(ISTORE); gen_string("result");
	gen_op(IPOP);

	gen_op(IPUSH); gen_int(2);
	gen_op(IPUSH); gen_int(3);
	gen_op(IADD);

	gen_op(IFETCH); gen_string("result");
	gen_op(IADD);

	gen_op(ICASTF);

	gen_op(FPUSH); gen_float(7.5);

	gen_op(FGT);

	gen_op(NOT);

	stop_gen();
}

static void gen_example_mean(void)
{
	start_gen();

	gen_op(ASUM); gen_string("n1"); gen_string("id");

	gen_op(ALEN); gen_string("n1");

	gen_op(ICASTF);

	gen_op(FDIV2);

	stop_gen();
}

static void gen_example_for_loop(void)
{
	start_gen();

	// Initial Code
	gen_op(IVAR); gen_string("i");

	gen_op(FPUSH); gen_float(0);

	// Initalise loop counter
	gen_op(IPUSH); gen_int(0);
	gen_op(ISTORE); gen_string("i");

	// Perform loop termination check
	jmp_label_t label1 = 
	gen_op(ALEN); gen_string("n1");

	gen_op(INEQ);
	gen_op(JZ); jmp_loc_ptr_t jmp1 = gen_jmp();


	// Perform body operations
	gen_op(IFETCH); gen_string("i");

	gen_op(AFETCH); gen_string("n1");
	gen_op(CALL); gen_string("slot");
	gen_op(ICASTF);

	gen_op(FADD);

	gen_op(FPUSH); gen_float(2);

	gen_op(FDIV2);

	// Increment loop counter
	gen_op(IFETCH); gen_string("i");
	gen_op(IINC);
	gen_op(ISTORE); gen_string("i");


	// Jump to start of loop
	gen_op(JMP); jmp_loc_ptr_t jmp2 = gen_jmp();


	// Program termination
	ubyte * last = gen_op(HALT);


	// Set jump locations
	// We do this here because when we add
	// the jump in the code we may not know whereresolve (jump);
	// we are jumping to because the destination
	// may be after the current jump is added.
	alloc_jmp(jmp1, last);
	alloc_jmp(jmp2, label1);

	stop_gen();
}
#endif

#ifdef MAIN_FUNC
// FROM: http://www.anyexample.com/programming/c/how_to_load_file_into_memory_using_plain_ansi_c_language.xml
nuint load_file_to_memory(char const * filename, ubyte ** result) 
{
	if (filename == NULL || result == NULL)
	{
		return -4;
	}

	FILE * f = fopen(filename, "rb");

	if (f == NULL)
	{
		*result = NULL;
		return -1; // -1 means file opening fail
	}

	fseek(f, 0, SEEK_END);
	nint size = ftell(f);
	fseek(f, 0, SEEK_SET);

	*result = (ubyte *)heap_alloc(size);

	if (*result == NULL)
	{
		return -3;
	}

	if (size != fread(*result, sizeof(ubyte), size, f))
	{
		return -2; // -2 means file reading fail
	}

	fclose(f);

	return size;
}

static bool run_program_from_file(int argc, char ** argv)
{
	char const * filename = argv[1];

	printf("Filename: %s\n", filename);

	nuint program_size = load_file_to_memory(filename, &program_start);

	if (program_size <= 0)
	{
		return false;
	}

	program_end = program_start + program_size;

	printf("Program length %d\n", program_size);

	return true;
}

int main(int argc, char * argv[])
{
	init_pred_lang(&local_node_data_fn, sizeof(user_data_t));

	// Register the data functions 
	register_function("id", &get_id_fn, TYPE_INTEGER);
	register_function("slot", &get_slot_fn, TYPE_INTEGER);
	register_function("temp", &get_temp_fn, TYPE_FLOATING);
	register_function("humidity", &get_humidity_fn, TYPE_FLOATING);

	variable_reg_t * var_array = create_array("n1", strlen("n1"), TYPE_USER, 10);

	user_data_t * arr = (user_data_t *)var_array->location;
	set_user_data(&arr[0], 0, 1, 25, 122);
	set_user_data(&arr[1], 1, 3, 26, 122);
	set_user_data(&arr[2], 2, 5, 27, 122);
	set_user_data(&arr[3], 3, 7, 26, 122);
	set_user_data(&arr[4], 4, 9, 25, 122);
	set_user_data(&arr[5], 5, 11, 26, 122);
	set_user_data(&arr[6], 6, 13, 27, 122);
	set_user_data(&arr[7], 7, 15, 26, 122);
	set_user_data(&arr[8], 8, 17, 25, 122);
	set_user_data(&arr[9], 9, 19, 26, 122);

	printf("Array length %d\n", var_array->length);

	printf("sizeof(void *): %u\n", sizeof(void *));
	printf("sizeof(int): %u\n", sizeof(nint));
	printf("sizeof(float): %u\n", sizeof(nfloat));
	printf("sizeof(variable_reg_t): %u\n", sizeof(variable_reg_t));
	printf("sizeof(function_reg_t): %u\n", sizeof(function_reg_t));

	// Load a program into memory
	gen_example_for_loop();

	// Evaluate the program
	nbool result = evaluate(program_start, program_end - program_start);

	// Print the results
	printf("Stack ptr value (float %f) (int %d) (bool %d)\n",
		*((nfloat *)stack_ptr), *((nint *)stack_ptr), *((nbool *)stack_ptr) != 0
	);

	printf("Result: %d\n", result);

	inspect_stack();

	return 0;
}
#endif

