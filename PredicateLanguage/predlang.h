#ifndef CS407_PRED_LANG_H
#define CS407_PRED_LANG_H

#include <stdint.h>

#ifndef _MSC_VER
#	include <stdbool.h>
#else
#	define snprintf _snprintf
#endif

typedef enum
{
	TYPE_INTEGER = 0,
	TYPE_FLOATING = 1,
	TYPE_USER = 2
	
} variable_type_t;


typedef signed char byte;
typedef unsigned char ubyte;
typedef int16_t nint;
typedef uint16_t nuint;
typedef float nfloat;

// Please keep the boolean type the same size
// as the integer size. Otherwise JZ and JNZ
// will cause issues with the stack.
typedef nint nbool;


typedef ubyte function_id_t;
typedef ubyte variable_id_t;


typedef void const * (*data_access_fn)(void const * ptr);
bool register_function(function_id_t id, data_access_fn fn, variable_type_t type);

typedef void (*node_data_fn)(void *);
bool init_pred_lang(node_data_fn given_data_fn, nuint given_data_size);

// Creates an array with the given data
// This data must be a non-null array of length: sizeof(TYPE_USER) * length
// The id must be the id the bytecode expects the array to be.
// The memory provided must not be free'd until evaluate() has finished
bool bind_input(variable_id_t id, void const * data, unsigned int length);

nbool evaluate(ubyte * start, nuint program_length);

char const * error_message(void);

#endif /*CS407_PRED_LANG_H*/

