if (krnl_check_blocking() || !krnl_box_get_active(krnl_guess_box(Box)) || TROLL_CFG_FScrn){exit;}
image_index = 2;
var base = obj_troll_base;
obj_troll_vframe.v_path  = base.videos[context].path;
obj_troll_vframe.vid     = context;
obj_troll_vframe.changed = 1;
TROLL_STT_VPath = base.videos[context].path;
TROLL_STT_VName = base.videos[context].name;
TROLL_STT_VSize = base.videos[context].size;
TROLL_STT_TTime = base.videos[context].duration;