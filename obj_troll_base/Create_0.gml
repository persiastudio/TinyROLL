show_debug_message("[PATH] game_save_id: " + game_save_id);
show_debug_message("[PATH] UsrPath: " + UsrPath[LgdUser]);
var video_folder = game_save_id + UsrPath[LgdUser] + "\\Videos";
tinyroll_parse_thumbs(video_folder);

videos        = [];
scroll_offset = 0;
selectors     = [];

var f = file_find_first(video_folder + "\\*.mp4", 0);
while (f != "")
{
    var full_path = video_folder + "\\" + f;
	show_debug_message("[TinyROLL] Parsed: " + f);
    array_push(videos, 
	{
        path    : full_path						  ,
        name    : filename_change_ext  (f, ""	 ),
        size    : tinyroll_get_filesize(full_path),
        duration: tinyroll_get_duration(full_path)
    });
    f = file_find_next();
}
file_find_close();

video_count   = array_length(videos);
image_yscale  = obj_tinyroll_background.sprite_height / sprite_get_height(sprite_index);

var available_h = (sprite_get_height(sprite_index) * image_yscale) - 80;
var item_h      = 70;
visible_count   = floor(available_h / item_h);

for (var i = 0; i < min(visible_count, video_count); i++)
{
    var inst = instance_create_layer(x + 4, y + 80 + i * item_h, BmLayer[Box + 1], obj_troll_vid_selector, {context: i, Layer: Layer, Box: Box + 1});
    array_push(selectors, inst);
}

instance_create_layer(bbox_right, y,               BmLayer[Box], obj_troll_uparrow,   {Layer: Layer, Box: Box});
instance_create_layer(bbox_right, bbox_bottom - 10, BmLayer[Box], obj_troll_downarrow, {Layer: Layer, Box: Box});
instance_create_layer(bbox_right, obj_troll_uparrow.bbox_bottom, BmLayer[Box], obj_troll_scrollbar, {Layer: Layer, Box: Box});