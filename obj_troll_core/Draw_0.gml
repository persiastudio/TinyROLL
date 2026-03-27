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