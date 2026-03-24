if fid.context == last_context { exit; }
last_context = fid.context;

var base       = obj_troll_base;
if fid.context >= array_length(base.videos) { exit; }

thumb_path = game_save_id + UsrPath[LgdUser] + "//Videos//Thumbs//" + base.videos[fid.context].name + ".png";

if thumb_sprite != -1
{
    sprite_index = spr_troll_thumb;
    sprite_delete(thumb_sprite);
    thumb_sprite = -1;
}

if file_exists(thumb_path)
{
	thumb_sprite = sprite_add(thumb_path, 1, false, false, 0, 0);
}