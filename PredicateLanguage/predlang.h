#ifndef CS407_PRED_LANG_H
#define CS407_PRED_LANG_H

#include <stdint.h>
#include <stdbool.h>

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


typedef void const * (*data_access_fn)(void const * ptr);
bool register_function(char const * name, data_access_fn fn, variable_type_t type);

typedef void (*node_data_fn)(void *);
bool init_pred_lang(node_data_fn given_data_fn, nuint given_data_size);

nbool evaluate(ubyte * start, nuint program_length);

char const * error_message(void);

#endif /*CS407_PRED_LANG_H*/

