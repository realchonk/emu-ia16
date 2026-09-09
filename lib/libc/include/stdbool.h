#ifndef __bool_true_false_are_defined

# if __GNUC__
#  include_next <stdbool.h>
# else
typedef int bool;
#  define false 0
#  define true
#  define __bool_true_false_are_defined 1
# endif
#endif
