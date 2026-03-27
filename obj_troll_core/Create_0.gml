if (tinyroll_cmd("open", TROLL_STT_VPath) <= 0) 
{
    show_debug_message("[TinyROLL] Erro ao abrir vídeo!");
    instance_destroy();	exit;
}

vw = external_call(global.tr_fn_width);
vh = external_call(global.tr_fn_height);

tr_surface  = surface_create(tinyroll_cmd("width"), tinyroll_cmd("height"));
tr_buf_size = vw * vh * 4;
tr_buffer   = buffer_create(tr_buf_size, buffer_fixed, 1);

// Setar o endereço fixo para a DLL
tinyroll_cmd("set_target", buffer_get_address(tr_buffer));

TROLL_STT_TTime = tinyroll_cmd("duration");
TROLL_STT_Stats = tinyroll_cmd("status");
TROLL_STT_CrPos = 0;

instance_create_layer(
    obj_troll_vframe.x + obj_troll_vframe.sprite_width / 2,
    obj_troll_vframe.bbox_bottom - 26,
    BmLayer[Box + 1], obj_troll_shadow, {Box: Box + 1, Layer: Layer}
);

instance_create_layer(x + obj_troll_vframe.sprite_width / 2,  y + obj_troll_vframe.sprite_height / 2, BmLayer[Box + 1], obj_troll_playbutton, {Box: Box + 1, Layer: Layer});
instance_create_layer(BsmLeft, BsmtTop, BmLayer[Box], obj_troll_black, {Box: Box, Layer: Layer});