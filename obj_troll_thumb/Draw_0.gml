draw_self();

if file_exists(thumb_path)
{
    draw_sprite_stretched(thumb_sprite, 0, x + 1, y + 1, sprite_width - 2, sprite_height - 2);
}