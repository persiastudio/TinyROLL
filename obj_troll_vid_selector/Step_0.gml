if (krnl_check_blocking() || !krnl_box_get_active(krnl_guess_box(Box)) || TROLL_CFG_FScrn){exit;}
if image_index != 1
{
	if (obj_troll_vframe.vid == context){image_index = 2;}else{image_index = 0;}
}
thumb_inst.image_index = image_index;