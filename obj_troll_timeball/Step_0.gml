image_index = ThmGrID mod 3;
image_alpha = obj_troll_shadow.image_alpha;

var bar   = obj_troll_timebar;
var range = (bar.bbox_right - bar.bbox_left) - 14;

// Garante que o drag é cancelado se o botão foi solto fora da ball
if dragging && !mouse_check_button(mb_left)
{
    var new_ratio   = clamp((mouse_x - bar.bbox_left - 7) / range, 0, 1);
    var seek_target = new_ratio * TROLL_STT_TTime;
    tinyroll_cmd("seek", seek_target);
    // Atualiza globalmente, não só local
    TROLL_STT_CrPos = tinyroll_cmd("position");
    dragging = 0;
    sel      = 0;
}

// Posição: segue mouse durante drag, senão segue tempo real
if dragging
{
    sel = 2;
    if range > 0
        x = clamp(mouse_x, bar.bbox_left + 7, bar.bbox_right - 7);
    y = bar.y;
}
else
{
    var ratio = (TROLL_STT_TTime > 0) ? clamp(TROLL_STT_CrPos / TROLL_STT_TTime, 0, 1) : 0;
    x = bar.bbox_left + 7 + floor(ratio * range);
    y = bar.y;
}