image_index = ThmGrID mod 3;

if TROLL_CFG_FScrn
{
    image_xscale = (BsWidth - 48) / 580;
    x = BsmLeft + BsWidth / 2;
    y = BBottom - 53;
}
else
{
    image_xscale = (obj_troll_vframe.sprite_width - 48) / 580;
    x = obj_troll_vframe.x + obj_troll_vframe.sprite_width / 2;
    y = obj_troll_vframe.bbox_bottom - 53;
}