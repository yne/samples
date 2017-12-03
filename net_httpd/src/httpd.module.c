void module_get(int s, char*url, char*path, char**headers, char**params, char**data) {
	printf("url:'%s' path='%s'\n", url, path);
	for(int i = 0; headers[i]; i += 2)printf("[%s=%s]\n", headers[i], headers[i+1]);
	for(int i = 0; params [i]; i += 2)printf("(%s=%s)\n",  params[i], params[i+1]);
	for(int i = 0; data   [i]; i += 2)printf("{%s=%s}\n",    data[i], data[i+1]);

	sendall(s, $(((char*[]){HTML_HDR, "<h1>Module List:</h1><form action=?test=ok method=post><input type=submit name=action value=stop><ul>"
	"<li><input placeholder='ux0:dummy.self'/><input type=submit name=action value=start/></li>"})));
#ifdef __vita__
	SceUID modids[32];
	int num = sizeof(modids)/sizeof(*modids);
	int ret = sceKernelGetModuleList(0x80|0x1, modids, &num);
	for(int n = 0; n < num; n++){
		SceKernelModuleInfo mi;
		ret = sceKernelGetModuleInfo(modids[n],&mi);
		char line[1024];
		sprintf(line, "<li><input type=checkbox value=%08lX/>%08lX %s %s</li>", mi.modid, mi.modid, mi.module_name, mi.path);
	}
#endif
	sendall(s, $(((char*[]){"</ul>"})));
} 
void module_post(int s, char*url, char*path, char**headers, char**params, char**data){
	printf("url:'%s' path='%s'\n", url, path);
	for(int i = 0; headers[i]; i += 2)printf("[%s=%s]\n", headers[i], headers[i+1]);
	for(int i = 0; params [i]; i += 2)printf("(%s=%s)\n",  params[i], params[i+1]);
	for(int i = 0; data   [i]; i += 2)printf("{%s=%s}\n",    data[i], data[i+1]);
#ifdef __vita__
	int status;
	char*suprx_path="TODO.suprx", *arg[]={"toto:","get","from","request"};
	int modid = sceKernelLoadStartModule(suprx_path, sizeof(arg), &arg, 0, NULL, &status);
#endif
}