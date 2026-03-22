status = video_get_status();
video_enable_loop(loop);

if (play)
{
	video_resume();
	play = 0;
}

if (pause)
{
	video_pause();
	pause = 0;
}