#include "inputs.h"
#include "gio.h"

uint8_t *input_targets[4] = {0};


void init_inputs(void)
{
    // Initialize GPIO pins for buttons, switches, etc. as needed
    // For example, if using gio:
    gioInit();

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
    // This function can be called periodically to read the state of the inputs and update the linked variables
    for (int i = 0; i < 4; i++)
    {
        if (input_targets[i] != 0)
        {
            *input_targets[i] = (uint8_t)read_inputs((input_source_t)i);
        }
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
    }
}
