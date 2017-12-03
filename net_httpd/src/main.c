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
#endif

#define HTTP_HDR(S,T) "HTTP/1.1 "S"\r\nContent-Type: " T "\r\n\r\n"
#define HTML_HDR HTTP_HDR("200", "text/html")"<!doctype html>\r\n<html>"
#define $(val) val,(sizeof(val)/sizeof(*val))

void  forward(int from, int to) {
	char buf[1024];
	for(int len;(len = read(from, buf, sizeof(buf))) > 0;)
		write(to, buf, len);
}
void  sendall(int s, char*str[], size_t max) {
	for(size_t i = 0; i < max; i++)
		write(s, str[i], strlen(str[i]));
}
char* readlen(int s, char*buf, size_t len) {
	int ret, pos=0;
	while( (ret = read(s, buf + pos,len)) > 0) {
		len -= ret;
		pos += ret;
	}
	return buf;
}
char* readsep(int s, char*buf, ssize_t buf_max, char* delim, ssize_t delim_size) {delim_size--;
	for(ssize_t i = 0; i < buf_max && read(s, buf + i, sizeof(*buf)) > 0; i++){
		char*look = buf + i - delim_size + 1;
		if((look >= buf) && !memcmp(look, delim, delim_size)){
			memset(look,0,delim_size);
			return buf;
		}
	}
	return buf;
}
char**pair(char**out, size_t max, char*ksep, size_t ksiz, char*vsep, size_t vsiz) {vsiz--;ksiz--;
	for (size_t i = 0; (i < max + 2) && out[i]; i += 2)
		if ((out[i+2] = strstr(out[i]+1, ksep)))
			out[i+2] = memset(out[i+2], 0, ksiz) + ksiz;
	for (size_t i = 0; (i < max + 2) && out[i]; i += 2)
		if ((out[i+1] = strstr(out[i]+1, vsep)))
			out[i+1] = memset(out[i+1], 0, vsiz) + vsiz;
	return out;
}
char*valueof(char**keyval, char*key) {
	for (size_t i = 0; keyval[i] ;i+=2)
		if (!strcasecmp(keyval[i], key))
			return keyval[i+1];
	return "";
}
char*unescape(char*str) {
	#define HATOI(C) (C>'9'?(C&7)+9:C-'0') /*convert a char to it Hex value (A=10)*/
	for (int e=0,u=0;str[u];e++,u++)
		if(str[e]=='%' && str[e+1] && str[e+2])
			str[u] = HATOI(str[e+1]) << 4 | HATOI(str[e+2]),e+=2;
		else
			str[u] = str[e];
	return str;
	#undef HATOI
}

#include "httpd.file.c"
#include "httpd.module.c"
/*TODO /app/ // VPK + PKG support*/
struct{char*module,*export;void (*handler)(int,char*,char*,char**,char**,char**);} routes[64]= {
	{"/file/","GET", file_get},
	{"/file/","POST",file_post},
	{"/module/","GET", module_get},
	{"/module/","POST",module_post},
//	{"/dev/","GET", dev_get},
//	{"/dev/","POST",dev_post},
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
		sendall(s, $(((char*[]){"<li><",tag," href='",routes[i].module,"'>", routes[i].module,"</",tag,"> <kbd>",routes[i].export,"</kbd></li>"})));
	}
	sendall(s, $(((char*[]){"</ul></body></html>\n"})));
}

int main() {
#ifdef __vita__
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
		if(strlen(request)<=1) // empty path
			route_list(out);
		else if(!strchr(request+1,'/')) // no terminating / for module
			sendall(out, $(((char*[]){HTTP_HDR("301","text/plain") "Location: ",request, "/"})));
		else{
			fprintf(stderr,"req=%s\n",request);
			route(request, method)(out, request, strchr(request + 1, '/'), headers, params, formdata);
		}
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