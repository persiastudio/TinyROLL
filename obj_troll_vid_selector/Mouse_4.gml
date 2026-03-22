//Seleciona o vídeo (manda pro core a pasta relacionada ao context)
image_index = 2;

obj_troll_vframe.v_path = "C:\\Users\\gabri\\AppData\\Local\\TinyBox_OS\\Users\\[0] - User 0\\Videos\\1774101643549.mp4";
show_message(string(obj_troll_vframe.v_path));
obj_troll_vframe.vid = context;
obj_troll_vframe.changed = 1;