char* ctrl_types[] = {
	[SCE_CTRL_TYPE_UNPAIRED]="UNPAIRED",
	[SCE_CTRL_TYPE_PHY]="PHY",
	[SCE_CTRL_TYPE_VIRT]="VIRT",
	[SCE_CTRL_TYPE_DS3]="DS3",
	[SCE_CTRL_TYPE_DS4]="DS4",
};
char* ctrl_modes[] = {
	[SCE_CTRL_MODE_DIGITAL]="DIGITAL",
	[SCE_CTRL_MODE_ANALOG]="ANALOG",
	[SCE_CTRL_MODE_ANALOG_WIDE]="ANALOG_WIDE",
};
int ctrl_threshold = 0, ctrl_log = 0;
int ctrl_trunc(int val){
	val = (val>>ctrl_log)-(128>>ctrl_log);
	if(val > ctrl_threshold)return val-ctrl_threshold;
	if(val <-ctrl_threshold)return val+ctrl_threshold;
	return 0;
}
void ctrl_get(int s, Request req) {
	int h = !!strstr(valueof(req.headers, "Accept"), "html");
	char line[512];
	if(!strcmp(req.path,"/")){
		SceCtrlPortInfo info;
		sceCtrlGetControllerPortInfo(&info);
		#define INPUT(name) "<input name="name" title="name" placeholder="name">"
		#define FORM(url) "<form method=POST action="url">"INPUT("value")"<button>"url"</button></form>"
		sendall(s, $(((char*[]){h?HTML_HDR FORM("mode") FORM("modex") FORM("intercept") FORM("threshold") FORM("divider"):TEXT_HDR})));
		for (int p=0; p < sizeof(info.port)/sizeof(*info.port); p++) {
			char* type = "UNKNOW";
			for(int i=0 ; i < sizeof(ctrl_types)/sizeof(*ctrl_types); i++)
				if(i==info.port[p])
					type = ctrl_types[i];
			snprintf(line,sizeof(line),"%i %s (%i)",p,type,info.port[p]);
			if(h && info.port[p]){
				sendall(s, $(((char*[]){ "<h1>",line,"</h1>"})));
				sendall(s, $(((char*[]){ 
					"<form method=POST action=",(char[]){p+'0',0},"/rapid>" INPUT("mask") INPUT("trigger") INPUT("target") INPUT("delay") INPUT("make") "<button>rapid</button></form>"
					"<form method=POST action=",(char[]){p+'0',0},"/actuator>"INPUT("value")"<button>actuator</button></form>"
					"<form method=POST action=",(char[]){p+'0',0},"/light   >"INPUT("value")"<button>light</button></form>"
				})));
			}else
				sendall(s, $(((char*[]){line,"\n"})));
		}
		#undef INPUT
		#undef FORM
	}else if(!strcmp(req.path,"/mode")){
		SceCtrlPadInputMode inputMode;
		sceCtrlGetSamplingMode(&inputMode);
		char* mode = "UNKNOW";
		for (int i=0 ; i < sizeof(ctrl_modes)/sizeof(*ctrl_modes); i++)
			if (i == inputMode)
				mode = ctrl_modes[i];
		sendall(s, $(((char*[]){h?HTML_HDR:TEXT_HDR, mode})));
	}else if(!strcmp(req.path,"/intercept")){
		int intercept = 0;
		sceCtrlGetButtonIntercept(&intercept);
		snprintf(line,sizeof(line),"0x%08X",intercept);
		sendall(s, $(((char*[]){h?HTML_HDR:TEXT_HDR, line})));
	}else if(isdigit(req.path[1]) && (req.path[2]=='/' || req.path[2]=='\0')){ // /:port.*
		int port = req.path[1]-'0';
		if (!strcmp(req.path+2, "/battery")) {
			SceUInt8 level;
			sceCtrlGetBatteryInfo(port, &level);
			snprintf(line,sizeof(line), "%i", level);
		}else{
			SceCtrlData d;
			SceCtrlPortInfo info;
			sceCtrlGetControllerPortInfo(&info);
			int ds = (info.port[port] == SCE_CTRL_TYPE_DS3) || (info.port[port] == SCE_CTRL_TYPE_DS4);
			sendall(s, $(((char*[]){h?HTML_HDR:TEXT_HDR})));
			for(;;){
				sceCtrlReadBufferPositive(port, &d, 1);//need d.timeStamp ?
				snprintf(line, sizeof(line), ds?"%08X [%4i %4i] [%4i %4i] %3u %3u %3u %3u %3u %3u %3u %3u %3u %3u %3u %3u\n" : "%08X [%4i %4i] [%4i %4i]\n",
					d.buttons, ctrl_trunc(d.lx), ctrl_trunc(d.ly), ctrl_trunc(d.rx), ctrl_trunc(d.ry),
					d.up, d.right, d.down, d.left, d.lt, d.rt, d.l1, d.r1, d.triangle, d.circle, d.cross, d.square);
				sendall(s, $(((char*[]){line})));
			}
		}
	}else{
		sendall(s, $(((char*[]){HTTP_HDR("404 NOT FOUND","text/plain"),"the url ",req.path," does not match any handler"})));
	}
}
void ctrl_post(int s, Request req){
	char*val=valueof(req.formdata,"value");
	int value = strtol(val,NULL,val[0]=='#'?16:0);
	int ret = -1;
	if ( !strcmp(req.path,"/mode")) {
		ret = sceCtrlSetSamplingMode(value);
	} else if ( !strcmp(req.path,"/modex")) {
		ret = sceCtrlSetSamplingModeExt(value);//SceCtrlPadInputMode
	} else if ( !strcmp(req.path,"/intercept")) {
		ret = sceCtrlSetButtonIntercept(value);
	} else if ( !strcmp(req.path,"/threshold")) {
		ctrl_threshold = value;
	} else if ( !strcmp(req.path,"/divider")) {
		ctrl_log = value?:ctrl_log;
	}else if(isdigit(req.path[1] && (req.path[2]=='/' || req.path[2]=='\0'))){ // /:port.*
		int port = req.path[1]-'0';
		if (strcmp(req.path+2, "/rapid")) {
			SceCtrlRapidFireRule rule = {
				.Mask    = strtol(valueof(req.formdata,"mask"),NULL,0),
				.Trigger = strtol(valueof(req.formdata,"trigger"),NULL,0),
				.Target  = strtol(valueof(req.formdata,"target"),NULL,0),
				.Delay   = strtol(valueof(req.formdata,"delay"),NULL,0),
				.Make    = strtol(valueof(req.formdata,"make"),NULL,0),
				.Break   = strtol(valueof(req.formdata,"break"),NULL,0),
			};
			ret = sceCtrlSetRapidFire(port, value, &rule);
		} else if (!strcmp(req.path+2, "/actuator")) {
			ret = sceCtrlSetActuator(port, &(SceCtrlActuator){value>>16, value&0xFFFF});
		} else if (!strcmp(req.path+2, "/light")) {
			ret = sceCtrlSetLightBar(port, value>>24, (value>>16)&0xFF, value&0xFF);
		}
	}else{
		sendall(s, $(((char*[]){HTTP_HDR("404 NOT FOUND","text/plain"),"the url ",req.path," does not match any handler"})));
		return;
	}
	sendall(s, $(((char*[]){ret?HTTP_HDR("500","text/plain"):TEXT_HDR})));
}