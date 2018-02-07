#define GENERATE_FUNC(NAME) int sceCameraSet##NAME(int dev, int val){return printf(#NAME "[%i]=%i\n",dev,val);}
#define GENERATE_FORM(NAME) "<input type=number name="#NAME" title="#NAME" placeholder="#NAME" value=", valueof(req.formdata,#NAME) ,">"
#define GENERATE_COMP(NAME) if(!strcasecmp(#NAME,param))sceCameraSet##NAME(device,value);
#define FOREACH_PARAMS(_)  _(Saturation)_(Brightness)_(Contrast)_(Sharpness)_(Reverse)_(AutoControlHold)_(Effect)_(EV)_(Zoom)_(AntiFlicker)_(ISO)_(Gain)_(WhiteBalance)_(Backlight)_(Nightmode)_(ExposureCeiling)

#ifndef __vita__ //genreate sceCameraSet* stubs 
FOREACH_PARAMS(GENERATE_FUNC)
#endif

void camera_get(int s, Request req) {
	if (!strcmp(req.path,"/")) {
		sendall(s, $(((char*[]){HTML_HDR "<img src=0?format=5&fps=3&resolution=1><form method=POST>" FOREACH_PARAMS(GENERATE_FORM) "<input type=submit></form>"})));
		return;
	}
#ifdef __vita__
	sendall(s, $(((char*[]){HTTP_HDR("200","text/plain"), "sup!\n\n"})));

	void* base;
	SceUID memblock = sceKernelAllocMemBlock("camera", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, 256 * 1024 * 5, NULL);
	int res = sceKernelGetMemBlockBase(memblock, &base);
	printf("base = %08X (%i)\n\n",base,res);
	sceKernelFreeMemBlock(memblock);








	return;
	static char ctx[0x200];//sceJpegEncoderGetContextSize()=384
	static SceJpegEncoderContext camEnc = &ctx;
	static SceCameraInfo prevInfo;
	static int prevDevice, rgba_reso, jpeg_reso;
	static void *encbuf;

	int res_table[][2] = {{0,0},{640,480},{320,240},{160,120},{352,288},{176,144},{480,272},{640,360},{640,360}};
	int ret = 0, device = atoi(req.path+1), cur_res = strtol(valueof(req.params,"resolution"),NULL,0)%9;
	if(!encbuf){//allocate at least 640*480*4 bytes (ex: 256 * 1024 * 5)
		rgba_reso = CEIL(res_table[1][0] * res_table[1][1] * 2, 256);
		jpeg_reso = CEIL(res_table[1][0] * res_table[1][1], 256);
		sceKernelGetMemBlockBase(sceKernelAllocMemBlock("camera", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, CEIL(rgba_reso + jpeg_reso, 0x40000), NULL), &encbuf);
		printf("encbuf: %p\n",encbuf); 
	}

	SceCameraInfo info = {sizeof(SceCameraInfo),
		.format = strtol(valueof(req.params,"format"),NULL,0),//422_TO_ABGR
		.resolution = cur_res,
		.framerate = strtol(valueof(req.params,"fps"),NULL,0),
		.sizeIBase = 4*res_table[cur_res][0]*res_table[cur_res][1],
		.pitch     = 4*res_table[1][0],
		.pIBase = encbuf,
	};
	if (memcmp(&prevInfo, &info, sizeof(info)) || prevDevice != device) {
		if (prevDevice != device) {
			sceJpegEncoderEnd(camEnc);
			sceCameraStop(prevDevice);
			sceCameraClose(prevDevice);
			prevDevice = device;
		}
		memcpy(&prevInfo, &info, sizeof(info));//memcpy before CameraOpen will modify it info
		sceCameraOpen(device, &info);
		sceCameraStart(device);
		sceJpegEncoderInit(camEnc, res_table[cur_res][0], res_table[cur_res][1], SCE_JPEGENC_PIXELFORMAT_YCBCR420 | SCE_JPEGENC_PIXELFORMAT_CSC_ARGB_YCBCR, encbuf + rgba_reso, jpeg_reso);
		//sceJpegEncoderSetCompressionRatio(camEnc, 25);
	}
	if (sceCameraIsActive(device) > 0) {
		sceCameraRead(device, &(SceCameraRead){sizeof(SceCameraRead)});
		sceJpegEncoderCsc(camEnc, encbuf, info.pIBase, info.pitch, SCE_JPEGENC_PIXELFORMAT_ARGB8888);
		int jpeg_size = sceJpegEncoderEncode(camEnc, encbuf);//multipart/x-mixed-replace;boundary=<boundary-name>
		sendall(s, $(((char*[]){HTTP_HDR("200","image/jpeg")})));
		write(s, encbuf + rgba_reso, jpeg_size);
	} else{
		sendall(s, $(((char*[]){HTTP_HDR("500","text/plain")})));
	}
#else
	sendall(s, $(((char*[]){HTTP_HDR("200","image/jpeg")})));//Refresh: 1; url=http://www.example.org/
	write(s,$("\xff\xd8\xff\xdb\0\x43\0\3\2\2\2\2\2\3\2\2\2\3\3\3\3\4\6\4\4\4\4\4\0x08\6\6\5\6\x09\x08\x0a\x0a\x09\x08\x09\x09\x0a\x0c\x0f\x0c\x0a\x0b\x0e\x0b\x09\x09\x0d\x11\x0d\x0e\x0f\x10\x10\x11\x10\x0a\x0c\x12\x13\x12\x10\x13\x0f\x10\x10\x10\xff\xc9\0\x0b\x08\0\1\0\1\1\1\x11\0\xff\xcc\0\6\0\x10\x10\5\xff\xda\0\x08\1\1\0\0\x3f\0\xd2\xcf\x20\xff\xd9"));
#endif
}
void camera_post(int s, Request req) {
	int device = atoi(req.path+1);
	for (size_t i = 0; req.formdata[i] && req.formdata[i+1]; i+=2) {
		char*param = req.formdata[i];
		int value = atoi(req.formdata[i+1]);
		FOREACH_PARAMS(GENERATE_COMP);
	}
	camera_get(s, req);
}