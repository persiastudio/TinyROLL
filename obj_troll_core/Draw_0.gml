/// @description Render
if (surface_exists(tr_surface) && buffer_exists(tr_buffer))
{
    // Sincroniza o buffer com a surface
    buffer_set_surface(tr_buffer, tr_surface, 0);
    
    gpu_set_tex_filter(TROLL_CFG_Aniso);
    
    // Desenho com Letterbox
    if (TROLL_CFG_FScrn == 0) 
	{
        tinyroll_draw_letterbox(tr_surface, x, y, obj_troll_vframe.sprite_width - 8, obj_troll_vframe.sprite_height - 8);
    } 
	else 
	{
        tinyroll_draw_letterbox(tr_surface, BsmLeft, BsmtTop, BsWidth, BHeight);
    }
    
    gpu_set_tex_filter(0);
}

// Desenhar barra de tempo e textos
draw_set_font(fnt_carlito_12b);
draw_set_colour(ThmText);
draw_set_halign(fa_left);

var _time_string = tinyecho_format_duration(TROLL_STT_CrPos) + "/" + tinyecho_format_duration(TROLL_STT_TTime);
switch(TROLL_CFG_FScrn)
{
	case 0: draw_text(x + obj_troll_vframe.bbox_left - 294, y + obj_troll_vframe.sprite_height - 48, _time_string); break;
	case 1: draw_text(BsmLeft + 24, BBottom - 48, _time_string); break;
}
draw_set_halign(fa_left);