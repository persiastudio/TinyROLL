function internal_app_open(app_id)
{
	var fbx = krnl_box_find_free();
	if fbx != -1
    {
        if (krnl_box_prepare(fbx, app_id))
        {
            var c = 3 + (fbx - 1) * 5;
            internal_app_run(app_id, c);
        }
    }
	else
	{
		show_message("[KRNL: BoxSys] No free Box available to open app.");
	}
}


function internal_app_run(app_id, box_layer)
{
	
	switch(app_id)
	{
		case 0: //TinyCEF
		{
			instance_create_layer
			(BsmLeft + 1, BsmtTop + 1, BmLayer[box_layer], db.app[app_id].object,{l: box_layer});
			break;
		}
		
		case 1: //TinyECHO
		{
			instance_create_layer
			(
				room_width/2, 
				room_height/2, 
				BmLayer[box_layer],
				db.app[app_id].object
			);
			break;
		}
		
		case 2: //
		{
			instance_create_layer
			(
				BsmLeft + BsWidth/2, 
				BsmtTop + BHeight/2,
				BmLayer[box_layer],
				db.app[app_id].object
			);
			break;
		}
		
		case 3: //
		{
			instance_create_layer
			(
				BsmLeft + BsWidth/2, 
				BsmtTop + BHeight/2, 
				BmLayer[box_layer],
				db.app[app_id].object
			);
			break;
		}
		//TinyPix
		case 4: instance_create_layer
		(
			BsmLeft, BsmtTop, BmLayer[box_layer],db.app[app_id].object
		);	break;
		
		case 5: //
		{
			instance_create_layer
			(
				BsmLeft + BsWidth/2, 
				BsmtTop + BHeight/2, 
				BmLayer[box_layer],
				db.app[app_id].object
			);
			break;
		}
		
		//TinyROLL
		case 6: instance_create_layer
		(BsmLeft, BsmtTop, BmLayer[box_layer], dbA[app_id].object,
		{context: 0, Layer: BmLayer[box_layer], Box: box_layer});break;
		
		default: instance_create_layer
		(BsmLeft, BsmtTop, BmLayer[box_layer],dbA[app_id].object); break;
	}
}

//Ignorar o código abaixo, vai ser pra outra coisa
/*function app_open(app_id,xx,yy)
{
    var app_name = global.app_name[app_id];

    // Procura primeira box livre (1 a 13)
    for (var i = 1; i <= 13; i++)
    {
        if (!variable_global_exists("boxstatus")) global.box_status = array_create(14, 0);

        if (global.box_status[i] == 0)
        {
            global.box[i] = app_id;
            global.box_name[i] = app_name;
            global.box_status[i] = 1;

            // Cria instância do app na layer da box
            var layername = "box_" + string(i);
            instance_create_layer(xx, yy, layername, obj_app_runner, { context: app_id });

            // Oculta a shelf e mostra só essa box
            layer_set_visible("basement", false);
            for (var j = 1; j <= 13; j++)
            {
                layer_set_visible("box_" + string(j), (j == i));
            }

            // Mantém sempre visíveis as camadas fixas
            layer_set_visible("BG", true);
            layer_set_visible("PAT", true);
            layer_set_visible("UI", true);

            break;
        }
    }
}
