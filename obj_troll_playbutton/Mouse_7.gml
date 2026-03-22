switch(obj_troll_core.status)
{
	case video_status_playing: obj_troll_core.pause = 1; break;
	case video_status_paused : obj_troll_core.play  = 1; break;
}