#include <assert.h>
#include <tcl.h>
#include <tk.h>
#include <stdlib.h>

#include "queue.h"
#include "reflector.h"

extern char tclscript[];

static Tcl_Interp *interp;
static struct queue_s *c;
static int exit_now = 0;

/*****************************************************************************/
/* Tcl functions implemented in C                                            */
static int 
ui_exit(ClientData clientData,
        Tcl_Interp *interp,
        int argc,
        char *argv[])
{
        Tcl_DeleteInterp(interp);
        interp = NULL;

        exit_now = 1;
}


static int
get_ports(ClientData clientData,
              Tcl_Interp *interp,
              int argc,
              char *argv[])
{
    char msg[255];
    channel_t *chan;
    int i,n;

    memset(msg,0,255);
    n = queue_len(c);
    Tcl_SetResult(interp,NULL,TCL_STATIC);
    for(i=0;i<n;i++) {
        chan = (channel_t*)get_item_no(c,i,Q_KEEP);
        sprintf(msg,"%d",chan->port);
        Tcl_AppendElement(interp,msg);
    }
    return TCL_OK;
}

static int
query_engine(ClientData clientData,
             Tcl_Interp *interp,
             int argc,
             char *argv[])
{
    char var[255];
    int port;
    channel_t *chan;

    assert(argc>0);
    port = atoi(argv[argc-1]);
    chan = (channel_t*)get_matching(c,(char*)&port,Q_KEEP);

    sprintf(var,"%d",chan->port);
    Tcl_SetVar2(interp,"c","port", var, TCL_GLOBAL_ONLY);
    sprintf(var,"%.4f",chan->loss*100);
    Tcl_SetVar2(interp,"c","loss", var, TCL_GLOBAL_ONLY);
    sprintf(var,"%d",chan->min_delay);
    Tcl_SetVar2(interp,"c","min_delay", var, TCL_GLOBAL_ONLY);
    sprintf(var,"%d",chan->max_delay);
    Tcl_SetVar2(interp,"c","max_delay", var, TCL_GLOBAL_ONLY);
    sprintf(var,"%.4f", chan->dup_pr * 100);
    Tcl_SetVar2(interp,"c","dup_pr", var, TCL_GLOBAL_ONLY);
    Tcl_SetResult(interp,NULL,TCL_STATIC);
    
    return TCL_OK;
}

static int 
update_engine(ClientData clientData,
              Tcl_Interp *interp,
              int argc,
              char *argv[])
{
    char *c_port, *c_loss, *c_min_delay, *c_max_delay, *c_dup_pr;
    channel_t *chan;
    char var[255];
    int  port;
    int  tmp;

    c_port = Tcl_GetVar2(interp,"c","port",TCL_GLOBAL_ONLY);
    if (!c_port) {
        Tcl_SetResult(interp, "Could not read global variable c", TCL_STATIC);
        return TCL_ERROR;
    }

    c_loss = Tcl_GetVar2(interp,"c","loss",TCL_GLOBAL_ONLY);
    if (!c_loss) {
        Tcl_SetResult(interp, "Could not read global variable c", TCL_STATIC);
        return TCL_ERROR;
    }

    c_min_delay = Tcl_GetVar2(interp,"c","min_delay",TCL_GLOBAL_ONLY);
    if (!c_min_delay) {
        Tcl_SetResult(interp, "Could not read global variable c", TCL_STATIC);
        return TCL_ERROR;
    }

    c_max_delay = Tcl_GetVar2(interp,"c","max_delay",TCL_GLOBAL_ONLY);
    if (!c_max_delay) {
        Tcl_SetResult(interp, "Could not read global variable c", NULL);
        return TCL_ERROR;
    }

    c_dup_pr = Tcl_GetVar2(interp,"c","dup_pr",TCL_GLOBAL_ONLY);
    if (!c_dup_pr) {
        Tcl_SetResult(interp, "Could not read global variable c", NULL);
        return TCL_ERROR;
    }

    port = atoi(c_port);
    if (!(chan=(channel_t*)get_matching(c,(char*)&port,Q_KEEP))) {
        sprintf(var,"Could not find port %d",port);
        Tcl_SetResult(interp, var, TCL_STATIC);
        return TCL_ERROR;
    }

    chan->loss=      strtod(c_loss,NULL)/100.0;
    chan->min_delay= atoi(c_min_delay);
    chan->max_delay= atoi(c_max_delay);
    chan->dup_pr   = strtod(c_dup_pr, NULL)/100.0;

    if (chan->min_delay>chan->max_delay) {
        tmp = chan->min_delay;
        chan->min_delay = chan->max_delay;
        chan->max_delay = tmp;
    }

    if (chan->loss>1.0||chan->loss<0) {
        Tcl_SetResult(interp,"Loss rate outside range 0-100",TCL_STATIC);
        return TCL_ERROR;
    }

    if (chan->min_delay<0) chan->min_delay = 0;
    if (chan->max_delay<0) chan->max_delay = 0;

    query_engine(NULL,interp,1,&c_port);

    return TCL_OK;
}


/*****************************************************************************/
/* ui admin functions                                                        */
int
init_ui()
{
    interp = Tcl_CreateInterp();
 
    if (Tcl_Init(interp) == TCL_ERROR) {
        printf("Failed to initialise Tcl interpreter:\n%s\n",
               (interp)->result);
        return TCL_ERROR;
    }

    if (Tk_Init(interp)  == TCL_ERROR) {
        printf("Failed to initialise Tk package:\n%s\n", 
               (interp)->result);
        return TCL_ERROR;
    }
       
    Tcl_StaticPackage(interp, "Tk", Tk_Init, (Tcl_PackageInitProc *) NULL);

    Tcl_CreateCommand(interp, "get_ports", get_ports, NULL, NULL);
    Tcl_CreateCommand(interp, "update_engine", update_engine, NULL, NULL);
    Tcl_CreateCommand(interp, "query_engine", query_engine, NULL, NULL);
    Tcl_CreateCommand(interp, "ui_exit", ui_exit, NULL, NULL);

    if (Tcl_Eval(interp, &tclscript[0]) != TCL_OK) {
        printf("Failed to run tcl command, error: %s\n", (interp)->result);
        return 0;
    }

    return 1;
}

int 
channels_to_ui(struct queue_s *q)
{
    c = q;
    return queue_len(c);
}

int
process_ui()
{
    while(Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT) != 0) {}
    return (!exit_now);
}

void ui_update()
{
        Tcl_Eval(interp, "update_from_remote");
}



