if (krnl_check_blocking() || !krnl_box_get_active(krnl_guess_box(Box))){exit;}
switch(TROLL_STT_Stats)
{
    case 0: TROLL_CMD_VPlay  = 1; break;
	case 1: TROLL_CMD_Pause  = 1; break;
    case 2: TROLL_CMD_VPlay  = 1; break;
}