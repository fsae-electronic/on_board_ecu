#ifndef SOURCE_TEST_DATA_H_
#define SOURCE_TEST_DATA_H_

#include <stdbool.h>
#include "dashboard.h"

void test_data_set_enabled(bool enabled);
bool test_data_is_enabled(void);
void test_data_toggle(void);
void test_data_update(dashboard_data_t *data);

#endif
