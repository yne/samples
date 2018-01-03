#include <openssl/sha.h>
#include <endian.h>
#include <inttypes.h>
/*#define PRINT_ENV() printf("url:'%s' path='%s'\n", url, path);\
	for(int i = 0; headers[i]; i += 2)printf("[%s=%s]\n", headers[i], headers[i+1]);\
	for(int i = 0; params [i]; i += 2)printf("(%s=%s)\n",  params[i], params[i+1]);\
	for(int i = 0; data   [i]; i += 2)printf("{%s=%s}\n",    data[i], data[i+1]);
*/
char*base64(unsigned char*in, size_t len, char*out){
	char*enc = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	char pad = '=';
	unsigned int i, j;
	for (i = j = 0; i < len; i++)
		switch (i % 3) {
		case 0:
			out[j++] = enc[(in[i] >> 2) & 0x3F];
			continue;
		case 1:
			out[j++] = enc[((in[i-1] & 0x3) << 4) + ((in[i] >> 4) & 0xF)];
			continue;
		case 2:
			out[j++] = enc[((in[i-1] & 0xF) << 2) + ((in[i] >> 6) & 0x3)];
			out[j++] = enc[in[i] & 0x3F];
		}
	i--;
	if ((i % 3) == 0) {
		out[j++] = enc[(in[i] & 0x3) << 4];
		out[j++] = pad;
		out[j++] = pad;
	} else if ((i % 3) == 1) {
		out[j++] = enc[(in[i] & 0xF) << 2];
		out[j++] = pad;
	}
	return out;
}
void ws_send(int s, char*str){
	int len = strlen(str);
	write(s, &(uint8_t[]){0x81,len}, 2);//FIN=1+OP=1, MASKED=0+LEN=4
	write(s, str, len);
}
void tty_get(int s, char*url, char*path, char**headers, char**params, char**data) {
	char derived[128], *wskey = valueof(headers, "Sec-WebSocket-Key");
	if (!*wskey) {
		sendall(s, $(((char*[]){ HTML_HDR "<body onload=\"ws=new WebSocket(location.href.replace(/^http/,'ws'));"
		"ws.onmessage=function(ev){out.innerHTML+=ev.data+'\\n'};"
		"ws.onopen=ws.onclose=function(ev){document.forms[0].q.disabled=ws.readyState!=1}\">"
		"<pre id=out></pre><form autocomplete=off onsubmit=\"out.innerHTML+='$ '+this.q.value+'\\n';ws.send(this.q.value);this.q.value='';return false;\">"
		"<input disabled name=q placeholder=$><input type=button value=open onclick=document.body.onload()><input type=button value=close onclick=ws.close()></form>"})));
		return;
	}
	snprintf($(derived),"%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", wskey);
  unsigned char*hash = SHA1((unsigned char*)derived, strlen(derived), (unsigned char[SHA_DIGEST_LENGTH]){});
	sendall(s, $(((char*[]){
		"HTTP/1.1 101\r\n"
		"Connection: Upgrade\r\n"
		"Upgrade: websocket\r\n"
		"Sec-WebSocket-Accept: ",base64(hash, SHA_DIGEST_LENGTH, (char[256]){}),"\r\n"
		"\r\n"})));
	ws_send(s, "Welcome");
	for(;;){
		uint8_t flag,len1,mask[4]; uint16_t len2=0;uint64_t len8=0;char payload[512]={};
		                     if(read(s,&flag,sizeof(flag))!=sizeof(flag))break;
		                     if(read(s,&len1,sizeof(len1))!=sizeof(len1))break;
		if((len1&0x7F) == 126)if(read(s,&len2,sizeof(len2))!=sizeof(len2))break;
		if((len1&0x7F) == 127)if(read(s,&len8,sizeof(len8))!=sizeof(len8))break;
		if((len1&0x80) == 128)if(read(s, mask,sizeof(mask))!=sizeof(mask))break;
		uint64_t size = len8?be64toh(len8):len2?be16toh(len2):len1&0x7F;
		printf("%sOP:%X L:%i M:%08X >>>", flag&0xF0?"FIN ":"", flag&0x0F, (int)size, len1&0x80?*(uint32_t*)mask:0);
		if(read(s, payload, size) != (int)size)break;
		
		for(size_t i = 0; i < size; i++)payload[i] ^= mask[i%4];
		printf("%.*s<<<\n",(int)size,payload);
		if(!strcmp((char*)payload,"ping"))
			ws_send(s, "pong");
		else if(!strcmp((char*)payload,"exit")){
			ws_send(s, "byebye");
			break;
		}else{
			ws_send(s, "unknown command");
		}
	}
	printf("The End\n");
}
void tty_post(int s, char*url, char*path, char**headers, char**params, char**data) {
	tty_get(s, url, path, headers, params, data);
}