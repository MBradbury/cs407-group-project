#include "predlang.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//#define MAIN_FUNC
//#define ENABLE_CODE_GEN
//#define NDEBUG

#ifndef NDEBUG
#	define DEBUG_PRINT(...) do { printf(__VA_ARGS__); } while(false)
#	define DEBUG_ERR_PRINT(...) do { fprintf(stderr, __VA_ARGS__); } while(false)
#else
#	define DEBUG_PRINT(...) (void)0
#	define DEBUG_ERR_PRINT(...) (void)0
#endif


#define STACK_SIZE (1 * 512)

#define MAXIMUM_FUNCTIONS 5
#define MAXIMUM_VARIABLES 10


#define THIS_VAR_ID 0


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

#ifndef NDEBUG
static void inspect_stack(FILE * f)
{

	fprintf(f, "Stack values:\n");
	ubyte * max = stack + STACK_SIZE;
	ubyte * ptr;
	for (ptr = stack_ptr; ptr < max; ++ptr)
	{
		fprintf(f, "\tStack %d %d\n", max - ptr, *ptr);
	}
}
#else
#	define inspect_stack(f)
#endif

/****************************************************
 ** MEMORY MANAGEMENT
 ***************************************************/


/****************************************************
 ** VARIABLE MANAGEMENT START
 ***************************************************/

typedef struct
{
	variable_id_t id;
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


static variable_reg_t variable_regs[MAXIMUM_VARIABLES];
static nuint variable_regs_count = 0;

static variable_reg_t * create_variable(variable_id_t id, variable_type_t type)
{
	if (variable_regs_count == MAXIMUM_VARIABLES)
	{
		error = "Created maximum number of variables";
		DEBUG_PRINT("========%s========\n", error);
		return NULL;
	}

	size_t i = 0;
	for (; i != variable_regs_count; ++i)
	{
		if (variable_regs[i].id == id)
		{
			if (variable_regs[i].type == type && variable_regs[i].is_array == false &&
				variable_regs[i].length == 0 && variable_regs[i].location != NULL)
			{
				// Reset variable
				memset(variable_regs[i].location, 0, variable_type_size(type));

				return &variable_regs[i];
			}
			else
			{
				error = "Already registered variable with id";
				DEBUG_PRINT("========%s=====%u===\n", error, id);
				return NULL;
			}
		}
	}

	variable_reg_t * variable = &variable_regs[variable_regs_count];

	variable->id = id;

	DEBUG_ERR_PRINT("Registered variable with id '%u'\n", variable->id);

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

static variable_reg_t * create_array(variable_id_t id, variable_type_t type, nuint length)
{
	if (variable_regs_count == MAXIMUM_VARIABLES)
	{
		error = "Created maximum number of variables";
		DEBUG_PRINT("========%s========\n", error);
		return NULL;
	}

	size_t i = 0;
	for (; i != variable_regs_count; ++i)
	{
		if (variable_regs[i].id == id)
		{
			error = "Already registered variable with id";
			DEBUG_PRINT("========%s=====%u===\n", error, id);
			return NULL;
		}
	}

	variable_reg_t * variable = &variable_regs[variable_regs_count];

	variable->id = id;

	variable->location = NULL;

	variable->type = type;
	variable->is_array = true;
	variable->length = length;

	DEBUG_ERR_PRINT("Registered array with id '%u' and length %d and elem size %d\n",
		variable->id, variable->length, variable_type_size(type));

	variable_regs_count += 1;

	return variable;
}

static bool alloc_array(variable_reg_t * variable)
{
	if (variable == NULL)
		return false;

	// Lets create some space in the heap to store the variable
	// We allocate enough space of `length' `data_size'ed items
	// So we can store `length' user data items
	variable->location = heap_alloc(variable_type_size(variable->type) * variable->length);

	if (variable->location == NULL)
	{
		error = "Failed to allocate enough space on heap for variable";
		DEBUG_PRINT("========%s=====%d===\n", error, variable_type_size(variable->type) * variable->length);
		return false;
	}

	memset(variable->location, 0, variable_type_size(variable->type) * variable->length);

	return true;
}

static variable_reg_t * get_variable(variable_id_t id)
{
	nuint i = 0;
	for (; i != variable_regs_count; ++i)
	{
		variable_reg_t * variable = &variable_regs[i];

		if (id == variable->id)
		{
			return variable;
		}
	}

	error = "No variable with the given name exists";
	DEBUG_PRINT("========%s=====%u===\n", error, id);

	return NULL;
}

static inline nint * get_variable_as_int(variable_id_t id)
{
	variable_reg_t * var = get_variable(id);

	return (var != NULL) ? (nint *)var->location : NULL;
}

static inline nfloat * get_variable_as_float(variable_id_t id)
{
	variable_reg_t * var = get_variable(id);

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
	function_id_t id;
	data_access_fn fn;
	variable_type_t type;
} function_reg_t;

static function_reg_t functions_regs[MAXIMUM_FUNCTIONS];
static nuint function_regs_count = 0;

bool register_function(function_id_t id, data_access_fn fn, variable_type_t type)
{
	if (function_regs_count == MAXIMUM_FUNCTIONS)
	{
		error = "Already registered maximum number of functions";
		DEBUG_PRINT("========%s=====%u===\n", error, id);
		return false;
	}

	size_t i = 0;
	for (; i != function_regs_count; ++i)
	{
		if (functions_regs[i].id == id)
		{
			error = "Already registered function with id";
			DEBUG_PRINT("========%s=====%u===\n", error, id);
			return false;
		}
	}

	functions_regs[function_regs_count].id = id;
	functions_regs[function_regs_count].fn = fn;
	functions_regs[function_regs_count].type = type;

	// Record that we have another function
	++function_regs_count;

	return true;
}

static function_reg_t const * get_function(function_id_t id)
{
	nuint i = 0;
	for (; i != function_regs_count; ++i)
	{
		if (functions_regs[i].id == id)
		{
			return &functions_regs[i];
		}
	}

	error = "Unknown function name";
	DEBUG_PRINT("========%s======%u==\n", error, id);

	return NULL;
}

// Type is an optional output variable
// pass NULL to it if you don't wait to know the type
static void const * call_function(function_id_t id, void * data, variable_type_t * type)
{
	function_reg_t const * reg = get_function(id);

	if (reg != NULL)
	{
		if (type != NULL)
			*type = reg->type;

		return reg->fn(data);
	}

	error = "Function with given id doesn't exist";
	DEBUG_PRINT("========%s======%u==\n", error, id);

	return NULL;
}


/****************************************************
 ** FUNCTION MANAGEMENT END
 ***************************************************/

#ifdef ENABLE_CODE_GEN
/****************************************************
 ** CODE GEN
 ***************************************************/

typedef ubyte jmp_loc_t;
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

static inline void gen_ubyte(ubyte i)
{
	*(ubyte *)heap_ptr = i;
	heap_ptr += sizeof(ubyte);
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
  HALT=0,

  IPUSH=1, IPOP=2, FPUSH=3, FPOP=4,
  IFETCH=5, ISTORE=6, FFETCH=7, FSTORE=8,
  
  AFETCH=9, ALEN=10,

  ASUM=11, AMEAN=12, AMAX=13, AMIN=14,

  CALL=15,

  ICASTF=16, FCASTI=17,

  JMP=18, JZ=19, JNZ=20,

  IADD=21, ISUB=22, IMUL=23, IDIV1=24, IDIV2=25, IINC=26, 
  IEQ=27, INEQ=28, ILT=29, ILEQ=30, IGT=31, IGEQ=32,

  FADD=33, FSUB=34, FMUL=35, FDIV1=36, FDIV2=37,
  FEQ=38, FNEQ=39, FLT=40, FLEQ=41, FGT=42, FGEQ=43,

  AND=44, OR=45, XOR=46, NOT=47,

  IVAR=48, FVAR=49,

  IABS=50, FABS=51,

  FPOW=52,

  VIINC=53, VIDEC=54, VIFAFC=55,

  IDEC=56,

  EQUIVALENT=57, IMPLIES=58,

} opcode;

#ifndef NDEBUG
static const char * opcode_names[] = {
	"HALT", // Stop evaluation

	"IPUSH", "IPOP", "FPUSH", "FPOP", // Put variables onto the stack
	"IFETCH", "ISTORE", "FFETCH", "FSTORE", // Read / Write variables

	"AFETCH", "ALEN", // Arrays Ops

	"ASUM", "AMEAN", "AMAX", "AMIN", // Custom user data array ops

	"CALL",

	"ICASTF", "FCASTI", // Casting operations

	"JMP", "JZ", "JNZ", // Jump operations

	"IADD", "ISUB", "IMUL", "IDIV1", "IDIV2", "IINC", // Aritmetic operations
	"IEQ", "INEQ", "ILT", "ILEQ", "IGT", "IGEQ", // Comparison Operations

	"FADD", "FSUB", "FMUL", "FDIV1", "FDIV2", // Aritmetic operations
	"FEQ", "FNEQ", "FLT", "FLEQ", "FGT", "FGEQ", // Comparison Operations

	"AND", "OR", "XOR", "NOT", // Logic operations

	"IVAR", "FVAR", // Variable creation

	"IABS", "FABS",

	"FPOW",

	"VIINC", "VIDEC", "VIFAFC",

	"IDEC",

	"EQUIVALENT", "IMPLIES"
};
#endif

#define OPERATION_POP(code, op, type, store_type, format_type, idx1, idx2) \
	case code: \
		{ \
			DEBUG_ERR_PRINT("Calling %s on " format_type " and " format_type "\n", opcode_names[*current], ((type *)stack_ptr)[idx1], ((type *)stack_ptr)[idx2]); \
			if (!require_stack_size(sizeof(type) * 2)) \
				return false; \
			store_type res = ((type *)stack_ptr)[idx1] op ((type *)stack_ptr)[idx2]; \
			if (!pop_stack(sizeof(type) * 2)) \
				return false; \
			if (!push_stack(&res, sizeof(store_type))) \
				return false; \
		} break

#define OPERATION_ARRAY(code, array_fn, end_array_fn) \
	case code: \
		{ \
			variable_id_t array_id = *(variable_id_t const *)(current + 1); \
			current += sizeof(variable_id_t); \
			DEBUG_ERR_PRINT("Array id %u\n", array_id); \
			 \
			variable_id_t fn_id = *(variable_id_t const *)(current + 1); \
			current += sizeof(variable_id_t); \
			DEBUG_ERR_PRINT("FN id %u\n", fn_id); \
			 \
			variable_reg_t const * var_reg = get_variable(array_id); \
			 \
			if (var_reg == NULL) \
				return false; \
			 \
			function_reg_t const * fn_reg = get_function(fn_id); \
			 \
			if (fn_reg == NULL) \
				return false; \
			 \
			if (var_reg->type == TYPE_USER) \
			{ \
				nfloat op_result = 0; \
				nuint size = variable_type_size(TYPE_USER); \
				byte const * data = (byte *)var_reg->location; \
				byte const * const end = data + (size * var_reg->length); \
				 \
				for (; data != end; data += size) \
				{ \
					void const * result = fn_reg->fn(data); \
					 \
					if (result == NULL) \
					{ \
						error = "User defined function returns NULL"; \
						DEBUG_PRINT("==========%s==========\n", error); \
						return false; \
					} \
					 \
					nfloat temp_result; \
					 \
					if (fn_reg->type == TYPE_INTEGER) \
						temp_result = (nfloat)*(nint const *)result; \
					else \
						temp_result = *(nfloat const *)result; \
					 \
					array_fn(&op_result, temp_result); \
				} \
				 \
				end_array_fn \
				 \
				if (!float_push_stack(op_result)) \
					return false; \
			} \
			else \
			{ \
				error = "Variable not an array of user types!"; \
				DEBUG_PRINT("==========%s==========\n", error); \
				return false; \
			} \
		} break;

static inline void array_sum_fn(float * out, float in)
{
	*out += in;
}

static inline void array_max_fn(float * out, float in)
{
	if (in > *out)
		*out = in;
}

static inline void array_min_fn(float * out, float in)
{
	if (in < *out)
		*out = in;
}



nbool evaluate(ubyte const * start, nuint program_length)
{
	ubyte const * current = start;

	while (current - start < program_length)
	{
		DEBUG_ERR_PRINT("Executing %s at %d\n", opcode_names[*current], current - start);

		// Ideally want this op codes in numerical order
		// so the compiler can generate a jump table
		switch (*current)
		{
		case HALT:
			DEBUG_ERR_PRINT("Halting\n");
			if (!require_stack_size(sizeof(nbool)))
				return false;

			return *(int *)stack_ptr;

		case IPUSH:
			DEBUG_ERR_PRINT("Pushing int %d onto the stack\n", *(nint const*)(current + 1));
			if (!int_push_stack(*(nint const*)(current + 1)))
				return false;
			current += sizeof(nint);
			break;

		case IPOP:
			if (!pop_stack(sizeof(nint)))
				return false;
			break;

		case FPUSH:
			DEBUG_ERR_PRINT("Pushing float %f onto the stack\n", *(nfloat const*)(current + 1));
			if (!float_push_stack(*(nfloat const*)(current + 1)))
				return false;
			current += sizeof(nfloat);
			break;

		case FPOP:
			if (!pop_stack(sizeof(nfloat)))
				return false;
			break;

		case IFETCH:
			{
				nint * var = get_variable_as_int(*(variable_id_t const *)(current + 1));

				if (var == NULL)
					return false;

				if (!int_push_stack(*var))
					return false;

				current += sizeof(variable_id_t);
			} break;

		case ISTORE:
			{
				if (!require_stack_size(sizeof(nint)))
					return false;

				nint * var = get_variable_as_int(*(variable_id_t const *)(current + 1));

				if (var == NULL)
					return false;
				
				*var = *(nint *)stack_ptr;

				current += sizeof(variable_id_t);
			} break;

		case FFETCH:
			{
				nfloat * var = get_variable_as_float(*(variable_id_t const *)(current + 1));

				if (var == NULL)
					return false;

				if (!float_push_stack(*var))
					return false;

				current += sizeof(variable_id_t);
			} break;

		case FSTORE:
			{
				if (!require_stack_size(sizeof(nfloat)))
					return false;

				nfloat * var = get_variable_as_float(*(variable_id_t const *)(current + 1));

				if (var == NULL)
					return false;
				
				*var = *(nfloat *)stack_ptr;

				current += sizeof(variable_id_t);
			} break;

		case AFETCH:
			{
				if (!require_stack_size(sizeof(nint)))
					return false;

				variable_reg_t * var = get_variable(*(variable_id_t const *)(current + 1));

				nint i = ((nint *)stack_ptr)[0];

				if (!pop_stack(sizeof(nint)))
					return false;

				if (!push_stack((char *)var->location + (i * variable_type_size(var->type)), variable_type_size(var->type)))
					return false;

				current += sizeof(variable_id_t);
			} break;

		case ALEN:
			{
				variable_reg_t * var = get_variable(*(variable_id_t const *)(current + 1));

				if (var == NULL)
					return false;

				if (!int_push_stack(var->length))
					return false;

				current += sizeof(variable_id_t);
			} break;

		OPERATION_ARRAY(ASUM, array_sum_fn, (void)0;)
		OPERATION_ARRAY(AMEAN, array_sum_fn, op_result /= var_reg->length;)
		OPERATION_ARRAY(AMAX, array_max_fn, (void)0;)
		OPERATION_ARRAY(AMIN, array_min_fn, (void)0;)

		case CALL:
			{
				if (!require_stack_size(data_size))
					return false;

				variable_type_t type;
				void const * data = call_function(*(function_id_t const *)(current + 1), stack_ptr, &type);
				current += sizeof(function_id_t);

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
			current = start + *(ubyte const *)(current + 1) - 1;
			DEBUG_ERR_PRINT("Jumping to %d\n", (current + 1) - start);
			break;

		case JZ:
			if (!require_stack_size(sizeof(nint)))
				return false;

			if (((nint *)stack_ptr)[0] == 0)
			{
				current = start + *(ubyte const *)(current + 1) - 1;
				DEBUG_ERR_PRINT("Jumping to %d\n", (current + 1) - start);
			}
			else
			{
				current += sizeof(ubyte);
			}

			if (!pop_stack(sizeof(nint)))
				return false;

			break;

		case JNZ:
			if (!require_stack_size(sizeof(nint)))
				return false;

			if (((nint *)stack_ptr)[0] != 0)
			{
				current = start + *(ubyte const *)(current + 1) - 1;
				DEBUG_ERR_PRINT("Jumping to %d\n", (current + 1) - start);
			}
			else
			{
				current += sizeof(ubyte);
			}

			if (!pop_stack(sizeof(nint)))
				return false;

			break;

		// Integer operations
		OPERATION_POP(IADD, +, nint, nint, "%d", 1, 0);
		OPERATION_POP(ISUB, -, nint, nint, "%d", 1, 0);
		OPERATION_POP(IMUL, *, nint, nint, "%d", 1, 0);
		OPERATION_POP(IDIV1, /, nint, nint, "%d", 0, 1);
		OPERATION_POP(IDIV2, /, nint, nint, "%d", 1, 0);

		case IINC:
			if (!require_stack_size(sizeof(nint)))
				return false;

			DEBUG_ERR_PRINT("Incrementing %d\n", ((nint *)stack_ptr)[0]);
			((nint *)stack_ptr)[0] += 1;

			break;

		case IDEC:
			if (!require_stack_size(sizeof(nint)))
				return false;

			DEBUG_ERR_PRINT("Decrementing %d\n", ((nint *)stack_ptr)[0]);
			((nint *)stack_ptr)[0] -= 1;

			break;

		OPERATION_POP(IEQ, ==, nint, nbool, "%d", 1, 0);
		OPERATION_POP(INEQ, !=, nint, nbool, "%d", 1, 0);
		OPERATION_POP(ILT, <, nint, nbool, "%d", 1, 0);
		OPERATION_POP(ILEQ, <=, nint, nbool, "%d", 1, 0);
		OPERATION_POP(IGT, >, nint, nbool, "%d", 1, 0);
		OPERATION_POP(IGEQ, >=, nint, nbool, "%d", 1, 0);

		// Floating point operations
		OPERATION_POP(FADD, +, nfloat, nfloat, "%f", 1, 0);
		OPERATION_POP(FSUB, -, nfloat, nfloat, "%f", 1, 0);
		OPERATION_POP(FMUL, *, nfloat, nfloat, "%f", 1, 0);
		OPERATION_POP(FDIV1, /, nfloat, nfloat, "%f", 0, 1);
		OPERATION_POP(FDIV2, /, nfloat, nfloat, "%f", 1, 0);
		OPERATION_POP(FEQ, ==, nfloat, nbool, "%f", 1, 0);
		OPERATION_POP(FNEQ, !=, nfloat, nbool, "%f", 1, 0);
		OPERATION_POP(FLT, <, nfloat, nbool, "%f", 1, 0);
		OPERATION_POP(FLEQ, <=, nfloat, nbool, "%f", 1, 0);
		OPERATION_POP(FGT, >, nfloat, nbool, "%f", 1, 0);
		OPERATION_POP(FGEQ, >=, nfloat, nbool, "%f", 1, 0);

		// Logical operations
		OPERATION_POP(AND, &&, nbool, nbool, "%d", 1, 0);
		OPERATION_POP(OR, ||, nbool, nbool, "%d", 1, 0);
		OPERATION_POP(XOR, ^, nbool, nbool, "%d", 1, 0);
		OPERATION_POP(EQUIVALENT, ==, nbool, nbool, "%d", 1, 0);

		//#define OPERATION_POP(code, op, type, store_type, format_type, idx1, idx2)
		case IMPLIES:
			{
				DEBUG_ERR_PRINT("Calling %s on %d and %d\n", opcode_names[*current], ((nbool *)stack_ptr)[0], ((nbool *)stack_ptr)[1]);
				if (!require_stack_size(sizeof(nbool) * 2))
					return false;
				nbool res = !((nbool *)stack_ptr)[1] || ((nbool *)stack_ptr)[0];
				if (!pop_stack(sizeof(nbool) * 2))
					return false;
				if (!push_stack(&res, sizeof(nbool)))
					return false;
			} break;

		case NOT:
			if (!require_stack_size(sizeof(nbool)))
				return false;
			((nbool *)stack_ptr)[0] = ! ((nbool *)stack_ptr)[0];
			break;

		case IVAR:
			{
				variable_id_t id = *(variable_id_t const *)(current + 1);

				if (create_variable(id, TYPE_INTEGER) == NULL)
				{
					return false;
				}

				current += sizeof(variable_id_t);
			} break;

		case FVAR:
			{
				variable_id_t id = *(variable_id_t const *)(current + 1);

				if (create_variable(id, TYPE_FLOATING) == NULL)
				{
					return false;
				}

				current += sizeof(variable_id_t);
			} break;

		case FABS:
			{
				if (!require_stack_size(sizeof(nfloat)))
					return false;

				nfloat res = abs(*(nfloat *)stack_ptr);

				if (!pop_stack(sizeof(nfloat)))
					return false;

				if (!push_stack(&res, sizeof(nfloat)))
					return false;
			} break;

		case IABS:
			{
				if (!require_stack_size(sizeof(nint)))
					return false;

				nint res = *(nint *)stack_ptr;

				if (res < 0)
					res = -res;

				if (!pop_stack(sizeof(nint)))
					return false;

				if (!push_stack(&res, sizeof(nint)))
					return false;
			} break;

		case FPOW:
			{
				/*if (!require_stack_size(sizeof(nfloat) * 2))
					return false;

				nfloat res = (nfloat)pow(((nfloat *)stack_ptr)[0], ((nfloat *)stack_ptr)[1]);

				if (!pop_stack(sizeof(nfloat) * 2))
					return false;

				if (!push_stack(&res, sizeof(nfloat)))
					return false;*/

				return false;

			} break;

		// VIINC x
		// Same as doing:
		// IFETCH x
		// IINC
		// ISTORE x
		case VIINC:
			{
				nint * var = get_variable_as_int(*(variable_id_t const *)(current + 1));

				if (var == NULL)
					return false;

				*var += 1;

				if (!int_push_stack(*var))
					return false;

				current += sizeof(variable_id_t);
			} break;

		// VIINC x
		// Same as doing:
		// IFETCH x
		// IDEC
		// ISTORE x
		case VIDEC:
			{
				nint * var = get_variable_as_int(*(variable_id_t const *)(current + 1));

				if (var == NULL)
					return false;

				*var -= 1;

				if (!int_push_stack(*var))
					return false;

				current += sizeof(variable_id_t);
			} break;

		// VIFAFC x y z
		// Same as doing:
		// IFETCH x
		// AFETCH y
		// CALL z
		case VIFAFC:
			{
				nint const * idx = get_variable_as_int(*(variable_id_t const *)(current + 1));
				current += sizeof(variable_id_t);

				if (idx == NULL)
					return false;

				variable_reg_t const * var = get_variable(*(variable_id_t const *)(current + 1));
				current += sizeof(variable_id_t);

				if (var == NULL)
					return false;

				void * inputdata = (char *)var->location + (*idx * variable_type_size(var->type));

				variable_type_t type;
				void const * data = call_function(*(function_id_t const *)(current + 1), inputdata, &type);
				current += sizeof(function_id_t);

				if (data == NULL)
				{
					return false;
				}

				if (!push_stack(data, variable_type_size(type)))
					return false;

			} break;

		default:
			DEBUG_PRINT("Unknown OP CODE %d\n", *current);
			break;
		}

		//inspect_stack(stderr);

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
	{
		error = "No user data function provided";
		DEBUG_PRINT("==========%s==========\n", error);
		return false;
	}

	if (given_data_size == 0)
	{
		error = "Zero sized user data";
		DEBUG_PRINT("==========%s==========\n", error);
		return false;
	}

	data_size = given_data_size;

	// Reset the stack and heap positions
	stack_ptr = &stack[STACK_SIZE];
	heap_ptr = stack;

	// Lets memset the stack to a certain pattern
	// This makes if obvious if we have memory issues
	memset(stack, 0xEE, STACK_SIZE);


	// Reset function and variable records
	function_regs_count = 0;
	memset(functions_regs, 0, sizeof(function_reg_t) * MAXIMUM_FUNCTIONS);

	variable_regs_count = 0;
	memset(variable_regs, 0, sizeof(variable_reg_t) * MAXIMUM_VARIABLES);


	// Put values in the `this' variable
	variable_reg_t * thisvar = create_variable(THIS_VAR_ID, TYPE_USER);
	given_data_fn(thisvar->location);

	// Reset the error message variable
	error = NULL;

	return true;
}


bool bind_input(variable_id_t id, void * data, unsigned int length)
{
	if (data == NULL)
		return false;

	if (length == 0)
		return false;

	variable_reg_t * var_array = create_array(id, TYPE_USER, length);

	if (var_array == NULL)
		return false;

	var_array->location = data;

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

#define ID_FN_ID 0
#define SLOT_FN_ID 1
#define TEMP_FN_ID 2
#define HUMIDITY_FN_ID 3

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
	variable_id_t result_id = 2;

	start_gen();

	gen_op(IVAR); gen_ubyte(result_id);

	gen_op(IPUSH); gen_int(2);
	gen_op(ISTORE); gen_ubyte(result_id);
	gen_op(IPOP);

	gen_op(IPUSH); gen_int(2);
	gen_op(IPUSH); gen_int(3);
	gen_op(IADD);

	gen_op(IFETCH); gen_ubyte(result_id);
	gen_op(IADD);

	gen_op(ICASTF);

	gen_op(FPUSH); gen_float(7.5);

	gen_op(FGT);

	gen_op(NOT);

	stop_gen();
}

static void gen_example_array_op(opcode arrayop)
{
	variable_id_t n1_id = 255;

	start_gen();

	gen_op(arrayop); gen_ubyte(n1_id); gen_ubyte(ID_FN_ID);

	//gen_op(ALEN); gen_ubyte(n1_id);

	//gen_op(ICASTF);

	//gen_op(FDIV2);

	stop_gen();
}

static void gen_example_for_loop(void)
{
	variable_id_t n1_id = 255;
	variable_id_t i_id = 2;

	start_gen();

	// Initial Code
	gen_op(IVAR); gen_ubyte(i_id);

	gen_op(FPUSH); gen_float(0);

	// Initalise loop counter
	gen_op(IPUSH); gen_int(0);
	gen_op(ISTORE); gen_ubyte(i_id);

	// Perform loop termination check
	jmp_label_t label1 = 
	gen_op(ALEN); gen_ubyte(n1_id);

	gen_op(INEQ);
	gen_op(JZ); jmp_loc_ptr_t jmp1 = gen_jmp();


	// Perform body operations
	gen_op(IFETCH); gen_ubyte(i_id);

	gen_op(AFETCH); gen_ubyte(n1_id);
	gen_op(CALL); gen_ubyte(SLOT_FN_ID);
	gen_op(ICASTF);

	gen_op(FADD);

	gen_op(FPUSH); gen_float(2);

	gen_op(FDIV2);

	// Increment loop counter
	gen_op(IFETCH); gen_ubyte(i_id);
	gen_op(IINC);
	gen_op(ISTORE); gen_ubyte(i_id);


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
nint load_file_to_memory(char const * filename, ubyte ** result) 
{
	if (filename == NULL || result == NULL)
	{
		printf("Invalid parameters\n");
		return -4;
	}

	FILE * f = fopen(filename, "rb");

	if (f == NULL)
	{
		printf("Failed to open file\n");
		*result = NULL;
		return -1; // -1 means file opening fail
	}

	fseek(f, 0, SEEK_END);
	nint size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (size == 0)
	{
		printf("File size is zero\n");
		fclose(f);
		return -4;
	}

	*result = (ubyte *)heap_alloc(size);

	if (*result == NULL)
	{
		printf("Failed to allocate enough space (%d) for program\n", size);
		fclose(f);
		return -3;
	}

	if (size != fread(*result, sizeof(ubyte), size, f))
	{
		printf("Reading input file failed\n");
		fclose(f);
		return -2; // -2 means file reading fail
	}

	fclose(f);
	return size;
}

static bool run_program_from_file(int argc, char ** argv)
{
	if (argv == NULL || argc != 2)
	{
		printf("Invalid arguments\n");
		return false;
	}

	char const * filename = argv[1];

#ifndef NDEBUG
	fprintf(stderr, "Filename: %s\n", filename);
#endif

	nint program_size = load_file_to_memory(filename, &program_start);

	if (program_size <= 0)
	{
		printf("No program to execute\n");
		return false;
	}

	program_end = program_start + program_size;

	return true;
}

int main(int argc, char * argv[])
{
	init_pred_lang(&local_node_data_fn, sizeof(user_data_t));

	// Register the data functions 
	register_function(ID_FN_ID, &get_id_fn, TYPE_INTEGER);
	register_function(SLOT_FN_ID, &get_slot_fn, TYPE_INTEGER);
	register_function(TEMP_FN_ID, &get_temp_fn, TYPE_FLOATING);
	register_function(HUMIDITY_FN_ID, &get_humidity_fn, TYPE_FLOATING);

	user_data_t * data = (user_data_t *)malloc(sizeof(user_data_t) * 20);

	// N(1)
	set_user_data(&data[0], 0, 1, 25, 122);
	set_user_data(&data[1], 1, 3, 26, 122);
	set_user_data(&data[2], 2, 5, 27, 122);
	set_user_data(&data[3], 3, 7, 26, 122);
	set_user_data(&data[4], 4, 9, 25, 122);
	set_user_data(&data[5], 5, 11, 26, 122);
	set_user_data(&data[6], 6, 13, 27, 122);
	set_user_data(&data[7], 7, 15, 26, 122);
	set_user_data(&data[8], 8, 17, 25, 122);
	set_user_data(&data[9], 9, 19, 26, 122);

	// N(2)
	set_user_data(&data[10], 10, 2, 25, 122);
	set_user_data(&data[11], 11, 4, 26, 122);
	set_user_data(&data[12], 12, 6, 27, 122);
	set_user_data(&data[13], 13, 8, 26, 122);
	set_user_data(&data[14], 14, 10, 25, 122);
	set_user_data(&data[15], 15, 12, 26, 122);
	set_user_data(&data[16], 16, 14, 27, 122);
	set_user_data(&data[17], 17, 16, 26, 122);
	set_user_data(&data[18], 18, 18, 25, 122);
	set_user_data(&data[19], 19, 20, 26, 122);

	bind_input(255, data, 10);
	bind_input(254, data, 20);

#ifndef NDEBUG
	fprintf(stderr, "sizeof(void *): %u\n", sizeof(void *));
	fprintf(stderr, "sizeof(int): %u\n", sizeof(nint));
	fprintf(stderr, "sizeof(float): %u\n", sizeof(nfloat));
	fprintf(stderr, "sizeof(variable_reg_t): %u\n", sizeof(variable_reg_t));
	fprintf(stderr, "sizeof(function_reg_t): %u\n", sizeof(function_reg_t));
#endif

	if (!run_program_from_file(argc, argv))
	{
		return 1;
	}

	// Load a program into memory
	//gen_example_for_loop();
	//gen_example_array_op(AMEAN);

	unsigned program_length = (unsigned)(program_end - program_start);

	printf("Program length %u\n", program_length);

	// Evaluate the program
	nbool result = evaluate(program_start, program_end - program_start);
	

	// Print the results
	printf("Stack ptr value (float %f) (int %d) (bool %d)\n",
		*((nfloat *)stack_ptr), *((nint *)stack_ptr), *((nbool *)stack_ptr) != 0
	);

	printf("Result: %d\n", result);

	inspect_stack(stdout);

	free(data);

	return 0;
}
#endif

