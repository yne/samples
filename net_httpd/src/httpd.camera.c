#define GENERATE_STUB(NAME) int sceCameraSet##NAME(int dev, int val){return printf(#NAME "[%i]=%i\n",dev,val);}
#define GENERATE_FORM(NAME) "<input type=number size=3 name="#NAME" title="#NAME" placeholder="#NAME" value=", valueof(req.formdata,#NAME) ,">"
#define GENERATE_COMP(NAME) if(!strcasecmp(#NAME,param)){printf("%3i "#NAME"        \n", value);sceCameraSet##NAME(device,value);}
#define FOREACH_PARAMS(_)  _(Saturation)_(Brightness)_(Contrast)_(Sharpness)_(Reverse)_(AutoControlHold)_(Effect)_(EV)_(Zoom)_(AntiFlicker)_(ISO)_(Gain)_(WhiteBalance)_(Backlight)_(Nightmode)_(ExposureCeiling)
#define CAMERA_TOTAL 2
/*
TODO: use YCBCR420 isntead of RGBA8888 on camera to (maybe) avoid jpeg csc
*/
SceUID camMem[CAMERA_TOTAL], camJpgMem;
void camera_get(int s, Request req) {
	if (!strcmp(req.path,"/")) {
		sendall(s, $(((char*[]){HTML_HDR "<img src=1.mjpg><form method=POST>" FOREACH_PARAMS(GENERATE_FORM) "<input type=submit></form>"})));
		return;
	}
	static void*camBase[CAMERA_TOTAL];
	static SceCameraInfo info[CAMERA_TOTAL];
	int device = atoi(req.path+1) % CAMERA_TOTAL;
	int mjpg = !!strstr(req.path, ".mjp"), reso_idx = (strtol(valueof(req.params,"resolution"),NULL,0)%8)?:1;
	int*reso = (int[][2]){{0,0},{640,480},{320,240},{160,120},{352,288},{176,144},{480,272},{640,360},{640,360}}[reso_idx];
	int rate = (int[]){3,5,7,10,15,20,30}[strtol(valueof(req.params,"framerate"),NULL,0)%7];
	if(!camMem[device]) {
		camMem[device] = sceKernelAllocMemBlock("camera", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, 256 * 1024 * 5, NULL);
		sceKernelGetMemBlockBase(camMem[device], &camBase[device]);
	}
	SceCameraInfo new_info = {sizeof(SceCameraInfo), 0, SCE_CAMERA_FORMAT_ABGR, reso_idx, rate, reso[0], reso[1],
		.sizeIBase = reso[0] * reso[1] * 4,
		.pIBase    = camBase[device],
	};
	if(memcmp(&info[device], &new_info, sizeof(new_info))) {
		memcpy(&info[device], &new_info, sizeof(new_info));
		sceCameraStop(device);
		sceCameraClose(device);
		sceCameraOpen(device, &info[device]);
		//TODO add some default values (H-flip for front cam ?)
		sceCameraStart(device);
	}
	sendall(s, $(((char*[]){"HTTP/1.1 200 OK\r\nContent-Type: ", mjpg?"multipart/x-mixed-replace; boundary=myboundary":"image/jpeg", "\r\n\r\n"})));
	for(int size, nb = 0; ;nb++) {
		sceCameraRead(device, &(SceCameraRead){sizeof(SceCameraRead),0});
		void*buf = toJpg(&camJpgMem, camBase[device], reso[0], reso[0], reso[1], &size);
		char size_s[8]; snprintf($(size_s),"%i",size);
		if(mjpg)
			sendall(s, $(((char*[]){nb?"\r\n":"", "--myboundary\r\n" "Content-Type: image/jpeg\r\n" "Content-Length: ",size_s,"\r\n\r\n"})));
		if(buf && send(s, buf, size, 0)<0 || !mjpg)
			break;
	}
}
void camera_post(int s, Request req) {
	int device = atoi(req.path+1);
	for (size_t i = 0; req.formdata[i] && req.formdata[i+1]; i+=2) {
		char*param = req.formdata[i];
		if(!*req.formdata[i+1])continue;//skip empty form values
		int value = atoi(req.formdata[i+1]);
		FOREACH_PARAMS(GENERATE_COMP);
	}
	camera_get(s, req);
}