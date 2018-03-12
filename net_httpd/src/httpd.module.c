void module_get(int s, Request req) {
	int h = !!strstr(valueof(req.headers, "Accept"), "html");
	if(h)sendall(s, $(((char*[]){HTML_HDR,
	"<pre>", valueof(req.params,"message"), "</pre>",
	"<h1>Module List:</h1><form method=post>"
		"<input name=action type=submit value=Load>"
		"<input name=action type=submit value=LoadStart>"
		"<input name=path   placeholder=path>"
		"<input name=action type=submit value=Start>"
		"<input name=args   placeholder=args>"
		"<input name=action type=submit value=Stop>"
		"<input name=action type=submit value=Unload>"
		"<input name=action type=submit value=StopUnload>"
	"<pre>"})));
	else sendall(s, $(((char*[]){ TEXT_HDR })));

	#define FMT_HTML "<input type=radio name=modid value=0x%08lX>0x%08lX %08lX %-16s <a href=\"/file/%s\">%s</a>\n"
	#define FMT_TXT  "%.lu%#08lx %08lX %-18s %s\n"
	char line[1024];
#ifdef __vita__
	SceUID modids[32];
	int num = sizeof(modids)/sizeof(*modids);
	int ret = sceKernelGetModuleList(0x80|0x1, modids, &num);
	for(int n = 0; n < num; n++) {
		SceKernelModuleInfo mi;
		ret = sceKernelGetModuleInfo(modids[n],&mi);
		sprintf(line, h?FMT_HTML:FMT_TXT, h?modids[n]:0, modids[n], mi.flags, mi.module_name, mi.path, mi.path);
		sendall(s, $(((char*[]){line})));
	}
#else
	for(long unsigned n = 1; n < 16; n++) {
		sprintf(line, h?FMT_HTML:FMT_TXT, h?n:0, n, 0LU, "myExample", "ux0:example.suprx", "ux0:example.suprx");
		sendall(s, $(((char*[]){line})));
	}
#endif
	if(h)sendall(s, $(((char*[]){"</pre></form>"})));
} 
void module_post(int s, Request req) {
	int ret = -1, status = 0;
	char*o = valueof(req.formdata,"o");//alias fot action (alow us to curl -do=Load)
	char*action = *o ? o : valueof(req.formdata,"action");
	int modid = strtol(valueof(req.formdata,"modid")?:"0",NULL,0);
	int flags = strtol(valueof(req.formdata,"flags")?:"0x01010000",NULL,0);
	char*fpath = unescape(valueof(req.formdata,"path"));
	char*argp = unescape(valueof(req.formdata,"args"));
	int  args = strlen(argp)+1;// args must include the NUL delimiter
	debug("%s %#.X%s (%.*s) ... ", action, modid, fpath, args, argp);
	#ifdef __vita__
	if(!strcasecmp(action,"Load"      ))ret = sceKernelLoadModule      (fpath,             flags, NULL         );
	if(!strcasecmp(action,"LoadStart" ))ret = sceKernelLoadStartModule (fpath, args, argp, flags, NULL, &status);
	if(!strcasecmp(action,"Start"     ))ret = sceKernelStartModule     (modid, args, argp, flags, NULL, &status);
	if(!strcasecmp(action,"Stop"      ))ret = sceKernelStopModule      (modid, args, argp, flags, NULL, &status);
	if(!strcasecmp(action,"Unload"    ))ret = sceKernelUnloadModule    (modid,             flags, NULL         );
	if(!strcasecmp(action,"StopUnload"))ret = sceKernelStopUnloadModule(modid, args, argp, flags, NULL, &status);
	#endif
	debug("0x%08X\n", ret);
	char head[1024],body[1024];
	if(ret < 0) {
		snprintf(head, sizeof(head), HTTP_HDR("500 0x%08X","text/plain") , ret);
		snprintf(body, sizeof(body), "Failed to %s %#.x%s args(%.*s) [%08X]: 0x%08X", action, modid, fpath, args, argp, flags, ret);
	} else {
		snprintf(head, sizeof(head), HTTP_HDR("200 %i","text/plain"), status);
		snprintf(body, sizeof(body), "0x%08X", ret?:status);//return the PID or the module_{start/stop} result
	}
	int h = !!strstr(valueof(req.headers, "Accept"), "html");
	req.params = (char*[]){"message",body,NULL};
	if(h)module_get(s, req);
	else sendall(s, $(((char*[]){head,body})));
}