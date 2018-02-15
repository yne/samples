#define GENERATE_FUNC(NAME) int sceCameraSet##NAME(int dev, int val){return printf(#NAME "[%i]=%i\n",dev,val);}
#define GENERATE_FORM(NAME) "<input type=number name="#NAME" title="#NAME" placeholder="#NAME" value=", valueof(req.formdata,#NAME) ,">"
#define GENERATE_COMP(NAME) if(!strcasecmp(#NAME,param)){printf(#NAME":%i            \n", value);sceCameraSet##NAME(device,value);}
#define FOREACH_PARAMS(_)  _(Saturation)_(Brightness)_(Contrast)_(Sharpness)_(Reverse)_(AutoControlHold)_(Effect)_(EV)_(Zoom)_(AntiFlicker)_(ISO)_(Gain)_(WhiteBalance)_(Backlight)_(Nightmode)_(ExposureCeiling)
/*
TODO: use YCBCR420 isntead of RGBA8888 on camera to maybe avoid jpeg csc
*/
#ifndef __vita__ //generate sceCameraSet* stubs 
FOREACH_PARAMS(GENERATE_FUNC)
#endif
SceUID camMem, camJpgMem;
void* cameraEncode(void*rgba_buf, int realwidth, int width, int height, int*outsize){
	char ctx[0x200];//sceJpegEncoderGetContextSize()=384
	SceJpegEncoderContext encCtx = &ctx;
	void* cbcr_buf, *jpeg_buf;
	int cbcr_size = CEIL(width * height * 2, 256); // 4:2:0=>(6*w*h)/4 // 4:2:2=>(w*h*8)/4
	int jpeg_size = CEIL(width * height, 256);
	int bloc_size = CEIL(cbcr_size + jpeg_size, 256 * 1024);
	int ret, format = SCE_JPEGENC_PIXELFORMAT_YCBCR420 | SCE_JPEGENC_PIXELFORMAT_CSC_ARGB_YCBCR;
	if(camJpgMem){
		sceJpegEncoderEnd(encCtx);
		sceKernelFreeMemBlock(camJpgMem);
		camJpgMem = 0;
	}
	if (!camJpgMem) {
		camJpgMem = sceKernelAllocMemBlock("camJpgMem", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, bloc_size, NULL);
		sceKernelGetMemBlockBase(camJpgMem, &cbcr_buf);
		jpeg_buf = cbcr_buf + cbcr_size;
		if(sceJpegEncoderInit(encCtx, width, height, format, jpeg_buf, jpeg_size))return NULL;
		if(sceJpegEncoderSetOutputAddr(encCtx, jpeg_buf, jpeg_size))return NULL;
	}
	//sceJpegEncoderSetHeaderMode(encCtx,strtol(valueof(req.params,"mode"),NULL,0));
	//sceJpegEncoderSetCompressionRatio(encCtx, strtol(valueof(req.params,"ratio"),NULL,0));
	if(sceJpegEncoderCsc(encCtx, cbcr_buf, rgba_buf, realwidth, SCE_JPEGENC_PIXELFORMAT_ARGB8888))return NULL;
	if((*outsize=sceJpegEncoderEncode(encCtx, cbcr_buf))<0)return NULL;
	return jpeg_buf;
}
void camera_get(int s, Request req) {
	if (!strcmp(req.path,"/")) {
		sendall(s, $(((char*[]){HTML_HDR "<img onnload='this.src=this.src.replace(/(_=)(\\d+)/,function(a,_,n){return _+(+n+1)})' src=0?format=5&framerate=0&resolution=1&_=0><form method=POST>" FOREACH_PARAMS(GENERATE_FORM) "<input type=submit></form>"})));
		return;
	}
#ifdef __vita__
	static void* camBase;
	int res_table[][2] = {{0,0},{640,480},{320,240},{160,120},{352,288},{176,144},{480,272},{640,360},{640,360}};
	int size, fps_table[] = {3,5,7,10,15,20,30};
	int device = atoi(req.path+1), cur_res = (strtol(valueof(req.params,"resolution"),NULL,0)%8)?:1;
	if(!camMem) {
		camMem = sceKernelAllocMemBlock("camera", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, 256 * 1024 * 5, NULL);
		sceKernelGetMemBlockBase(camMem, &camBase);
		SceCameraInfo info = {sizeof(SceCameraInfo),
			.format = SCE_CAMERA_FORMAT_ABGR,
			.resolution = cur_res,
			.framerate = fps_table[strtol(valueof(req.params,"framerate"),NULL,0)%7],
			.sizeIBase = res_table[cur_res][0] * res_table[cur_res][1] * 4,
			.pIBase    = camBase,
		};
		sceCameraStop(device);
		sceCameraClose(device);
		sceCameraOpen(device, &info);
		sceCameraStart(device);
	}
	sceCameraRead(device, &(SceCameraRead){sizeof(SceCameraRead),0});
	void* buf = cameraEncode(camBase, res_table[cur_res][0], res_table[cur_res][0], res_table[cur_res][1], &size);
	sendall(s, $(((char*[]){HTTP_HDR("200","image/jpeg")})));
	send(s, buf, size, 0);
	printf(">>>> sent:%i\n",size);
#else
	sendall(s, $(((char*[]){HTTP_HDR("200","image/jpeg\r\nRefresh: 1; url=http://192.168.1.100:8000/camera/0.jpg")})));//
	send(s,$("\xff\xd8\xff\xdb\0\x43\0\3\2\2\2\2\2\3\2\2\2\3\3\3\3\4\6\4\4\4\4\4\0x08\6\6\5\6\x09\x08\x0a\x0a\x09\x08\x09\x09\x0a\x0c\x0f\x0c\x0a\x0b\x0e\x0b\x09\x09\x0d\x11\x0d\x0e\x0f\x10\x10\x11\x10\x0a\x0c\x12\x13\x12\x10\x13\x0f\x10\x10\x10\xff\xc9\0\x0b\x08\0\1\0\1\1\1\x11\0\xff\xcc\0\6\0\x10\x10\5\xff\xda\0\x08\1\1\0\0\x3f\0\xd2\xcf\x20\xff\xd9"),0);
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