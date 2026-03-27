if (krnl_check_blocking() || !krnl_box_get_active(krnl_guess_box(Box))){exit;}
image_index = ThmGrID mod 3;
switch(TROLL_CFG_FScrn)
{
    case 0: x = myX; y = myY; break;
    case 1: x = BsmLeft + BsWidth / 2; y = BsmtTop + BHeight / 2; break;
}

switch(TROLL_STT_Stats)
{
    case 0: visible = 1; sprite_index = spr_troll_playbutton;  break;
	case 1: if sel == 0 { visible = 0; } else { visible = 1; } sprite_index = spr_troll_playbutton;  break;
    case 2: sprite_index = spr_troll_pausebutton; visible = 1; break;
}