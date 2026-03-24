image_speed  = 0;
thumb_sprite = -1;
last_context = -1;

var base      = obj_troll_base;
var thumb_dir = game_save_id + UsrPath[LgdUser] + "//Videos//Thumbs//";
thumb_path = thumb_dir + base.videos[context].name + ".png";

if file_exists(thumb_path)
{
    thumb_sprite = sprite_add(thumb_path, 1, false, false, 0, 0);
    sprite_index = thumb_sprite;
}

show_debug_message("[THUMB] context: " + string(context));
show_debug_message("[THUMB] path: " + thumb_path);
show_debug_message("[THUMB] exists: " + string(file_exists(thumb_path)));