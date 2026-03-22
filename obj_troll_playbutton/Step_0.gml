image_index = ThmGrID mod 3;

switch(obj_troll_core.fullscreen)
{
	case 0: x = myX; y = myY; break;
	case 1: x = BsmLeft + BsWidth/2; y = BsmtTop + BHeight/2; break;
}

switch(obj_troll_core.status) 
{
	case video_status_playing: if sel == 0{visible = 0;}else{visible = 1;}
	sprite_index = spr_troll_playbutton; break;
	case video_status_paused: sprite_index = spr_troll_pausebutton; 
	visible = 1; break;
}