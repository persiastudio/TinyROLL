image_index = ThmGrID mod 3;

var bar   = obj_troll_timebar;
var ratio = (TROLL_STT_TTime > 0) ? clamp(TROLL_STT_CrPos / TROLL_STT_TTime, 0, 1) : 0;
var range = (bar.bbox_right - bar.bbox_left) - 14;
x = bar.bbox_left + 7 + floor(ratio * range);
y = bar.y;

if mouse_check_button_pressed(mb_left)
&& mouse_x >= bbox_left && mouse_x <= bbox_right
&& mouse_y >= bbox_top  && mouse_y <= bbox_bottom
{
    dragging = 1;
}

if mouse_check_button_released(mb_left)
{
    dragging = 0;
}

if dragging && mouse_check_button(mb_left)
{
    sel = 2;
    if range <= 0 { exit; }
    var new_ratio   = clamp((mouse_x - bar.bbox_left - 7) / range, 0, 1);
    var seek_target = new_ratio * TROLL_STT_TTime;
    tinyroll_cmd("seek", seek_target);
    TROLL_STT_CrPos = tinyroll_cmd("position");
}