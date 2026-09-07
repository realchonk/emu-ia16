#ifndef FILE__VARARGS_H
#define FILE__VARARGS_H

typedef char *va_list;
#define va_start(ap, a) (ap = (char *)(&a + 1))
#define va_end(ap)
#define va_arg(ap, T) ((T *)(ap += sizeof (T)))[-1]

#endif /* FILE__VARARGS_H */
