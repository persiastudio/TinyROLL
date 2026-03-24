image_index = ThmGrID mod 3;

if keyboard_check_pressed(vk_down)
{
    holding = 1;
    held_ms = 0;
    with (obj_troll_base)
    {
        if scroll_offset + array_length(selectors) < video_count
        {
            scroll_offset++;
            for (var i = 0; i < array_length(selectors); i++) { selectors[i].context = i + scroll_offset; }
        }
    }
}

if keyboard_check_released(vk_down) { holding = 0; held_ms = 0; }

if holding && keyboard_check(vk_down)
{
    held_ms += delta_time / 1000;
    if held_ms >= 500 && held_ms mod (1000/10) < (delta_time / 1000)
    {
        with (obj_troll_base)
        {
            if scroll_offset + array_length(selectors) < video_count
            {
                scroll_offset++;
                for (var i = 0; i < array_length(selectors); i++) { selectors[i].context = i + scroll_offset; }
            }
        }
    }
}