
#ifndef Q7_H
#define Q7_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void q7_init(void);
void q7_run(void);

void serial_print(const char *text);
int serial_receive_available(void);
int serial_receive_read(void);

#ifdef __cplusplus
}
#endif

#endif
