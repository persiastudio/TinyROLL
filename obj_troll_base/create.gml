image_yscale = obj_tinyroll_background.image_yscale;

/*	Aqui, precisamos de um jeito de:
	1. Descobrir a quantidade de vídeos presentes na pasta
	2. Descobrir como criar os objetos (obj_troll_vid_selector) 
	de acordo com a quantidade de vídeos na pasta, e: 
	3. Como calcular quantos obj_troll_vid_selector cabem no interior desse
	objeto aqui, visto que ele se expande no eixo Y, e também, cada 
	obj_troll_vid_selector têm 2 de espaço entre o fim de um e início de outro.
	O primeiro precisa ser criado em: 
	*/
	
instance_create_layer(x + 5, y + 86, BmLayer[Box + 2], obj_troll_vid_selector, {context: 0, Layer: Layer, Box: Box});

/*	x + 5, y + 86. Todo o resto segue x + 5, mas o y que muda. Cada objeto desse
	tem 68 de altura, então, você já sabe a base pra calcular. Ah, esse objeto aqui
	tem 647 de altura sem scaling, mas como vc viu, os objetos vid_selector começam
	a partir do y + 86. Por que? Ah, porque tem o logo do TinyROLL, então depois desse
	logo que o resto é criado.
	*/
