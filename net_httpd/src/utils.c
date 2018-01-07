#define HTTP_HDR(STATUS,TYPE) "HTTP/1.1 "STATUS"\r\nConnection: close\r\nContent-Type: "TYPE"\r\n\r\n"
#define HTML_HDR HTTP_HDR("200", "text/html")"<!doctype html>\r\n<html>"
#define $(val) val,sizeof(val)/sizeof(*val)

char* base64(unsigned char*in, size_t len, char*out){
	char*enc = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	char pad = '=';
	unsigned int i, j;
	for (i = j = 0; i < len; i++)
		switch (i % 3) {
		case 0:out[j++] = enc[(in[i] >> 2) & 0x3F];continue;
		case 1:out[j++] = enc[((in[i-1] & 0x3) << 4) + ((in[i] >> 4) & 0xF)];continue;
		case 2:out[j++] = enc[((in[i-1] & 0xF) << 2) + ((in[i] >> 6) & 0x3)];
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
char* unescape(char*str) {
	#define HATOI(C) (C>'9'?(C&7)+9:C-'0') /*HexAscii to int ('A'=10)*/
	for (int e=0,u=0;str[u];e++,u++)
		if(str[e]=='%' && str[e+1] && str[e+2])
			str[u] = HATOI(str[e+1]) << 4 | HATOI(str[e+2]),e+=2;
		else
			str[u] = str[e];
	return str;
	#undef HATOI
}
void  ws_sendall(int s, char*str[], size_t max){
	for(size_t i = 0; i < max; i++){
		char*data=(!i && !memcmp(str[i],"HTTP/1.1 2",10))?"":str[i];
		//ifcontinue;//skip 200 HTTP headers
		int len = strlen(data);
		if(data[len-1]=='\n')len--;
		write(s, &(uint8_t[]){(i==(max-1)?0x80:0) | !i,len}, 2);//FIN=1|OP=1, MASKED=0+LEN=4
		write(s, data, len);
	}
}
char* ws_recv(int s, char*payload){
	uint8_t flag,len1,mask[4]; uint16_t len2=0;uint64_t len8=0;
												if(read(s,&flag,sizeof(flag))!=sizeof(flag))return "";
												if(read(s,&len1,sizeof(len1))!=sizeof(len1))return "";
	if((len1&0x7F) == 126)if(read(s,&len2,sizeof(len2))!=sizeof(len2))return "";
	if((len1&0x7F) == 127)if(read(s,&len8,sizeof(len8))!=sizeof(len8))return "";
	if((len1&0x80) == 128)if(read(s, mask,sizeof(mask))!=sizeof(mask))return "";
	uint64_t size = len8?be64toh(len8):len2?be16toh(len2):len1&0x7F;
	//printf("%sOP:%X L:%i M:%08X >>>", flag&0xF0?"FIN ":"", flag&0x0F, (int)size, len1&0x80?*(uint32_t*)mask:0);
	if(read(s, payload, size) != (int)size)return "";
	for(size_t i = 0; i < size; i++)payload[i] ^= mask[i%4];
	//printf("%.*s<<<\n",(int)size,payload);
	return payload;
}
void  http_sendall(int s, char*str[], size_t max) {
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
char* valueof(char**keyval, char*key) {
	if(keyval&&key)for (size_t i = 0; keyval[i] ;i+=2)
		if (!strcasecmp(keyval[i], key))
			return keyval[i+1]?:"";
	return "";
}

void(*sendall)(int s, char*str[], size_t max) = http_sendall;
