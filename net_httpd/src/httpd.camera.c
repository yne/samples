#define GENERATE_FUNC(NAME) int sceCameraSet##NAME(int dev, int val){return printf(#NAME "[%i]=%i\n",dev,val);}
#define GENERATE_FORM(NAME) "<input type=number name="#NAME" title="#NAME" placeholder="#NAME" value=", valueof(data,#NAME) ,">"
#define FOREACH_PARAMS(_)  _(Saturation)_(Brightness)_(Contrast)_(Sharpness)_(Reverse)_(AutoControlHold)_(Effect)_(EV)_(Zoom)_(AntiFlicker)_(ISO)_(Gain)_(WhiteBalance)_(Backlight)_(Nightmode)_(ExposureCeiling)

#ifndef __vita__
FOREACH_PARAMS(GENERATE_FUNC)
#endif
#define SENDFAIL() {}
void camera_get(int s, char*url, char*path, char**headers, char**params, char**data) {
	if (!strcmp(path,"/")) {
		sendall(s, $(((char*[]){HTML_HDR "<img src=0?format=5&fps=3&resolution=1><form method=POST>" FOREACH_PARAMS(GENERATE_FORM) "<input type=submit></form>"})));
		return;
	}
#ifdef __vita__
	static SceCameraInfo prevInfo;
	static int prevDevice;
	static void* base;
	if(!base)//allocate at least 640*480*4 bytes (ex: 256 * 1024 * 5)
		sceKernelGetMemBlockBase(sceKernelAllocMemBlock("camera", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, 256 * 1024 * 5, NULL), &base);
	int res_table[][2] = {{0,0},{640,480},{320,240},{160,120},{352,288},{176,144},{480,272},{640,360},{640,360}};
	int ret = 0, device = atoi(path+1), cur_res = strtol(valueof(params,"resolution"),NULL,0)%9;

	SceCameraInfo info = {sizeof(SceCameraInfo),
		.format = strtol(valueof(params,"format"),NULL,0),//422_TO_ABGR
		.resolution = cur_res,
		.framerate = strtol(valueof(params,"fps"),NULL,0),
		.sizeIBase = 4*res_table[cur_res][0]*res_table[cur_res][1],
		.pitch     = 0,//4*(DISPLAY_WIDTH-res_table[cur_res][0]),
		.pIBase = base,
	};
	if (memcmp(&prevInfo, &info, sizeof(info)) || prevDevice != device) {
		if(prevDevice != device){
			printf("--stop:%08X\n",sceCameraStop(prevDevice));
			printf("--close:%08X\n",sceCameraClose(prevDevice));
			prevDevice = device;
		}
		memcpy(&prevInfo, &info, sizeof(info));//memcpy before CameraOpen will modify it info
		printf("--open:%08X\n",sceCameraOpen(device, &info));
		printf("--start:%08X\n",sceCameraStart(device));
	}
	if(sceCameraIsActive(device)>0)
		printf("----read:%08X\n",sceCameraRead(device, &(SceCameraRead){sizeof(SceCameraRead)}));
	sendall(s, $(((char*[]){ret?HTTP_HDR("500","text/plain"):HTTP_HDR("200","image/jpeg")})));//Refresh: 1; url=http://www.example.org/
	//sceDisplaySetFrameBuf(&(SceDisplayFrameBuf){sizeof(SceDisplayFrameBuf), base, DISPLAY_WIDTH, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT}, SCE_DISPLAY_SETBUF_NEXTFRAME);
	write(s,base,640*480*4);
#else
	sendall(s, $(((char*[]){HTTP_HDR("200","image/jpeg")})));//Refresh: 1; url=http://www.example.org/
	write(s,$("\xff\xd8\xff\xdb\0\x43\0\3\2\2\2\2\2\3\2\2\2\3\3\3\3\4\6\4\4\4\4\4\0x08\6\6\5\6\x09\x08\x0a\x0a\x09\x08\x09\x09\x0a\x0c\x0f\x0c\x0a\x0b\x0e\x0b\x09\x09\x0d\x11\x0d\x0e\x0f\x10\x10\x11\x10\x0a\x0c\x12\x13\x12\x10\x13\x0f\x10\x10\x10\xff\xc9\0\x0b\x08\0\1\0\1\1\1\x11\0\xff\xcc\0\6\0\x10\x10\5\xff\xda\0\x08\1\1\0\0\x3f\0\xd2\xcf\x20\xff\xd9"));
#endif
}
void camera_post(int s, char*url, char*path, char**headers, char**params, char**data) {
	int device = atoi(path+1);
	for (size_t i = 0; data[i] && data[i+1]; i+=2) {
		char*param = data[i];
		int value = atoi(data[i+1]);
		if(!strcasecmp("Saturation",param))sceCameraSetSaturation(device,value);
		if(!strcasecmp("Brightness",param))sceCameraSetBrightness(device,value);
		if(!strcasecmp("Contrast",param))sceCameraSetContrast(device,value);
		if(!strcasecmp("Sharpness",param))sceCameraSetSharpness(device,value);
		if(!strcasecmp("Reverse",param))sceCameraSetReverse(device,value);
		if(!strcasecmp("Effect",param))sceCameraSetEffect(device,value);
		if(!strcasecmp("EV",param))sceCameraSetEV(device,value);
		if(!strcasecmp("Zoom",param))sceCameraSetZoom(device,value);
		if(!strcasecmp("AntiFlicker",param))sceCameraSetAntiFlicker(device,value);
		if(!strcasecmp("ISO",param))sceCameraSetISO(device,value);
		if(!strcasecmp("Gain",param))sceCameraSetGain(device,value);
		if(!strcasecmp("WhiteBalance",param))sceCameraSetWhiteBalance(device,value);
		if(!strcasecmp("Backlight",param))sceCameraSetBacklight(device,value);
		if(!strcasecmp("Nightmode",param))sceCameraSetNightmode(device,value);
		if(!strcasecmp("ExposureCeiling",param))sceCameraSetExposureCeiling(device,value);
		if(!strcasecmp("AutoControlHold",param))sceCameraSetAutoControlHold(device,value);
	}
	camera_get(s, url, path, headers, params, data);
}