image_xscale = (obj_troll_vframe.sprite_width - 48) / 580;
x = obj_troll_vframe.x + obj_troll_vframe.sprite_width / 2;
y = obj_troll_vframe.bbox_bottom - 53;
instance_create_layer(x, y, BmLayer[Box + 1], obj_troll_timeball, {Box: Box + 1});