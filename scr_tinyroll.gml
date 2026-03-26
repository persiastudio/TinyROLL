function tinyroll_init()
{
    #macro TRDLL "TinyROLL.dll"
    global.tr_fn_open        = external_define(TRDLL, "DLL_Video_Open",            dll_cdecl, ty_real, 1, ty_string);
    global.tr_fn_close       = external_define(TRDLL, "DLL_Video_Close",           dll_cdecl, ty_real, 0           );
    global.tr_fn_play        = external_define(TRDLL, "DLL_Video_Play",            dll_cdecl, ty_real, 0           );
    global.tr_fn_pause       = external_define(TRDLL, "DLL_Video_Pause",           dll_cdecl, ty_real, 0           );
    global.tr_fn_seek        = external_define(TRDLL, "DLL_Video_Seek",            dll_cdecl, ty_real, 1, ty_real  );
    global.tr_fn_loop        = external_define(TRDLL, "DLL_Video_SetLoop",         dll_cdecl, ty_real, 1, ty_real  );
    global.tr_fn_width       = external_define(TRDLL, "DLL_Video_GetWidth",        dll_cdecl, ty_real, 0           );
    global.tr_fn_height      = external_define(TRDLL, "DLL_Video_GetHeight",       dll_cdecl, ty_real, 0           );
    global.tr_fn_duration    = external_define(TRDLL, "DLL_Video_GetDuration",     dll_cdecl, ty_real, 0           );
    global.tr_fn_position    = external_define(TRDLL, "DLL_Video_GetPosition",     dll_cdecl, ty_real, 0           );
    global.tr_fn_status      = external_define(TRDLL, "DLL_Video_GetStatus",       dll_cdecl, ty_real, 0           );
    global.tr_fn_parse       = external_define(TRDLL, "DLL_ParseThumbs",           dll_cdecl, ty_real, 1, ty_string);
    global.tr_fn_filesize    = external_define(TRDLL, "DLL_GetFileSize",           dll_cdecl, ty_real, 1, ty_string);
    global.tr_fn_getdur      = external_define(TRDLL, "DLL_GetVideoDuration",      dll_cdecl, ty_real, 1, ty_string);
    global.tr_fn_set_target  = external_define(TRDLL, "DLL_Video_SetTargetBuffer", dll_cdecl, ty_real, 1, ty_string);
    global.tr_fn_copy_frame  = external_define(TRDLL, "DLL_Video_CopyFrame",      dll_cdecl, ty_real, 1, ty_real  );
    global.tr_fn_set_volume  = external_define(TRDLL, "DLL_Video_SetVolume",      dll_cdecl, ty_real, 1, ty_real  );
    global.tr_fn_get_volume  = external_define(TRDLL, "DLL_Video_GetVolume",      dll_cdecl, ty_real, 0           );
    global.tr_fn_has_audio   = external_define(TRDLL, "DLL_Video_HasAudio",       dll_cdecl, ty_real, 0           );
    global.tr_fn_has_video   = external_define(TRDLL, "DLL_Video_HasVideo",       dll_cdecl, ty_real, 0           );
    show_debug_message("[TinyROLL] DLL iniciada!");
}

function tinyroll_cmd(call, cmd)
{
	if is_undefined(cmd){};
	switch(call)
	{
		case "open"		 : return external_call(global.tr_fn_open	   , cmd);
		case "close"	 : return external_call(global.tr_fn_close	   , cmd);
		case "play"		 : return external_call(global.tr_fn_play			);
		case "pause"	 : return external_call(global.tr_fn_pause			);
		case "seek"		 : return external_call(global.tr_fn_seek	   , cmd);
		case "loop"		 : return external_call(global.tr_fn_loop	   , cmd);
		case "width"	 : return external_call(global.tr_fn_width			);
		case "height"	 : return external_call(global.tr_fn_height			);
		case "duration"	 : return external_call(global.tr_fn_duration		);
		case "position"	 : return external_call(global.tr_fn_position		);
		case "status"	 : return external_call(global.tr_fn_status			);
		case "parse"	 : return external_call(global.tr_fn_parse	   , cmd);
		case "filesize"	 : return external_call(global.tr_fn_filesize  , cmd);
		case "getdur"	 : return external_call(global.tr_fn_getdur	   , cmd);
		case "set_target": return external_call(global.tr_fn_set_target, cmd);
		case "copy_frame": return external_call(global.tr_fn_copy_frame, cmd);
		case "set_volume": return external_call(global.tr_fn_set_volume, cmd);
		case "get_volume": return external_call(global.tr_fn_get_volume		);
		case "has_audio" : return external_call(global.tr_fn_has_audio		);
		case "has_video" : return external_call(global.tr_fn_has_video		);
	}
}

function tinyroll_parse_thumbs(folder  ) {return tinyroll_cmd("parse", folder);		}
function tinyroll_get_filesize(filepath) {return tinyroll_cmd("filesize", filepath);}
function tinyroll_get_duration(filepath) {return tinyroll_cmd("getdur", filepath);	}
function tinyroll_set_volume(vol)        {return tinyroll_cmd("set_volume", vol);	}
function tinyroll_get_volume()           {return tinyroll_cmd("set_volume");		}

function tinyroll_draw_letterbox(surface, area_x, area_y, area_w, area_h)
{
    var vid_w   = surface_get_width(surface);
    var vid_h   = surface_get_height(surface);
    var scale_x = area_w / vid_w;
    var scale_y = area_h / vid_h;
    var scale   = min(scale_x, scale_y);
    var draw_w  = floor(vid_w * scale);
    var draw_h  = floor(vid_h * scale);
    var draw_x  = area_x + floor((area_w - draw_w) / 2);
    var draw_y  = area_y + floor((area_h - draw_h) / 2);
    draw_surface_stretched(surface, draw_x, draw_y, draw_w, draw_h);
}

function tinyroll_format_size(bytes)
{
    if bytes >= 1073741824 { return string(round(bytes / 1073741824 * 10) / 10) + "GB"; }
    if bytes >= 1048576    { return string(round(bytes / 1048576    * 10) / 10) + "MB"; }
    if bytes >= 1024       { return string(round(bytes / 1024       * 10) / 10) + "KB"; }
    return string(bytes) + " B";
}
