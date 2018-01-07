#include <openssl/sha.h>
#include <endian.h>
#include <inttypes.h>
/*#define PRINT_ENV() printf("url:'%s' path='%s'\n", url, path);\
	for(int i = 0; headers[i]; i += 2)printf("[%s=%s]\n", headers[i], headers[i+1]);\
	for(int i = 0; params [i]; i += 2)printf("(%s=%s)\n",  params[i], params[i+1]);\
	for(int i = 0; data   [i]; i += 2)printf("{%s=%s}\n",    data[i], data[i+1]);
*/
void usage(int s, char*url, char*path, char**headers, char**params, char**data);
static char* evalparam(char* str,char**args){
	if(!str || *str!='$')return str;
	return args[atoi(str+1)*2]?:str+2;
}
struct{char*name,*url,**params;void (*handler)(int,char*,char*,char**,char**,char**);} cmd[]= {
	{"ls"    ,"$1/",NULL,file_get},
	{"lsmod" ,"/" ,NULL,module_get},
	{"insmod","/" ,(char*[64]){"action","LoadStart" ,"path" ,"$1","args","$2"},module_post},
	{"rmmod" ,"/" ,(char*[64]){"action","StopUnload","modid","$1","args","$2"},module_post},
	{"help"  ,NULL,NULL,usage},
	{"exit"  ,NULL,NULL,NULL},
};
void usage(int s, char*url, char*path, char**headers, char**params, char**data) {
	for(size_t i = 0; i < sizeof(cmd)/sizeof(*cmd); i++)
		sendall(s, $(((char*[]){cmd[i].name})));
}

void tty_get(int s, char*url, char*path, char**headers, char**params, char**data) {
	char concate[128], *wskey = valueof(headers, "Sec-WebSocket-Key");
	if (!*wskey) {
		sendall(s, $(((char*[]){ HTML_HDR "<body onload=\"ws=new WebSocket(location.href.replace(/^http/,'ws'));"
		"ws.onmessage=function(ev){out.innerHTML+=ev.data+'\\n'};"
		"ws.onopen=ws.onclose=function(ev){document.forms[0].q.disabled=ws.readyState!=1}\">"
		"<pre id=out></pre><form autocomplete=off onsubmit=\"out.innerHTML+='$ '+this.q.value+'\\n';ws.send(this.q.value);this.q.value='';return false;\">"
		"<input disabled name=q placeholder=$><input type=button value=open onclick=document.body.onload()><input type=button value=close onclick=ws.close()></form>"})));
		return;
	}
	snprintf($(concate),"%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", wskey);
	unsigned char*hash = SHA1((unsigned char*)concate, strlen(concate), (unsigned char[SHA_DIGEST_LENGTH]){});
	sendall(s, $(((char*[]){"HTTP/1.1 101\r\nConnection: Upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Accept: ",base64(hash, SHA_DIGEST_LENGTH, (char[256]){}),"\r\n\r\n"})));
	
	ws_sendall(s, $(((char*[]){"WebShell type help to list cmd"})));
	for(char* msg;strcmp((msg = ws_recv(s,(char[512]){})),"exit");){
		sendall=ws_sendall;//TODO: use pthread_getspecific to be MT safe
		char found=0, *params[64]={}, **args = pair($((char*[64]){msg}),$(" "),$(" "));
		for(size_t i = 0; i < sizeof(cmd)/sizeof(*cmd); i++) {
			if(strcmp(args[0],cmd[i].name))continue;
			for(size_t j = 0 ; j < sizeof(params)/sizeof(*params); j++)
				params[j]=evalparam(cmd[i].params[j], args);
			cmd[i].handler(s, 0, evalparam(cmd[i].url, args), 0, params, params);
			found=1;
		}
		if(*args[0] && !found)sendall(s, $(((char*[]){"unknown command: ", args[0]})));
		sendall=http_sendall;
	}
	printf("The End\n");
}
void tty_post(int s, char*url, char*path, char**headers, char**params, char**data) {
	tty_get(s, url, path, headers, params, data);
}
