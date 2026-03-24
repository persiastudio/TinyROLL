function trtparser_init()
{
    #macro TRTDLL "TRTParser.dll"
    global.trt_fn_parse    = external_define(TRTDLL, "DLL_ParseThumbs", dll_cdecl, ty_real, 1, ty_string);
    global.trt_fn_duration = external_define(TRTDLL, "DLL_GetVideoDuration", dll_cdecl, ty_real, 1, ty_string);
    global.trt_fn_filesize = external_define(TRTDLL, "DLL_GetFileSize", dll_cdecl, ty_real, 1, ty_string);
	show_debug_message("[TRTParser] DLL iniciada!");
}

function trtparser_parse(folder)
{
    return external_call(global.trt_fn_parse, folder);
}

function trtparser_get_duration(filepath)
{
    return external_call(global.trt_fn_duration, filepath);
}

function trtparser_get_filesize(filepath)
{
    return external_call(global.trt_fn_filesize, filepath);
}

function tinyroll_draw_letterbox(surface, area_x, area_y, area_w, area_h)
{
    var vid_w  = surface_get_width(surface);
    var vid_h  = surface_get_height(surface);
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
    if bytes >= 1073741824 { return string(round(bytes / 1073741824 * 10) / 10) + " GB"; }
    if bytes >= 1048576    { return string(round(bytes / 1048576    * 10) / 10) + " MB"; }
    if bytes >= 1024       { return string(round(bytes / 1024       * 10) / 10) + " KB"; }
    return string(bytes) + " B";
}