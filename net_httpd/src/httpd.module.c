#ifndef __vita__
#define sceKernelLoadModule(       path,             flags, opt        ) printf("Load"   " %s "     "%i\n", path,          flags);
#define sceKernelLoadStartModule(  path, args, argp, flags, opt, status) printf("LoadStart %s   %.*s %i\n", path,args,argp,flags);
#define sceKernelStartModule(     modid, args, argp, flags, opt, status) printf(    "Start %08X %.*s %i\n",modid,args,argp,flags);
#define sceKernelStopModule(      modid, args, argp, flags, opt, status) printf("Stop"   " %08X %.*s %i\n",modid,args,argp,flags);
#define sceKernelUnloadModule(    modid,             flags, opt        ) printf(    "Unlad %08X "   "%i\n",modid,          flags);
#define sceKernelStopUnloadModule(modid, args, argp, flags, opt, status) printf("StopUnlad %08X %.*s %i\n",modid,args,argp,flags);
#endif

void module_get(int s, char*url, char*path, char**headers, char**params, char**data) {
	sendall(s, $(((char*[]){HTML_HDR,
	"<form method=post>"
		"<input type=submit name=action value=Load>"
		"<input type=submit name=action value=LoadStart>"
		"<input name=path placeholder='ux0:example.self'>"
	"</form>"
	"<h1>Module List:</h1><form method=post><pre>"})));
	#define FMT "<input type=radio name=modid value=0x%08lX>0x%08lX %08lX %-16s <a href=\"/file/%s\">%s</a>\n"
	char line[1024];
#ifdef __vita__
	SceUID modids[32];
	int num = sizeof(modids)/sizeof(*modids);
	int ret = sceKernelGetModuleList(0x80|0x1, modids, &num);
	for(int n = 0; n < num; n++){
		SceKernelModuleInfo mi;
		ret = sceKernelGetModuleInfo(modids[n],&mi);
		sprintf(line, FMT, modids[n], modids[n], mi.flags, mi.module_name, mi.path, mi.path);
		sendall(s, $(((char*[]){line})));
	}
#else
	for(long unsigned n = 0; n < 10; n++){
		sprintf(line, FMT, n, n, 0LU, "myExample", "/", "ux0:example.suprx");
		sendall(s, $(((char*[]){line})));
	}
#endif
	sendall(s, $(((char*[]){
		"<input type=submit name=action value=Start>",
		"<input type=submit name=action value=Stop>",
		"<input type=submit name=action value=Unload>",
		"<input type=submit name=action value=StopUnload>",
		"</pre></form>"})));
} 
void module_post(int s, char*url, char*path, char**headers, char**params, char**data){
	int ret = -1, status = 0;
	char*action = valueof(data,"action");
	int modid = strtol(valueof(data,"modid")?:"0",NULL,0);
	int flags = strtol(valueof(data,"flags")?:"0",NULL,0);
	char*args = unescape(valueof(data,"args"));
	int argsz = strlen(args);
	char*fpath = unescape(valueof(data,"path"));
	if(!strcasecmp(action,"Load"      ))ret = sceKernelLoadModule      (fpath,              flags, NULL         );
	if(!strcasecmp(action,"LoadStart" ))ret = sceKernelLoadStartModule (fpath, argsz, args, flags, NULL, &status);
	if(!strcasecmp(action,"Start"     ))ret = sceKernelStartModule     (modid, argsz, args, flags, NULL, &status);
	if(!strcasecmp(action,"Stop"      ))ret = sceKernelStopModule      (modid, argsz, args, flags, NULL, &status);
	if(!strcasecmp(action,"Unload"    ))ret = sceKernelUnloadModule    (modid,              flags, NULL         );
	if(!strcasecmp(action,"StopUnload"))ret = sceKernelStopUnloadModule(modid, argsz, args, flags, NULL, &status);
	char line[1024];
	if(ret < 0)
		snprintf(line, sizeof(line), HTTP_HDR("500 0x%08X","text/plain") "Failed to %s 0x%08X %s: 0x%08X", ret,action,modid,fpath, ret);
	else
		snprintf(line, sizeof(line), HTTP_HDR("200 %i","text/plain") "0x%08X", status, ret);
	sendall(s, $(((char*[]){line})));
}