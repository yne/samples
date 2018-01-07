#include <inttypes.h>

static char* evalparam(char* str,char**args){
	if(!str || *str!='$')return str;
	return args[atoi(str+1)*2]?:str+2;
}
void ws_help(int s, Request req);
struct{char*name;void (*handler)(int,Request req);char*url,**params;} cmd[]= {
	{"ls"    ,file_get   ,"$1/",NULL},
	{"rm"    ,file_post  ,"/" ,(char*[64]){"action","unlink","file","$1"}},
	{"rmdir" ,file_post  ,"/" ,(char*[64]){"action","rmdir", "file","$1"}},
	{"lsmod" ,module_get ,"/"  ,NULL},
	{"insmod",module_post,"/"  ,(char*[64]){"action","LoadStart" ,"path" ,"$1","args","$2"}},
	{"rmmod" ,module_post,"/"  ,(char*[64]){"action","StopUnload","modid","$1","args","$2"}},
	{"help"  ,ws_help    ,NULL ,NULL},
	{"exit"  ,NULL       ,NULL ,NULL},
	//thread{list,info,suspend,resume,term,prio,ctx,del...}
	//ipc{mutex,sema,msgbox,cb,timer,vpool,fpool,msgpipes,handler}
	//hook{print,delete,add}
	//memory{info,region,dump,blocks,save,load,poke,fill,find,cache,disasm}
};
void ws_help(int s, Request req) {
	(void)req;
	for(size_t i = 0; i < sizeof(cmd)/sizeof(*cmd); i++)
		sendall(s, $(((char*[]){cmd[i].name})));
}

void tty_get(int s, Request req) {
	char concate[128];
	char *wskey = valueof(req.headers, "Sec-WebSocket-Key");
	if (!*wskey) {
		sendall(s, $(((char*[]){ HTML_HDR "<body onload=\"ws=new WebSocket(location.href.replace(/^http/,'ws'));"
		"ws.onmessage=function(ev){out.innerHTML+=ev.data+'\\n';window.scrollTo(0,document.body.scrollHeight);};"
		"ws.onopen=ws.onclose=function(ev){document.forms[0].o.hidden=!(document.forms[0].q.hidden=ws.readyState!=1)}\">"
		"<pre id=out></pre><form autocomplete=off onsubmit=\"out.innerHTML+='$ '+this.q.value+'\\n';ws.send(this.q.value);this.q.value='';return false;\">"
		"<input hidden name=q placeholder=$><input name=o type=button value=open onclick=document.body.onload()></form>"})));
		return;
	}
	snprintf($(concate),"%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", wskey);
	unsigned char hash[SHA_DIGEST_LENGTH];
	SHA1((unsigned char*)concate, strlen(concate), hash);
	sendall(s, $(((char*[]){"HTTP/1.1 101\r\nConnection: Upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Accept: ",base64(hash, SHA_DIGEST_LENGTH, (char[256]){}),"\r\n\r\n"})));
	
	ws_sendall(s, $(((char*[]){"WebShell type help to list cmd"})));
	for(char* msg;strcmp((msg = ws_recv(s,(char[512]){})),"exit");){
		sendall=ws_sendall;//TODO: use pthread_getspecific to be MT safe
		char found=0, *params[64]={}, **args = pair($((char*[64]){msg}),$(" "),$(" "));
		for(size_t i = 0; i < sizeof(cmd)/sizeof(*cmd); i++) {
			if(strcmp(args[0],cmd[i].name))continue;
			if(cmd[i].params)for(size_t j = 0 ; j < sizeof(params)/sizeof(*params); j++)
				params[j]=evalparam(cmd[i].params[j], args);
			cmd[i].handler(s, (Request){path:evalparam(cmd[i].url, args), params:params, formdata:params});
			found=1;
		}
		if(*args[0] && !found)sendall(s, $(((char*[]){"unknown command: ", args[0]})));
		sendall=http_sendall;
	}
}

void tty_post(int s, Request req) {
	tty_get(s, req);
}
