#ifndef XBIND_EXAMPLES_HELLO_H
#define XBIND_EXAMPLES_HELLO_H

#ifdef __cplusplus
extern "C" {
#endif

int hello_add(int left, int right);
double hello_scale(double value, double factor);
const char *hello_greet(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* XBIND_EXAMPLES_HELLO_H */
