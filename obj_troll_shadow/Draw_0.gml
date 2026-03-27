draw_self();

// Desenhar barra de tempo e textos
draw_set_font(fnt_carlito_12b);
draw_set_colour(ThmText);
draw_set_halign(fa_left);

var time = tinyecho_format_duration(TROLL_STT_CrPos) + "/" + tinyecho_format_duration(TROLL_STT_TTime);

draw_set_alpha(image_alpha);
switch(TROLL_CFG_FScrn)
{
	case 0: draw_text(x - (sprite_width/2) + 24, y + 24, time); break;
	case 1: draw_text(BsmLeft + 24, BBottom - 48, time); break;
}
draw_set_alpha(1);