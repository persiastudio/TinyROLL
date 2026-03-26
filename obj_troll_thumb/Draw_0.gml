draw_self();

if file_exists(thumb_path)
{
    gpu_set_texfilter(1);
	draw_sprite_stretched(thumb_sprite, 0, x + 1, y + 1, sprite_width - 2, sprite_height - 2);
	gpu_set_texfilter(0);
}