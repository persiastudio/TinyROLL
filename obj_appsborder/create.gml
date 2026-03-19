image_xscale = BsWidth / 12;
image_yscale = BHeight / 12;
switch(context)
{
	case 0: instance_create_layer(x + 5, y + 5, layer, obj_tinyroll_background,{Layer: Layer, Box: Box}); break;
}

//O contexto é fornecido pelo abridor de apps, que indica qual app a borda deve abrir.
//Mais de um app usa a borda.
