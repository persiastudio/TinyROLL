fade_timer    = 0;
wait_duration = 2000000;
fade_duration = 500000;
visible_state = 0;
image_alpha   = 0;

var vframe = obj_troll_vframe;
var core   = obj_troll_core;

x = vframe.x + vframe.sprite_width / 2;
y = vframe.y + vframe.sprite_height / 2 - 4; // bbox_bottom do vframe - 4, origem middle-top

instance_create_layer
(
    vframe.x + vframe.sprite_width / 2,
    vframe.y + vframe.sprite_height / 2 - 55,
    BmLayer[Box + 1], obj_troll_timebar, {Box: Box + 1, Layer: Layer}
);

instance_create_layer
(
    core.x + vframe.sprite_width - 48,
    core.y + vframe.sprite_height - 48,
    BmLayer[Box + 1], obj_troll_fsc, {Box: Box + 1, Layer: Layer}
);

instance_create_layer
(
    core.x + vframe.sprite_width - 88,
    core.y + vframe.sprite_height - 48,
    BmLayer[Box + 1], obj_troll_loop, {Box: Box + 1, Layer: Layer}
);