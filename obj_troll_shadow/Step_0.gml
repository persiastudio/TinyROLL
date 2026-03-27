var vframe = obj_troll_vframe;
var core   = obj_troll_core;

if TROLL_CFG_FScrn
{
    image_xscale = BsWidth / sprite_get_width(sprite_index);
    x = BsmLeft + BsWidth / 2;
    y = BBottom - 64;
}
else
{
    image_xscale = (vframe.sprite_width - 8) / sprite_get_width(sprite_index);
    x = vframe.x + vframe.sprite_width / 2;
    y = vframe.y + vframe.sprite_height / 2 - 68;
}

var mouse_over = point_in_rectangle(mouse_x, mouse_y, bbox_left, bbox_top, bbox_right, bbox_bottom);

if mouse_over
{
    visible_state = 1;
    fade_timer    = 0;
    image_alpha   = 1;
}
else if visible_state == 1
{
    fade_timer += delta_time;

    if fade_timer >= wait_duration
    {
        var fade_progress = clamp((fade_timer - wait_duration) / fade_duration, 0, 1);
        var alpha = 1 - fade_progress;
        image_alpha = alpha;

        if fade_progress >= 1
        {
            visible_state = 0;
            fade_timer    = 0;
            image_alpha   = 0;
        }
    }
}