pause = 0;
stop = 0;
play = 0;
loop = 0;
fullscreen = 0;
aniso = 1;
status = video_get_status();

instance_create_layer(x + obj_troll_vframe.sprite_width - 48, y + obj_troll_vframe.sprite_height - 48, BmLayer[Box + 1], obj_troll_fsc, {Box: Box + 1, Layer: Layer});
instance_create_layer(x + obj_troll_vframe.sprite_width/2, y + obj_troll_vframe.sprite_height/2, BmLayer[Box + 1], obj_troll_playbutton, {Box: Box + 1, Layer: Layer});

video_open(v_path);
video_pause();