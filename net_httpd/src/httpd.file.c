#include <dirent.h>
#define PATH_SHIFT 0 // no path shifting: /file/ex will open(/ex)
#define ENT_IS_A_DIR(ent) ((ent)->d_type&DT_DIR) // hide dirent.d_type/SceIoDirent.d_stat delta

#ifdef __vita__
#undef  ENT_IS_A_DIR
#define ENT_IS_A_DIR(ent) SCE_S_ISDIR((ent)->d_stat.st_mode) //d_type does not exist
#undef  PATH_SHIFT  // PSV does not have a '/' folder, so path shifting is needed
#define PATH_SHIFT 1// with path shifting, /file/ex will open(ex)
#endif

void file_get(int s, Request req) {
	DIR*dirp = opendir(req.path+PATH_SHIFT);
	int h = !!strstr(valueof(req.headers, "Accept"), "html");
	int fd = open(req.path+PATH_SHIFT, O_RDONLY);
	#ifdef __vita__ // on vita "/" is the devices listing
	if(req.path[0]=='/' && req.path[1]=='\0') {
		char*dev[] = {"gro","grw","imc","os","pd","sa","tm","ud","uma","ur","ux","vd","vs","host"};
		if(h)sendall(s, $(((char*[]){ HTML_HDR "<h1>Devices:</h1><ul>"})));
		for(size_t i=0;i<sizeof(dev)/sizeof(*dev);i++)
			h?sendall(s, $(((char*[]){ "<li><a href=\"./",dev[i],"0:\">",dev[i],"0:/</a></li>"}))):sendall(s, $(((char*[]){"/",dev[i],"0:/\n"})));
	} else
	#endif
	if(dirp && req.path[strlen(req.path)-1]!='/'){ // add trailing / for empty url or non / terminated folder path
		sendall(s, $(((char*[]){"HTTP/1.1 301\r\nLocation: ",req.url, "/\r\n\r\n"})));
	}else if(!dirp && fd < 0){//neither a file nor a directory
		sendall(s, $(((char*[]){ HTTP_HDR("404","text/plain"), "'", req.path+PATH_SHIFT, "' No such file or directory"})));
	}else if(!dirp){//regular file
		sendall(s, $(((char*[]){ HTTP_HDR("200","application/octet-stream")})));
		char buf[1024];
		for(int len;(len = read(fd, buf, sizeof(buf))) > 0;)
			write(s, buf, len);
	}else{//is a directory => list it entries
		if(h)sendall(s, $(((char*[]){ HTML_HDR "<h1>",req.path+PATH_SHIFT,"</h1>"
		"<form method=post enctype='multipart/form-data'>"
			"<input type=submit value=Upload><input name=f type=file></form>"
		"<form method=post>"
			"<input type=submit name=action value=Unlink>"
			"<input type=submit name=action value=Rmdir>"
			"<input type=submit name=action value=Truncate><input name=size placeholder=size type=number>"
			"<input type=submit name=action value=Rename><input name=name placeholder=name><pre>\n"})));
		else sendall(s, $(((char*[]){ HTTP_HDR("200","text/plain") })));
		for(struct dirent*ent;(ent = readdir(dirp));)
//			if(ent->d_name[0]!='.')
				h?sendall(s, $(((char*[]){ "<input type=radio name=file value=\"",ent->d_name,"\"><a href=\"",ent->d_name,ENT_IS_A_DIR(ent)?"/":"","\">",ent->d_name,ENT_IS_A_DIR(ent)?"/":"","</a>","\n"}))):sendall(s, $(((char*[]){ ent->d_name,ENT_IS_A_DIR(ent)?"/":"","\n"})));
		if(h)sendall(s, $(((char*[]){ "</pre></form></html>"})));
	}
	if(dirp)closedir(dirp);
	if(fd>=0)close(fd);
}

void file_post(int s, Request req) {
	int ret = -1;
	char buf[16*1024], *action = valueof(req.formdata,"action");
	sprintf(buf, "%s%s", req.path+PATH_SHIFT, unescape(valueof(req.formdata,"file")));
	if(strstr(valueof(req.headers,"Content-Type"),"multipart/form-data")) {//file upload
		//foreach chunk given in header-separated data (first header is the boundary)
		for(char**headers;(headers = pair($((char*[64]){readsep(s, $((char[4096]){}), $("\r\n\r\n"))}),$("\r\n"),$(": "))) && headers[0];){
			int bound_len = strlen(headers[0]);
			char* fname = strtok((strstr(valueof(headers,"Content-Disposition"),"filename=\"")?:"filename=\"file\"")+10,"\"");//TODO fix segfault
			char* fpath = strcat(strcat((char[1024]){}, req.path), fname)+PATH_SHIFT;
			//int length  = atoi(valueof(headers,"Content-Disposition"));
			int n, fd = open(fpath, O_RDWR | O_CREAT | O_TRUNC, 0666);//O_RDWR is not RDONLY|WRONLY !
			while ((n = read(s, $(buf))) > 0) {
				write(fd, buf, n);
				off_t eof = lseek(fd, -(bound_len+4), SEEK_END);//seek back to the (possibly written) delim
				read(fd, buf, bound_len+4);// to read the last bytes + "--" + \r\n
				if(!memcmp(buf+0, headers[0], bound_len)){//last file bytes are the delimiter
					ret = ftruncate(fd, eof-2);//trucate the file to remove delim+"--"+"\r\n"
					break;
				}
			}
			close(fd);
			break;
		//no support for multiple file (reason: CPU intensiv/bad algo)
		}
	} else if(!strcasecmp(action, "unlink")) {
		ret = unlink(buf);
	} else if(!strcasecmp(action, "rmdir")) {
		ret = rmdir(buf);
	} else if(!strcasecmp(action, "truncate")) {
		ret = truncate(buf, atoi(valueof(req.formdata,"size")));
	} else if(!strcasecmp(action, "rename")) {
		ret = rename(buf, unescape(valueof(req.formdata,"name")));
	}
	int h = !!strstr(valueof(req.headers, "Accept"), "html");
	if(h)file_get(s, req);
	else sendall(s, $(((char*[]){ ret<0?HTTP_HDR("500","text/plain"):HTTP_HDR("200","text/plain")})));

}