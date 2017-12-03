#ifdef __vita__
#include "dirent.vita.h"
#define IS_A_DIR(ent) SCE_S_ISDIR((ent)->d_stat.st_mode)
#define PATH_SHIFT 1
#else
#include <dirent.h>
#define IS_A_DIR(ent) ((ent)->d_type&DT_DIR)
#define PATH_SHIFT 0
#endif

static char fbuf[128*1024];//merge write() call in chunk of 128KB
static unsigned fpos;
static int open_proxy(char* path) {
	fpos = 0;
	return open(path, O_WRONLY | O_CREAT |O_TRUNC, 0666);
}
static void write_proxy(int fd, char*buf, unsigned len) {
	for(unsigned i = 0; i < len; i++) {
		fbuf[fpos]=buf[i];
		if(!(fpos=(fpos+1)%sizeof(fbuf)))
			write(fd, fbuf, sizeof(fbuf));
	}
}
static void close_proxy(int fd) {
	if(fpos)write(fd,fbuf,fpos);
	close(fd);
}

void file_get(int s, char*url, char*path, char**headers, char**params, char**data) {	DIR*dirp = opendir(path+PATH_SHIFT);
	int fd = open(path+PATH_SHIFT, O_RDONLY);
	#ifdef __vita__ // on vita "/" is the devices listing
	if(path[0]=='/' && path[1]=='\0') {
		char*dev[] = {"gro","grw","imc","os","pd","sa","tm","ud","uma","ur","ux","vd","vs","host"};
		sendall(s, $(((char*[]){ HTML_HDR "<h1>Devices:</h1><ul>"})));
		for(size_t i=0;i<sizeof(dev)/sizeof(*dev);i++)sendall(s, $(((char*[]){ "<li><a href=\"./",dev[i],"0:\">",dev[i],"0:</a></li>"})));
	} else
	#endif
	if(dirp && path[strlen(path)-1]!='/'){ // add trailing / for empty url or non / terminated folder path
		sendall(s, $(((char*[]){ "HTTP/1.1 301\r\nLocation: /file/",path+1, "/"})));
	}else if(!dirp && fd < 0){//neither a file nor a directory
		sendall(s, $(((char*[]){ HTTP_HDR("404","text/plain"), "'", path+PATH_SHIFT, "' No such file or directory"})));
	}else if(!dirp){//regular file
		sendall(s, $(((char*[]){ HTTP_HDR("200","application/octet-stream")})));
		forward(fd, s);
		
	}else{
		sendall(s, $(((char*[]){ HTML_HDR "<h1>",path+PATH_SHIFT,"</h1>"
		"<form method=post enctype='multipart/form-data'><input name=f type=file multiple><input type=submit name=action value=Upload></form>"
		"<form method=post><input type=submit value=Delete name=action><ul>"})));
		for(struct dirent*ent;(ent = readdir(dirp));)
			if(ent->d_name[0]!='.')
				sendall(s, $(((char*[]){ "<li><input type=checkbox name=f value=\"",ent->d_name,"\"><a href=\"",ent->d_name,IS_A_DIR(ent)?"/":"","\">",ent->d_name,IS_A_DIR(ent)?"/":"","</a>","</li>"})));
		sendall(s, $(((char*[]){ "</ul></form></html>"})));
	}
	if(dirp)closedir(dirp);
	if(fd>=0)close(fd);
}
void file_post(int s, char*url, char*path, char**headers, char**params, char**data) {
	if(!strcmp(valueof(headers,"Content-Type"),"multipart/form-data")) {
		char sep[70] = {'\r','\n'};//separator implicitly start with a \r\n
		int sep_len = strlen(readsep(s, sep+2, sizeof(sep)-2, $("\r\n")));
		
		for(;;) {
			char**headers = pair($((char*[64]){readsep(s, $((char[4096]){}), $("\r\n\r\n"))}),$("\r\n"),$(": "));
			char buf[4096],*name = strtok((strstr(valueof(headers,"Content-Disposition"),"filename=\"")?:"filename=\"file\"")+10,"\"");
			if(!headers[0] || !name)break;
			
			int fd = open_proxy(strcat(strcat((char[1024]){}, path), name)+PATH_SHIFT);
			for(int len=1; len < sep_len && recv(s,&buf[len-1],1,0) == 1;len++)
				if (sep[len-1]!=buf[len-1])//TODO: use a ring buffer instead of per byte reading
					write_proxy(fd,buf,len),len=0;
			close_proxy(fd);
		}
	}
	file_get(s, url, path, headers, params, data);
}