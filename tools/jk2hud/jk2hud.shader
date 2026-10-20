// JK2 HUD shaders (cg_hudFiles 2 or ui/jk2hud.txt).
// Same layers as the Jedi Outcast gfx.shader; the HUD layout comes from the JK2 HUD of jaPRO (GPLv2). The names are new because the JKA and SWGL
// pk3 files replace gfx/hud/hudleft and the gfx/hud/* shaders of JKO.
// gfx/hud/jk2hudleft is a copy of the hudleft image of 0_JKO_Assets0.pk3 (made by make_jk2hud_pk3.ps1).

jk2hud/leftframe
{
	nopicmip
	cull	disable
	{
		map gfx/hud/static5
		blendFunc GL_ONE GL_ONE
		rgbGen wave inversesawtooth 0 1.5 1.4 1
		tcMod scroll 0 1
	}
	{
		map gfx/hud/static8
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
	{
		clampmap gfx/hud/jk2hudleft
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
	}
}

jk2hud/leftinner
{
	nopicmip
	cull	disable
	{
		clampmap gfx/hud/hudleft_innerframe
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
	}
}

jk2hud/rightframe
{
	nopicmip
	cull	disable
	{
		map gfx/hud/static5
		blendFunc GL_ONE GL_ONE
		rgbGen wave inversesawtooth 0 1.5 1.4 1
		tcMod scroll 0 1
	}
	{
		map gfx/hud/static9
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
	{
		clampmap gfx/hud/hudrightframe
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
	}
}

jk2hud/rightinner
{
	nopicmip
	cull	disable
	{
		clampmap gfx/hud/hudright_innerframe
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
	}
}

jk2hud/prong_off
{
	nopicmip
	cull	disable
	{
		clampmap gfx/hud/prong_off
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
	}
}

jk2hud/prong_on_w
{
	nopicmip
	cull	disable
	{
		clampmap gfx/hud/prong_on_w
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
	}
}

jk2hud/prong_on_f
{
	nopicmip
	cull	disable
	{
		clampmap gfx/hud/prong_on_f
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
	}
}

jk2hud/prong_on_i
{
	nopicmip
	cull	disable
	{
		clampmap gfx/hud/prong_on_i
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
	}
}

jk2hud/background
{
	q3map_nolightmap
	cull	disable
	{
		clampmap gfx/hud/background
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
		alphaGen vertex
	}
	{
		map gfx/hud/static_gold
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		rgbGen wave square 0 1 0.5 3
		tcMod scroll 0.3 0
	}
	{
		map gfx/hud/static_gold
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		rgbGen wave square 0 1 0 3
		tcMod scroll -0.3 0
	}
	{
		map gfx/hud/static_gold
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		detail
		tcMod scroll 1 0
	}
	{
		map gfx/hud/static_gold
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		detail
		tcMod scroll -1 0
	}
}

jk2hud/background_f
{
	q3map_nolightmap
	cull	disable
	{
		clampmap gfx/hud/background_f
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	}
	{
		map gfx/hud/static4
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		rgbGen wave square 0 1 0.5 3
		tcMod scroll 0.3 0
	}
	{
		map gfx/hud/static4
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		rgbGen wave square 0 1 0 3
		tcMod scroll -0.3 0
	}
	{
		map gfx/hud/static4
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		detail
		tcMod scroll 1 0
	}
	{
		map gfx/hud/static4
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		detail
		tcMod scroll -1 0
	}
}

jk2hud/background_i
{
	q3map_nolightmap
	cull	disable
	{
		clampmap gfx/hud/background_i
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	}
	{
		map gfx/hud/static_green
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		rgbGen wave square 0 1 0.5 3
		tcMod scroll 0.3 0
	}
	{
		map gfx/hud/static_green
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		rgbGen wave square 0 1 0 3
		tcMod scroll -0.3 0
	}
	{
		map gfx/hud/static_green
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		detail
		tcMod scroll 1 0
	}
	{
		map gfx/hud/static_green
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		detail
		tcMod scroll -1 0
	}
}

jk2hud/saber_fast
{
	nopicmip
	nomipmaps
	cull	disable
	{
		map gfx/hud/saber_stylesfast
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}

jk2hud/saber_med
{
	nopicmip
	nomipmaps
	cull	disable
	{
		map gfx/hud/saber_stylesmed
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}

jk2hud/saber_strong
{
	nopicmip
	nomipmaps
	cull	disable
	{
		map gfx/hud/saber_stylesstrong
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}

// Not in JK2: the strong arc recolored by make_jk2hud_pk3.ps1
jk2hud/saber_desann
{
	nopicmip
	nomipmaps
	cull	disable
	{
		map gfx/hud/jk2saber_desann
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}

jk2hud/saber_tavion
{
	nopicmip
	nomipmaps
	cull	disable
	{
		map gfx/hud/jk2saber_tavion
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}
