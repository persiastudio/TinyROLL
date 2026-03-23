image_yscale = (obj_troll_downarrow.bbox_top - bbox_top) / sprite_get_height(sprite_index);
instance_create_depth(x,y,depth - 1, obj_troll_scroll, {Layer: Layer, Box: Box});