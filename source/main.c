#include <pspkernel.h>
#include <pspctrl.h>
#include <string.h>
#include <unistd.h>
#include "utils.h"
#include "ftpsp.h"

#define MOD_NAME   "ftpsp"
PSP_MODULE_INFO(MOD_NAME, PSP_MODULE_USER, 1, 1);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(20480);

int run = 1;
int exit_callback(int arg1, int arg2, void *common);
int CallbackThread(SceSize args, void *argp);
int SetupCallbacks(void);

void list_netconfigs();
int select_netconfig();

int main(int argc, char **argv)
{
    SetupCallbacks();
    pspDebugScreenInit();
    printf("ftpsp by xerpi\n\n");
    
    load_net_modules();
    init_net();
    connect_net(3);
    
    //int config_n = select_netconfig();
    //cls();
    
    ftpsp_init();


    SceCtrlData pad; pad.Buttons = 0;
    while (run) {
        sceCtrlPeekBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_TRIANGLE) run = 0;
        sceKernelDelayThread(100*1000);
    }
    
    printf("Exiting...\n");
    ftpsp_shutdown();
    deinit_net();
    unload_net_modules();
    sceKernelExitGame();
    return 0;
}


/* Exit callback */
int exit_callback(int arg1, int arg2, void *common)
{
    run = 0;
    return 0;
}

/* Callback thread */
int CallbackThread(SceSize args, void *argp)
{
    int cbid;
    cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

/* Sets up the callback thread and returns its thread id */
int SetupCallbacks(void)
{
    int thid = 0;
    thid = sceKernelCreateThread("update_thread", CallbackThread,
             0x11, 0xFA0, PSP_THREAD_ATTR_USER, 0);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, 0);
    }
    return thid;
}
