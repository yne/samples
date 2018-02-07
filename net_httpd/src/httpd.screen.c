void screen_get(int s, Request req) {
	static char ctx[0x200];//sceJpegEncoderGetContextSize()=384
	static SceJpegEncoderContext encCtx = &ctx;
	static SceDisplayFrameBuf frameBuf = {sizeof(SceDisplayFrameBuf)};
	static int memuid, rgba_reso, jpeg_reso;
	static void* encbuf;

	int h = !!strstr(valueof(req.headers, "Accept"), "html");
	if( h && strlen(req.path) == 1 ) {
		sendall(s, $(((char*[]){HTML_HDR, "<img src=whatever.jpg?mode=0&ratio=128>"})));
		return;
	}
	if (!frameBuf.base && sceDisplayGetFrameBuf(&frameBuf, SCE_DISPLAY_SETBUF_IMMEDIATE) < 0) {
		sendall(s, $(((char*[]){HTTP_HDR("500","text/plain")})));
		return;
	}
	// already initialized but resolutions changed: reset the encoder
	if(jpeg_reso && jpeg_reso != CEIL((frameBuf.width * frameBuf.height) << 1, 256)){
		sceJpegEncoderEnd(encCtx);
		sceKernelFreeMemBlock(memuid);
		memuid = 0;
	}
	if (!memuid) { // first encoder call = init, TODO: detect resolutions changes ?
		rgba_reso = CEIL(frameBuf.width * frameBuf.height *2, 256);
		jpeg_reso = CEIL(frameBuf.width * frameBuf.height, 256);
		memuid = sceKernelAllocMemBlock("sceJpegEncoder", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, CEIL(rgba_reso + jpeg_reso, 0x40000), NULL);
		sceKernelGetMemBlockBase(memuid, &encbuf);
		sceJpegEncoderInit(encCtx, frameBuf.width, frameBuf.height, SCE_JPEGENC_PIXELFORMAT_YCBCR420 | SCE_JPEGENC_PIXELFORMAT_CSC_ARGB_YCBCR, encbuf + rgba_reso, jpeg_reso);
		sceJpegEncoderSetOutputAddr(encCtx, encbuf + rgba_reso, jpeg_reso);
	}
	sceJpegEncoderSetHeaderMode(encCtx,strtol(valueof(req.params,"mode"),NULL,0));
	sceJpegEncoderSetCompressionRatio(encCtx, strtol(valueof(req.params,"ratio"),NULL,0));
	sceJpegEncoderCsc(encCtx, encbuf, frameBuf.base, frameBuf.pitch, SCE_JPEGENC_PIXELFORMAT_ARGB8888);
	int jpeg_size = sceJpegEncoderEncode(encCtx, encbuf);
	if (jpeg_size > 0) {
		sendall(s, $(((char*[]){HTTP_HDR("200","image/jpeg")})));
		send(s, encbuf + rgba_reso, jpeg_size, 0);
	} else {
		sendall(s, $(((char*[]){HTTP_HDR("500","text/plain")})));
	}
}
void screen_post(int s, Request req){
	// What do ...
}