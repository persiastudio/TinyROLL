switch(video_get_status())
{
	case video_status_paused : play  = 1; break;
	case video_status_playing: pause = 1; break;
}