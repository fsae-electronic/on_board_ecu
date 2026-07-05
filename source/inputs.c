#include "inputs.h"
#include "gio.h"

uint8_t *input_targets[4] = {0};
static uint8_t last_input_states[4] = {0};


void init_inputs(void)
{
    uint32_t dirB = 0U;

    dirB |= (1U << 0);   // GIOB[0] -> Button 1
    dirB |= (1U << 1);   // GIOB[1] -> Button 2
    dirB |= (1U << 2);   // GIOB[2] -> Switch 1
    dirB |= (1U << 3);   // GIOB[3] -> Switch 2

    gioSetDirection(gioPORTB, dirB);
}


bool read_inputs(input_source_t source)
{
    switch(source)
    {
        case INPUT_0:
            return (bool)gioGetBit(gioPORTB, 0); // Button 1
        case INPUT_1:
            return (bool)gioGetBit(gioPORTB, 1); // Button 2
        case INPUT_2:
            return (bool)gioGetBit(gioPORTB, 2); // Switch 1
        case INPUT_3:
            return (bool)gioGetBit(gioPORTB, 3); // Switch 2
        default:
            return false;
    }
}

void update_inputs(void)
{
    // Toggle the linked variable only on a low->high transition.
    for (int i = 0; i < 4; i++)
    {
        uint8_t current_state = (uint8_t)read_inputs((input_source_t)i);

        if (input_targets[i] != 0)
        {
            if (current_state && !last_input_states[i])
            {
                *input_targets[i] = (uint8_t)!(*input_targets[i]);
            }
        }

        last_input_states[i] = current_state;
    }
}

void connect_input(input_source_t source, uint8_t *target_variable)
{
    // This function can be used to link an input source to a variable in the dashboard data structure
    // For example, you could set up a mapping from buttons to drive states or modes
    // This is just a placeholder and would need to be implemented based on the specific requirements of your application
    if (source < 4)
    {
        input_targets[source] = target_variable;
        last_input_states[source] = (uint8_t)read_inputs(source);
    }
}
