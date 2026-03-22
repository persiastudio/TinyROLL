image_xscale = (BsWidth - 10) / 16;
image_yscale = (BHeight - 10) / 16;

instance_create_layer(x, y, BmLayer[Box + 1], obj_troll_base, {Layer: Layer, Box: Box + 1});
var sx = (BsWidth - 10 - 241 - 7) / sprite_get_width(spr_troll_vframe);
instance_create_layer(x + 241, y - 10 + BHeight / 2, BmLayer[Box + 1], obj_troll_vframe, {Layer: Layer, Box: Box + 1});