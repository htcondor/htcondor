#ifndef CONDOR_SYSCALL_MACROS_H
#define CONDOR_SYSCALL_MACROS_H

#define REMAP_ZERO(old,new,type_return) \
type_return new() \
{ \
return old(); \
}

#define REMAP_ONE(old,new,type_return,type_1) \
type_return new( arg_1 ) \
type_1 arg_1; \
{ \
return old( arg_1 ); \
}

#define REMAP_TWO(old,new,type_return,type_1,type_2) \
type_return new( arg_1, arg_2 ) \
type_1 arg_1; \
type_2 arg_2; \
{ \
return old( arg_1, arg_2 ); \
}

#define REMAP_THREE(old,new,type_return,type_1,type_2,type_3) \
type_return new( arg_1, arg_2, arg_3 ) \
type_1 arg_1; \
type_2 arg_2; \
type_3 arg_3; \
{ \
return old( arg_1, arg_2, arg_3 ); \
}

#define REMAP_FOUR(old,new,type_return,type_1,type_2,type_3,type_4) \
type_return new( arg_1, arg_2, arg_3, arg_4 ) \
type_1 arg_1; \
type_2 arg_2; \
type_3 arg_3; \
type_4 arg_4; \
{ \
return old( arg_1, arg_2, arg_3, arg_4 ); \
}

#define REMAP_FIVE(old,new,type_return,type_1,type_2,type_3,type_4,type_5) \
type_return new( arg_1, arg_2, arg_3, arg_4, arg_5 ) \
type_1 arg_1; \
type_2 arg_2; \
type_3 arg_3; \
type_4 arg_4; \
type_5 arg_5; \
{ \
return old( arg_1, arg_2, arg_3, arg_4, arg_5 ); \
}

#define REMAP_SIX(old,new,type_return,type_1,type_2,type_3,type_4,type_5,type_6) \
type_return new( arg_1, arg_2, arg_3, arg_4, arg_5, arg_6 ) \
type_1 arg_1; \
type_2 arg_2; \
type_3 arg_3; \
type_4 arg_4; \
type_5 arg_5; \
type_6 arg_6; \
{ \
return old( arg_1, arg_2, arg_3, arg_4, arg_5, arg_6 ); \
}

#endif /* CONDOR_SYSCALL_MACROS_H */
