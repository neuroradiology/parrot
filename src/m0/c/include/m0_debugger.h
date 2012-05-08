#ifndef M0_DEBUGGER_H


typedef enum {
    STEP,
    RUN,
    BREAK
} M0_Debugger_State;

typedef struct {
    M0_Debugger_State state;
    unsigned long *breakpoints;
    unsigned int n_breakpoints;
} M0_Debugger_Info;

void debugger(M0_CallFrame *cf, const unsigned char *ops, const unsigned long pc);


#   define M0_DEBUGGER_H 1
#endif

/* vim: expandtab shiftwidth=4 cinoptions='\:2=2' :
*/