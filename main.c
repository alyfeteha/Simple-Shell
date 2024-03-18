#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ARG_MAX     1024
#define cint8_t     char
#define ccint8_t    const char
#define iswhtspc(c) ((isspace(c)) || (isblank(c)) || (c =='\n'))


typedef struct pid_stack{
    size_t size;
    size_t capacity;
    pid_t *arr;
}pid_stack;

pid_stack * pid_stack_init(void){
    pid_stack *s = malloc(sizeof(pid_stack));
    s->capacity  = 10;
    s->size      = 0;
    s->arr       = calloc(s->capacity,sizeof(pid_t));
    return s;
}

void pid_stack_dst(pid_stack *s){
    free(s->arr);
    free(s);
}

void push(pid_stack *s, pid_t p){
    if(s->size*2 > s->capacity)
        s->arr = realloc(s->arr,(s->capacity*=2)*sizeof(s->arr[0]));
    s->arr[s->size++] = p;
}

pid_t pop(pid_stack *s){
    if(s->size > 0 )
        return s->arr[--s->size];
    return -1;
}

__attribute__((always_inline)) void strim(ccint8_t *);
void proc_exit_hndlr(int32_t);
void cleanup(void);

FILE      *LOG_FILE;
pid_stack *PID_STACK;

int main(void) {
    cleanup();
    LOG_FILE = fopen("LOG.txt","a");
    PID_STACK = pid_stack_init();
    if(LOG_FILE == NULL){
        fprintf(stderr,"EXCEPTION: Failed to obtain handler to log file. Redirecting to STDERR.");
        LOG_FILE = stderr;
    }
    signal (SIGCHLD, proc_exit_hndlr);
    atexit(cleanup);

    while(true){
        ccint8_t line[ARG_MAX] = "", *pargs[ARG_MAX/2];
        memset(pargs,0, sizeof(pargs[0]) * (ARG_MAX/2));
        size_t nargs = 0;

        printf("Shell~>");
        if (!fgets((cint8_t *)line, ARG_MAX, stdin)) continue;
        strim(line);
        if (strlen(line) == 0) continue;
        ccint8_t* tkn =  pargs[0] =  strtok((cint8_t *)line," ");
        if(!pargs[0]) continue;
        while ((tkn = strtok(NULL, " ")) != NULL) pargs[++nargs] = tkn;

        if (pargs[0] && !strcmp(pargs[0], "exit")) exit(EXIT_SUCCESS);

        pid_t cpid  ;
        int   wstate;
        if ((cpid = fork()) == -1) {
            perror("ERROR: Failed to fork.\n");
            continue;
        }

        if (nargs && *pargs[nargs] == '&' ){
            *((cint8_t*)pargs[nargs]) = '\0';
            pargs[nargs--]            = NULL;
        } else {
            if (cpid > 0){
                push(PID_STACK,cpid);
                waitpid(cpid,&wstate,WCONTINUED | WUNTRACED);
            }
        }

        if(cpid == 0) {
            if (execvp( pargs[0], (char * const*)pargs) == -1){
                perror("ERROR: Failed to run execute program in shell.\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

/**
 * A String function that removes trailing whitespace
 * inplace by looping over the string replacing right trailing space with
 * @param s pointer of the string to trim.
 */
__attribute__((always_inline))
void strim(ccint8_t *s){
    if( !s || !*s) return;
    int8_t *p   = (int8_t*)s;
    size_t len  = strlen( (ccint8_t *)p);
    while(len && iswhtspc(((uint8_t)p[len - 1]))) p[--len] = '\0';
    while(*p && iswhtspc(((uint8_t)*p))) ++p,--len;
    memmove((void*)s,p,(len + 1) *sizeof(*s));
}

/**
 * A non-blocking SIG_CHILD handler function that logs the exit code of a child process.
 * @param sig signum provided by the callee, when the signal linked with this function occurs.
 */
void proc_exit_hndlr(int32_t sig) {
    if(PID_STACK->size > 0)
        fprintf(LOG_FILE,"Child Process of PID %d exited.\n", pop(PID_STACK));
}

/**
 * Cleanup function that gets called at the exit of the main shell process.
 * it is first initialized on the first call.
 */
void cleanup(void){
    static pid_t  shllpid = 0 ;
    if(shllpid == 0){
        shllpid  = getpid();
        return;
    }
    if(getpid() == shllpid){
        if(LOG_FILE !=NULL ){
            fclose(LOG_FILE);
            LOG_FILE = NULL;
            pid_stack_dst(PID_STACK);
            PID_STACK = NULL;
        }else{
            fprintf(stderr,"ERROR: No log file. This was not supposed to happen.");
        }
    }
}
