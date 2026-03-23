with (obj_troll_base)
{
    if scroll_offset > 0
    {
        scroll_offset--;
        for (var i = 0; i < array_length(selectors); i++)
        {
            selectors[i].context = i + scroll_offset;
        }
    }
}