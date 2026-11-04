#ifndef Q5_H
#define Q5_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void serial_print(const char *text);
void serial_print_number(uint32_t value);

void q5_init(void);
void q5_run(void);

#ifdef __cplusplus
}
#endif

#endif