var data = video_draw();
if (!data[0] && TROLL_STT_Stats == video_status_playing)
{
    var surf = data[1];
    gpu_set_tex_filter(TROLL_CFG_Aniso);
    switch(TROLL_CFG_FScrn)
    {
        case 0: tinyroll_draw_letterbox(surf, x, y, obj_troll_vframe.sprite_width - 8, obj_troll_vframe.sprite_height - 8); break;
        case 1: tinyroll_draw_letterbox(surf, BsmLeft, BsmtTop, BsWidth, BHeight); break;
    }
    gpu_set_tex_filter(0);
}

draw_set_font(fnt_carlito_12b);
draw_set_colour(ThmText);

draw_set_halign(fa_right);
draw_text(x + obj_troll_vframe.sprite_width - 16, y + obj_troll_vframe.sprite_height - 72,string(tinyecho_format_duration(video_get_position() / 1000)) + "/" + string(tinyecho_format_duration(TROLL_STT_TTime)));
draw_set_halign(fa_left);