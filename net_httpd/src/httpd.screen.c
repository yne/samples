SceUID scrJpgMem;
void* screenEncode(void*rgba_buf, int realwidth, int width, int height, int*outsize){
	char ctx[0x200];//sceJpegEncoderGetContextSize()=384
	SceJpegEncoderContext encCtx = &ctx;
	void* cbcr_buf, *jpeg_buf;
	int cbcr_size = CEIL(width * height * 2, 256); // 4:2:0=>(6*w*h)/4 // 4:2:2=>(w*h*8)/4
	int jpeg_size = CEIL(width * height, 256);
	int bloc_size = CEIL(cbcr_size + jpeg_size, 256 * 1024);
	int ret, format = SCE_JPEGENC_PIXELFORMAT_YCBCR420 | SCE_JPEGENC_PIXELFORMAT_CSC_ARGB_YCBCR;
	if(scrJpgMem){
		sceJpegEncoderEnd(encCtx);
		sceKernelFreeMemBlock(scrJpgMem);
		scrJpgMem = 0;
	}
	if (!scrJpgMem) {
		scrJpgMem = sceKernelAllocMemBlock("scrJpgMem", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, bloc_size, NULL);
		sceKernelGetMemBlockBase(scrJpgMem, &cbcr_buf);
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
void screen_get(int s, Request req) {
	int size=0, h = !!strstr(valueof(req.headers, "Accept"), "html");
	if( h && strlen(req.path) == 1 ) {
		sendall(s, $(((char*[]){HTML_HDR, "<img src=whatever.jpg?mode=0&ratio=128>"})));
		return;
	}
	SceDisplayFrameBuf frameBuf = {sizeof(SceDisplayFrameBuf)};
	sceDisplayGetFrameBuf(&frameBuf, SCE_DISPLAY_SETBUF_IMMEDIATE);
	void* buf = screenEncode(frameBuf.base, frameBuf.pitch, frameBuf.width, frameBuf.height, &size);
	sendall(s, $(((char*[]){buf?HTTP_HDR("200","image/jpeg"):HTTP_HDR("500","text/plain")})));
	send(s, buf, size, 0);
}
void screen_post(int s, Request req){
	// What do ...
}