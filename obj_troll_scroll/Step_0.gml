image_index = ThmGrID mod 3;

var base    = obj_troll_base;
var bar     = obj_troll_scrollbar;
var max_off = max(1, base.video_count - array_length(base.selectors));
var ratio   = base.scroll_offset / max_off;
var range   = (bar.bbox_bottom - bar.bbox_top) - sprite_height;
y = bar.bbox_top + floor(ratio * range);

if mouse_check_button_pressed(mb_left)
&& mouse_x >= bbox_left && mouse_x <= bbox_right
&& mouse_y >= bbox_top  && mouse_y <= bbox_bottom
{
    dragging = 1;
}

if mouse_check_button_released(mb_left)
{
    dragging = 0;
    sel      = 0;
}

if dragging && mouse_check_button(mb_left)
{
    sel = 2;
    if range <= 0 { exit; }
    var new_ratio = clamp((mouse_y - bar.bbox_top) / range, 0, 1);
    var new_off   = round(new_ratio * max_off);
    if new_off != base.scroll_offset
    {
        base.scroll_offset = new_off;
        for (var i = 0; i < array_length(base.selectors); i++) { base.selectors[i].context = i + base.scroll_offset; }
    }
}