sel = 2;

var bar     = obj_troll_scrollbar;
var base    = obj_troll_base;
var range   = bar.sprite_height * bar.image_yscale - sprite_height;
if range <= 0 { exit; }
var ratio    = clamp((mouse_y - bar.y) / range, 0, 1);
var max_off  = max(1, base.video_count - base.visible_count);
var new_off  = round(ratio * max_off);
if new_off != base.scroll_offset
{
    base.scroll_offset = new_off;
    for (var i = 0; i < array_length(base.selectors); i++) { base.selectors[i].context = i + base.scroll_offset; }
}