#include <vitasdk.h>
#include "debugScreen.h"

#define printf psvDebugScreenPrintf


int simple_greeting(int in){
	return in*2;
}
int module_stop(SceSize args, const void *argp){
	sceKernelDelayThread(1000);
	return 0;
}

int module_exit(){
	return 42;
}
void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize args, void *argp){
	SceDisplayFrameBuf frame = {sizeof(frame)};
	sceDisplayGetFrameBuf(&frame, SCE_DISPLAY_SETBUF_NEXTFRAME);
	base = frame.base;
	printf("Base=%08X\nok", base);
	return SCE_KERNEL_START_SUCCESS;
}

