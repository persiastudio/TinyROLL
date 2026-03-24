image_index = ThmGrID mod 3;

var base    = obj_troll_base;
var bar     = obj_troll_scrollbar;
var max_off = max(1, base.video_count - base.visible_count);
var ratio   = base.scroll_offset / max_off;
var range   = bar.sprite_height * bar.image_yscale - sprite_height;
y = bar.y + floor(ratio * range);