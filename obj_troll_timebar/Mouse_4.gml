// Left Pressed:
var range = (bbox_right - bbox_left) - 14;
if range > 0
{
    var new_ratio   = clamp((mouse_x - bbox_left - 7) / range, 0, 1);
    var seek_target = new_ratio * TROLL_STT_TTime;
    tinyroll_cmd("seek", seek_target);
    TROLL_STT_CrPos = tinyroll_cmd("position");
}