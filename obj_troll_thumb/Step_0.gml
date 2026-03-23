var base       = obj_troll_base;
var sel        = obj_troll_vid_selector;
var thumb_dir  = game_save_id + UsrPath[LgdUser] + "//Videos//Thumbs//";
var thumb_path = thumb_dir + base.videos[sel.context].name + ".png";

if thumb_sprite != -1 { sprite_delete(thumb_sprite); }

if file_exists(thumb_path)
{
    thumb_sprite = sprite_add(thumb_path, 1, false, false, 0, 0);
    sprite_index = thumb_sprite;
}
else
{
    sprite_index = spr_troll_thumb;
}

image_index = sel.image_index;