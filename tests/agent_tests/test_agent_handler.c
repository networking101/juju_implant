#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include "agent_handler.h"


static void test(void **state) {
    (void) state; /* unused */
}


int test_agent_handler(){
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test),
	};
	
	return cmocka_run_group_tests(tests, NULL, NULL);
}