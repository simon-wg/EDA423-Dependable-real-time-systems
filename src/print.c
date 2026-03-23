#include "sciTinyTimber.h"
#include <stdarg.h>
#include <stdio.h>

#define BUFFER_SIZE 1024

extern Serial sci0;

void print(const char *fmt, ...) {
  char buffer[BUFFER_SIZE];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buffer, BUFFER_SIZE, fmt, ap);
  va_end(ap);
  SCI_WRITE(&sci0, buffer);
}
