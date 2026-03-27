sel = 2;
var bar   = obj_troll_timebar;
var range = (bar.bbox_right - bar.bbox_left) - 14;
if range <= 0 { exit; }
var new_ratio   = clamp((mouse_x - bar.bbox_left - 7) / range, 0, 1);
var seek_target = new_ratio * TROLL_STT_TTime;
tinyroll_cmd("seek", seek_target);
TROLL_STT_CrPos = tinyroll_cmd("position");