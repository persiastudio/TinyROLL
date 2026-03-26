// 1. Comandos de Interface
if TROLL_CMD_VPlay { tinyroll_cmd("play"); TROLL_CMD_VPlay = 0; }
if TROLL_CMD_Pause { tinyroll_cmd("pause"); TROLL_CMD_Pause = 0; }

if TROLL_CMD_Rewnd 
{ 
    var target = max(0, TROLL_STT_CrPos - TROLL_CFG_RFAmt);
	tinyroll_cmd("seek", target);
    TROLL_STT_CrPos = external_call(global.tr_fn_position); // atualiza imediatamente
    TROLL_CMD_Rewnd = 0; 
}

if TROLL_CMD_FFwrd 
{ 
    var target = min(TROLL_STT_TTime, TROLL_STT_CrPos + TROLL_CFG_RFAmt);
    tinyroll_cmd("seek", target);
    TROLL_STT_CrPos = external_call(global.tr_fn_position); // atualiza imediatamente
    TROLL_CMD_FFwrd = 0; 
}

// 2. Controle de Loop e Volume
tinyroll_cmd("loop", TROLL_CFG_VLoop);
tinyroll_cmd("set_volume", TROLL_CFG_AuVol);


// 3. Update de Status e Frame
TROLL_STT_Stats = tinyroll_cmd("status");
TROLL_STT_CrPos = tinyroll_cmd("position");

// Copia o frame da DLL para o buffer do GM
tinyroll_cmd("copy_frame", tr_buf_size);