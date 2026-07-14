#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4244) // conversion from 'type1' to 'type2', possible loss of data
    #pragma warning(disable: 4100) // unreferenced formal parameter
    #pragma warning(disable: 4189) // local variable is initialized but not referenced
    #pragma warning(disable: 4245) // conversion from 'type1' to 'type2', signed/unsigned mismatch
    #pragma warning(disable: 4310) // cast truncates constant value
    #pragma warning(disable: 4389) // signed/unsigned mismatch
    #pragma warning(disable: 4701) // potentially uninitialized local variable used
    #pragma warning(disable: 4706) // assignment within conditional expression
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
