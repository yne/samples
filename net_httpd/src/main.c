#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#define PORT 8080
#define debug(...)

#ifdef __vita__
#undef  PORT    // vita userland programs can bind on < 1024 ports
#define PORT 80 // let's simply use the HTTP port
#include <vitasdk.h>
#include "debugScreen.h"
#define printf  psvDebugScreenPrintf
#define exit    sceKernelExitDeleteThread
#define rmdir   sceIoRmdir
#define pthread_create(thid, opt, func, argp) sceKernelStartThread(sceKernelCreateThread(__func__,func##_vita,0x10000100,0x10000,0,0,NULL), sizeof(argp), argp)
#endif

#include "utils.c"
#include "httpd.file.c"
#include "httpd.module.c"
#include "httpd.camera.c"
#include "httpd.screen.c"
#include "httpd.tty.c"

static int alive = 1;
void exit_get(int s, Request req) {
	alive = 0;
	sendall(s, $(((char*[]){"HTTP/1.1 301\r\nLocation: ",req.url, "-\r\n\r\n"})));
	close(s);
}
struct{char*module,*export;void (*handler)(int,Request);} routes[64]= {
	{"/exit/",  "GET", exit_get   },
	{"/file/",  "GET", file_get   },
	{"/file/",  "POST",file_post  },
	{"/module/","GET", module_get },
	{"/module/","POST",module_post},
	{"/camera/","GET", camera_get },
	{"/camera/","POST",camera_post},
	{"/screen/","GET", screen_get },
	{"/screen/","POST",screen_post},
	{"/tty/",   "GET", tty_get    },
	{"/tty/",   "POST",tty_post   },
};

void route404(int s, Request req){
	sendall(s, $(((char*[]){HTTP_HDR("404","text/plain"), req.url, " Not Found"})));
}
void add_slash(int s, Request req){
	sendall(s, $(((char*[]){"HTTP/1.1 301\r\nLocation: ",req.url, "/\r\n\r\n"})));
}
void route_list(int s, Request req){
	/* TODO: dynamic fetch of all running modules that have the 'http_$meth' export and print them */
	int h = !!strstr(valueof(req.headers, "Accept"), "html");
	sendall(s, $(((char*[]){ h ? HTML_HDR "<body><ul>" : HTTP_HDR("200","text/plain")})));
	for (int i = 0; routes[i].module; i++) {
		char* tag = strcmp("GET",routes[i].export) ? "u" : "a";
		if(h)sendall(s, $(((char*[]){"<li><",tag," href='",routes[i].module,"'>", routes[i].module,"</",tag,"> <kbd>",routes[i].export,"</kbd></li>\n"})));
		else sendall(s, $(((char*[]){routes[i].module," ",routes[i].export,"\n"})));
	}
	if(h)sendall(s, $(((char*[]){"</ul></body></html>\n"})));
}
void (*route(char*url, char*meth))(int s, Request req){
	if(strlen(url) <= 1) // empty path
		return route_list;
	if(!strchr(url + 1,'/')) // no terminating / for module
		return add_slash;
	for(int i = 0; routes[i].module; i++)
		if(!memcmp(url,routes[i].module,strlen(routes[i].module)) && !strcmp(meth,routes[i].export))
			return routes[i].handler;/* return the $module that have the 'http_$meth' export*/
	return route404;
}
void* requestHandler(void*arg){
	int out=*(int*)arg;
	Request req = {};
	req.method  = readsep(out, $((char[  16]){}), $(" "));
	req.url     = readsep(out, $((char[4096]){}), $(" "));
	req.version = readsep(out, $((char[  10]){}), $("\r\n"));
	req.path    = strchr(req.url + !!*req.url, '/');
	char*header = readsep(out, $((char[4096]){}), $("\r\n\r\n"));
	req.headers = pair($((char*[64]){header}),$("\r\n"),$(": "));
	char*param  = memset(strchr(req.url,'?')?:(char[]){'?',0},0,1)+1;
	req.params  = pair($((char*[64]){param}),$("&"),$("="));
	req.formdata= strcmp(valueof(req.headers, "Content-Type"), "application/x-www-form-urlencoded")?(char*[256]){}:
		            pair($((char*[256]){readlen(out, (char[4096]){},atoi(valueof(req.headers, "Content-Length")))}),$("&"),$("="));
	route(req.url, req.method)(out, req);
	return close(out)?NULL:NULL;
}
int requestHandler_vita(unsigned args, void*argp){return requestHandler(args?argp:NULL)!=NULL;}

int main(int argc, char*argv[]) {
#ifdef __vita__
	psvDebugScreenInit();
	static char net_mem[1*1024*1024];
	sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
	sceNetInit(&(SceNetInitParam){net_mem, sizeof(net_mem)});
#endif
	int out, in = socket(AF_INET, SOCK_STREAM, 0), port = argc>1?atoi(argv[1]):PORT;
	setsockopt(in, SOL_SOCKET, SO_REUSEPORT, &(int[]){1}, sizeof(int));
	bind(in, (struct sockaddr *) &((struct sockaddr_in){.sin_family=AF_INET,htons(port),{}}), sizeof(struct sockaddr_in));
	listen(in, 5);
	while (alive && (out=accept(in, NULL, NULL)) >= 0)
		pthread_create(&(pthread_t){0}, NULL, requestHandler, &out);
	close(in);
	// dodge newlib's exit() called on main() return that would kill our parent
	return exit(0);
}
