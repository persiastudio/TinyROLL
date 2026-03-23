var video_folder = game_save_id + UsrPath[LgdUser] + "//Videos";
var video_list   = [];
var f = file_find_first(video_folder + "//*.mp4", 0);
while (f != "")
{
    array_push(video_list, {
    path    : video_folder + "//" + f,
    name    : filename_change_ext(f, ""),
    size	: trtparser_get_filesize(video_folder + "//" + f),
    duration: trtparser_get_duration(video_folder + "//" + f)
	});
    f = file_find_next();
}
file_find_close();

video_count    = array_length(video_list);
videos         = video_list;
scroll_offset  = 0;
visible_count  = floor((647 - 86) / 70); // 70 = 68 altura + 2 espaço

image_yscale = obj_tinyroll_background.sprite_height / sprite_get_height(sprite_index);
trtparser_parse(video_folder);

selectors = [];
for (var i = 0; i < min(visible_count, video_count); i++)
{
    var inst = instance_create_layer(x + 4, y + 86 + i * 70, BmLayer[Box + 1], obj_troll_vid_selector, {context: i, Layer: Layer, Box: Box + 1});
    array_push(selectors, inst);
}

instance_create_layer(bbox_right, y,              BmLayer[Box], obj_troll_uparrow,  {Layer: Layer, Box: Box});
instance_create_layer(bbox_right, bbox_bottom - 10, BmLayer[Box], obj_troll_downarrow, {Layer: Layer, Box: Box});
instance_create_layer(bbox_right, obj_troll_uparrow.bbox_bottom, BmLayer[Box], obj_troll_scrollbar, {Layer: Layer, Box: Box});