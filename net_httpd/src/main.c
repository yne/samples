#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#define PORT 8080

#ifdef __vita__
#undef  PORT
#define PORT 80 // vita userland programs can bind on < 1024 ports
#include <sys/_timeval.h>
#include <vitasdk.h>
#include "debugScreen.h"
#define printf  psvDebugScreenPrintf
#endif


#include "utils.c"
#include "httpd.file.c"
#include "httpd.module.c"
#include "httpd.camera.c"
#include "httpd.tty.c"

struct{char*module,*export;void (*handler)(int,char*,char*,char**,char**,char**);} routes[64]= {
	{"/file/",  "GET", file_get   },
	{"/file/",  "POST",file_post  },
	{"/module/","GET", module_get },
	{"/module/","POST",module_post},
	{"/camera/","GET", camera_get },
	{"/camera/","POST",camera_post},
	{"/tty/",   "GET", tty_get    },
	{"/tty/",   "POST",tty_post   },
};
void route404(int s, char*url, char*path, char**hdr, char**par, char**formdata){
	sendall(s, $(((char*[]){HTTP_HDR("404","text/plain"), url, " Not Found"})));
}
void (*route(char*module,char*meth))(int s, char*url, char*path, char**headers, char**params, char**formdata){
	for(int i=0;routes[i].module;i++)
		if(!memcmp(module,routes[i].module,strlen(routes[i].module)) && !strcmp(meth,routes[i].export))
			return routes[i].handler;/* return the $module that have the 'http_$meth' export*/
	return route404;
}
void route_list(int s){
	//consume(s);
	/* TODO: dynamic fetch of all running modules that have the 'http_$meth' export and print them */
	sendall(s, $(((char*[]){HTML_HDR "<body><ul>"})));
	for(int i=0;routes[i].module;i++){
		char* tag = strcmp("GET",routes[i].export)?"u":"a";
		sendall(s, $(((char*[]){"<li><",tag," href='",routes[i].module,"'>", routes[i].module,"</",tag,"> <kbd>",routes[i].export,"</kbd></li>\n"})));
	}
	sendall(s, $(((char*[]){"</ul></body></html>\n"})));
}

int main(int argc, char**argv) {
#ifdef __vita__
	psvDebugScreenInit();
	printf("HTTP server: arg[%i]\n",argc);
	//for(int i=0;i<argc;i++)printf("arg[%i]='%s'\n",i,argv[i]);
	static char net_mem[1*1024*1024];
	sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
	sceNetInit(&(SceNetInitParam){net_mem, sizeof(net_mem)});
#endif
	int out, in = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(in, SOL_SOCKET, SO_REUSEPORT, &(int[]){1}, sizeof(int));
	bind(in, (struct sockaddr *) &((struct sockaddr_in){.sin_family=AF_INET,htons(PORT),{}}), sizeof(struct sockaddr_in));
	listen(in,5);
	while ((out=accept(in, NULL, NULL))>=0){
		//setsockopt(out, SOL_SOCKET, SO_RCVTIMEO, &(struct timeval){0,100*1000}, sizeof(struct timeval));
		char* method  = readsep(out, $((char[  16]){}), $(" "));
		char* request = readsep(out, $((char[4096]){}), $(" "));
		/* version = */ readsep(out, $((char[  10]){}), $("\r\n"));
		char* header  = readsep(out, $((char[4096]){}), $("\r\n\r\n"));
		char**headers = pair($((char*[64]){header}),$("\r\n"),$(": "));
		char* param   = memset(strchr(request,'?')?:(char[]){'?',0},0,1)+1;
		char**params  = pair($((char*[64]){param}),$("&"),$("="));
		char**formdata= (char*[256]){};
		if(!strcmp(valueof(headers, "Content-Type"), "application/x-www-form-urlencoded")){
			int len = atoi(valueof(headers, "Content-Length"));
			formdata = pair($((char*[256]){readlen(out, (char[4096]){},len>4096?4096:len)}),$("&"),$("="));
		}
		printf("%s\n",request);
		if(strlen(request)<=1) // empty path
			route_list(out);
		else if(!strchr(request+1,'/')) // no terminating / for module
			sendall(out, $(((char*[]){"HTTP/1.1 301\r\nLocation: ",request, "/\r\n\r\n"})));
		else
			route(request, method)(out, request, strchr(request + 1, '/'), headers, params, formdata);
		close(out);
	}
	close(in);
#ifdef __vita__
	sceNetTerm();
	sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
	sceKernelDelayThread(~0);
#endif
	return 0;
}