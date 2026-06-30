SET(SOURCES components/ui_comp_wlan.c
    components/ui_comp.c
    screens/ui_Screen1.c
    screens/ui_Screen2.c
    screens/ui_ListScreen.c
    screens/ui_ConsoleScreen.c
    ui_theme_manager.c
    ui_themes.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_wifi64_png.c
    images/ui_img_bluetooth64_png.c
    images/ui_img_battery64_png.c
    images/ui_img_334164688.c
    fonts/ui_font_Font1.c)

add_library(ui ${SOURCES})
