#ifdef __vita__
#define be64toh sceNetNtohll
#define be32toh sceNetNtohl
#define be16toh sceNetNtohs
#else
#include <endian.h>
#endif

#define CEIL(x, a)	(((x) + ((a) - 1)) & ~((a) - 1))
#define HTTP_HDR(STATUS,TYPE) "HTTP/1.1 "STATUS"\r\nConnection: close\r\nContent-Type: "TYPE"\r\n\r\n"
#define HTML_HDR HTTP_HDR("200", "text/html")"<!doctype html>\r\n<html><link rel=icon href=\"data:;base64,iVBORw0KGgo=\">"
#define $(val) val,sizeof(val)/sizeof(*val)

typedef struct{
	char*method,*url,*path,*version,**headers,**params,**formdata;
}Request;

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
	for(int i=0;str[i];i++)
		if(str[i]=='+')
			str[i]=' ';
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
	debug("%sOP:%X L:%i M:%08X >>>", flag&0xF0?"FIN ":"", flag&0x0F, (int)size, len1&0x80?*(uint32_t*)mask:0);
	if(read(s, payload, size) != (int)size)return "";
	for(size_t i = 0; i < size; i++)payload[i] ^= mask[i%4];
	debug("%.*s<<<\n",(int)size,payload);
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

#define SHA_DIGEST_LENGTH 20
#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))
#define blk(i) (block->l[i&15] = rol(block->l[(i+13)&15]^block->l[(i+8)&15] \
	^block->l[(i+2)&15]^block->l[i&15],1))
#define blk0(i) block->l[i]
#if BYTE_ORDER == LITTLE_ENDIAN
#undef blk0
#define blk0(i) (block->l[i] = (rol(block->l[i],24)&0xFF00FF00)  |(rol(block->l[i],8)&0x00FF00FF))
#endif

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);

typedef struct { uint32_t state[5], count[2]; unsigned char buffer[64];} SHA1_CTX;

void SHA1Transform(uint32_t state[5], const unsigned char buffer[64]){
	uint32_t a, b, c, d, e;
	typedef union {
		unsigned char c[64];
		uint32_t l[16];
	} CHAR64LONG16;
	CHAR64LONG16* block = (CHAR64LONG16*)buffer;
	/* Copy ctx->state[] to working vars */
	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];
	/* 4 rounds of 20 operations each. Loop unrolled. */
	R0(a,b,c,d,e, 0); R0(e,a,b,c,d, 1); R0(d,e,a,b,c, 2); R0(c,d,e,a,b, 3);
	R0(b,c,d,e,a, 4); R0(a,b,c,d,e, 5); R0(e,a,b,c,d, 6); R0(d,e,a,b,c, 7);
	R0(c,d,e,a,b, 8); R0(b,c,d,e,a, 9); R0(a,b,c,d,e,10); R0(e,a,b,c,d,11);
	R0(d,e,a,b,c,12); R0(c,d,e,a,b,13); R0(b,c,d,e,a,14); R0(a,b,c,d,e,15);
	R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);
	R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);
	R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);
	R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);
	R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);
	R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);
	R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);
	R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);
	R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);
	R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);
	R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);
	R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);
	R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);
	R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);
	R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);
	R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);
	/* Add the working vars back into ctx.state[] */
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	/* Wipe variables */
	a = b = c = d = e = 0;
}
void SHA1Init(SHA1_CTX* ctx){
	*ctx=(SHA1_CTX){{0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0},{},{}};
}
void SHA1Update(SHA1_CTX* ctx, const unsigned char* data, uint32_t len){
	uint32_t i, j = ctx->count[0];
	if ((ctx->count[0] += len << 3) < j)
		ctx->count[1]++;
	ctx->count[1] += (len>>29);
	j = (j >> 3) & 63;
	if ((j + len) > 63) {
		memcpy(&ctx->buffer[j], data, (i = 64-j));
		SHA1Transform(ctx->state, ctx->buffer);
		for ( ; i + 63 < len; i += 64)
			SHA1Transform(ctx->state, &data[i]);
		j = 0;
	} else i = 0;
	memcpy(&ctx->buffer[j], &data[i], len - i);
}
void SHA1Final(unsigned char digest[20], SHA1_CTX* ctx) {
	unsigned char finalcount[8], c;
	for (unsigned i = 0; i < 8; i++) {
		finalcount[i] = (unsigned char)((ctx->count[(i >= 4 ? 0 : 1)]
		 >> ((3-(i & 3)) * 8) ) & 255);  /* Endian independent */
	}
	c = 0200;
	SHA1Update(ctx, &c, 1);
	while ((ctx->count[0] & 504) != 448) {
		c = 0000;
		SHA1Update(ctx, &c, 1);
	}
	SHA1Update(ctx, finalcount, 8);
	for (unsigned i = 0; i < 20; i++) {
		digest[i] = (unsigned char)
		 ((ctx->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);
	}
	memset(ctx, '\0', sizeof(*ctx));
	memset(&finalcount, '\0', sizeof(finalcount));
}/*
void SHA1(void* buf, unsigned size, unsigned char hash[20]){
	SHA1_CTX ctx;
	SHA1Init(&ctx);
	SHA1Update(&ctx, buf, size);
	SHA1Final(hash, &ctx);
}*/
void SHA1(const unsigned char *str, unsigned len, unsigned char *hash_out)
{
	SHA1_CTX ctx;
	SHA1Init(&ctx);
	for (unsigned ii=0; ii<len; ii+=1)
		SHA1Update(&ctx, str + ii, 1);
	SHA1Final(hash_out, &ctx);
}
