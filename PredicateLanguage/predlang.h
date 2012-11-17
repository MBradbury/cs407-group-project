#ifndef CS407_PRED_LANG_H
#define CS407_PRED_LANG_H

#include <stdint.h>

typedef enum
{
	TYPE_INTEGER = 0,
	TYPE_FLOATING = 1,
	TYPE_USER = 2
	
} variable_type_t;


typedef unsigned char ubyte;
typedef int16_t nint;
typedef uint16_t nuint;
typedef float nfloat;



typedef void const * (*data_access_fn)(void const * ptr);

int register_function(char const * name, data_access_fn fn, variable_type_t type);



typedef void * (*node_data_fn)(void);

void init_pred_lang(node_data_fn given_data_fn, nuint given_data_size);



char const * error_message(void);
void reset_error(void);


#endif /*CS407_PRED_LANG_H*/
