draw_self();

draw_set_font(fnt_carlito_9b);
draw_set_colour(ThmText);

draw_text(x + 75, y + 16, tb_text_ellipsis(obj_troll_base.videos[context].name, 138));
draw_text(x + 75, y + 32, string(obj_troll_base.videos[context].duration) + ", " + string(obj_troll_base.videos[context].size));