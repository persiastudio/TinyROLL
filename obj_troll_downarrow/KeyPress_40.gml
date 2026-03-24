with (obj_troll_base)
{
    if scroll_offset + array_length(selectors) < video_count
    {
        scroll_offset++;
        for (var i = 0; i < array_length(selectors); i++)
        {
            selectors[i].context = i + scroll_offset;
        }
    }
}