#include "gio.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef enum
{
    INPUT_0,    //GIOB[0]
    INPUT_1,    //GIOB[1]
    INPUT_2,    //GIOB[2]
    INPUT_3     //GIOB[3]
} input_source_t;



void init_inputs(void);
void connect_input(input_source_t source, uint8_t *target_variable);
void update_inputs(void);

#ifdef __cplusplus
}
#endif
