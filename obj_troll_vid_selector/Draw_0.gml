draw_self();
draw_set_font(fnt_carlito_9b);
draw_set_colour(ThmText);
draw_set_halign(fa_left);
draw_text(x + 74, y + 16, tb_text_ellipsis(obj_troll_base.videos[context].name, 140));

draw_set_font(fnt_carlito_9);
draw_text(x + 74, y + 40, tinyecho_format_duration(obj_troll_base.videos[context].duration) + " | " + tinyroll_format_size(obj_troll_base.videos[context].size));