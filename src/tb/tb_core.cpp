// ================================================================================
// ==      This file is a part of Turbo Badger. (C) 2011-2014, Emil Segerås      ==
// ==                     See tb_core.h for more information.                    ==
// ================================================================================

#include "tb_core.h"
#include "tb_skin.h"
#include "tb_widgets_reader.h"
#include "tb_language.h"
#include "tb_font_renderer.h"
#include "tb_system.h"
#include "animation/tb_animation.h"
#include "image/tb_image_manager.h"

namespace tb {

TBRenderer *g_renderer;
TBSkin *g_tb_skin;
TBWidgetsReader *g_widgets_reader;
TBLanguage *g_tb_lng;
TBFontManager *g_font_manager;
TBColorManager *g_color_manager;

bool tb_core_init(TBRenderer *renderer)
{
	//TBDebugPrint("Initiating Turbo Badger - version %s\n", TB_VERSION_STR);
	g_renderer = renderer;
	g_tb_lng = new TBLanguage;
	g_font_manager = new TBFontManager();
	g_color_manager = new TBColorManager();
	g_tb_skin = new TBSkin();
	g_widgets_reader = TBWidgetsReader::Create();
#ifdef TB_IMAGE
	g_image_manager = new TBImageManager();
#endif
	return renderer && g_tb_lng && g_font_manager && g_tb_skin
		&& g_widgets_reader
#ifdef TB_IMAGE
		&& g_image_manager
#endif
;
}

void tb_core_shutdown()
{
	TBAnimationManager::AbortAllAnimations();
#ifdef TB_IMAGE
	delete g_image_manager;
	g_image_manager = nullptr;
#endif
	delete g_widgets_reader;
	g_widgets_reader = nullptr;
	delete g_tb_skin;
	g_tb_skin = nullptr;
	delete g_color_manager;
	g_color_manager = nullptr;
	delete g_font_manager;
	g_font_manager = nullptr;
	delete g_tb_lng;
	g_tb_lng = nullptr;
}

bool tb_core_is_initialized()
{
	return g_widgets_reader ? true : false;
}

} // namespace tb
