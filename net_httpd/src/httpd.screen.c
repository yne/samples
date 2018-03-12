SceUID scrJpgMem;
void screen_get(int s, Request req) {
	int h = !!strstr(valueof(req.headers, "Accept"), "html");
	int mjpg = !!strstr(req.path, ".mjp");
	if( h && strlen(req.path) == 1 ) {
		sendall(s, $(((char*[]){HTML_HDR, "<img src=whatever.jpg?mode=0&ratio=128>"})));
		return;
	}
	sendall(s, $(((char*[]){"HTTP/1.1 200 OK\r\nContent-Type: ", mjpg?"multipart/x-mixed-replace; boundary=myboundary":"image/jpeg", "\r\n\r\n"})));
	for(int size, nb = 0; ;nb++) {
		SceDisplayFrameBuf frameBuf = {sizeof(SceDisplayFrameBuf)};
		sceDisplayGetFrameBuf(&frameBuf, SCE_DISPLAY_SETBUF_NEXTFRAME);
		void* buf = toJpg(&scrJpgMem, frameBuf.base, frameBuf.pitch, frameBuf.width, frameBuf.height, &size);
		char size_s[8]; snprintf($(size_s),"%i",size);
		if(mjpg)
			sendall(s, $(((char*[]){nb?"\r\n":"", "--myboundary\r\n" "Content-Type: image/jpeg\r\n" "Content-Length: ",size_s,"\r\n\r\n"})));
		if(buf && send(s, buf, size, 0)<0 || !mjpg)
			break;
	}
}
void screen_post(int s, Request req){
	char*power = valueof(req.formdata,"power");
	if(!strcasecmp(power,"on" ))scePowerRequestDisplayOn();
	if(!strcasecmp(power,"off"))scePowerRequestDisplayOff();
	sendall(s, $(((char*[]){TEXT_HDR})));
}