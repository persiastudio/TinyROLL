TROLL_STT_Stats = video_get_status();
video_enable_loop(TROLL_CFG_VLoop);

if TROLL_CMD_VPlay { video_resume(); TROLL_CMD_VPlay  = 0; }
if TROLL_CMD_Pause { video_pause();  TROLL_CMD_Pause  = 0; }
if TROLL_CMD_Rewnd { video_seek_to(max(0, video_get_position() - TROLL_CFG_RFAmt * 1000)); TROLL_CMD_Rewnd = 0; }
if TROLL_CMD_FFwrd { video_seek_to(video_get_position() + TROLL_CFG_RFAmt * 1000); TROLL_CMD_FFwrd = 0;			}