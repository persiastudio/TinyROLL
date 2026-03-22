var _data = video_draw();
if (!_data[0] && status == video_status_playing)
{
    var _surface = _data[1];
    var _w = surface_get_width(_surface);
    var _h = surface_get_height(_surface);
	gpu_set_tex_filter(aniso);
	switch(fullscreen)
	{
		case 0: draw_surface_stretched
		(
			_surface, x, y, 
			obj_troll_vframe.sprite_width - 8, obj_troll_vframe.sprite_height - 8
		);	break;
		
		case 1: draw_surface_stretched
		(
			_surface, BsmLeft, BsmtTop, 
			BsWidth, BHeight
		);	break;
	}
	gpu_set_tex_filter(0);
}