#include "engine/UiStrings.h"

#include <unordered_map>

namespace layrix {
namespace {

UiLanguage g_language = UiLanguage::Ru;

struct Pair {
    const wchar_t* ru;
    const wchar_t* en;
};

const std::unordered_map<std::wstring_view, Pair> kStrings = {
    {L"lang_ru", {L"Русский", L"Русский"}},
    {L"lang_en", {L"English", L"English"}},
    {L"lang_tooltip", {L"Язык", L"Language"}},
    {L"settings_tooltip", {L"Настройки", L"Settings"}},
    {L"templates", {L"Шаблоны", L"Templates"}},
    {L"custom_layouts", {L"Свои макеты", L"Custom layouts"}},
    {L"custom_empty_title", {L"Ваше рабочее пространство", L"Your workspace"}},
    {L"custom_empty_hint", {L"Создайте макет и расположите зоны так, как удобно вам.", L"Create a layout and place zones the way you like."}},
    {L"snap_settings", {L"Настройки привязки", L"Snap settings"}},
    {L"spacing", {L"Отступ", L"Spacing"}},
    {L"spacing_between", {L"Отступ между зонами", L"Space between zones"}},
    {L"win_arrows_label", {L"Перехват Win + стрелки", L"Intercept Win + arrows"}},
    {L"back_to_layouts", {L"←  К макетам зон", L"←  Back to layouts"}},
    {L"settings_title", {L"Настройки приложения", L"App settings"}},
    {L"settings_intro", {L"Изменения применяются сразу.", L"Changes apply immediately."}},
    {L"startup", {L"Запускать вместе с Windows", L"Start with Windows"}},
    {L"on", {L"Включён", L"On"}},
    {L"off", {L"Выключен", L"Off"}},
    {L"enabled_f", {L"Включена", L"On"}},
    {L"disabled_f", {L"Выключена", L"Off"}},
    {L"enabled_n", {L"Включено", L"On"}},
    {L"disabled_n", {L"Выключено", L"Off"}},
    {L"startup_help", {L"Layrix будет запускаться при входе в вашу учётную запись и работать в трее. Окно редактора открываться не будет.", L"Layrix starts when you sign in and stays in the tray. The editor will not open."}},
    {L"startup_ok", {L"Автозапуск настроен через папку «Автозагрузка» текущего пользователя.", L"Startup uses the current user's Startup folder."}},
    {L"startup_off", {L"Автозапуск выключен.", L"Startup is off."}},
    {L"startup_legacy", {L"Старая запись автозапуска: выключите и снова включите переключатель для восстановления.", L"Legacy startup entry: turn the switch off and on again to repair it."}},
    {L"startup_read_fail", {L"Не удалось прочитать настройку автозапуска. Код ошибки: ", L"Could not read the startup setting. Error code: "}},
    {L"startup_write_fail", {L"Не удалось изменить автозапуск. Код ошибки: ", L"Could not change startup. Error code: "}},
    {L"zones", {L"Привязка окон к зонам", L"Snap windows to zones"}},
    {L"snap_key", {L"Клавиша привязки", L"Snap key"}},
    {L"reset", {L"Сбросить", L"Reset"}},
    {L"snap_hold_hint", {L"Нажмите кнопку с комбинацией, затем клавишу или сочетание. Esc отменяет запись.", L"Click the key button, then press a key or combination. Esc cancels."}},
    {L"snap_hold_capture", {L"Нажмите клавишу…", L"Press a key…"}},
    {L"snap_hold_capture_hint", {L"Удерживайте модификаторы и нажмите клавишу. Esc — отмена.", L"Hold modifiers and press a key. Esc cancels."}},
    {L"win_arrows", {L"Перемещать окна по зонам клавишами Win + ← / →", L"Move windows between zones with Win + ← / →"}},
    {L"win_arrows_hint", {L"Стрелки переключают активное окно между зонами выбранного монитора.", L"The arrows move the active window between zones on the selected monitor."}},
    {L"zone_display", {L"Отображение зон", L"Zone display"}},
    {L"zone_display_hidden", {L"Невидимые области", L"Hidden zones"}},
    {L"zone_display_top", {L"Зоны поверх", L"Zones on top"}},
    {L"zone_display_desktop", {L"Зоны на рабочем столе", L"Zones on desktop"}},
    {L"zone_display_hidden_hint", {L"При перетаскивании окна зоны не рисуются. Привязка по клавише всё равно работает.", L"Zones are not drawn while dragging. Snapping still works."}},
    {L"zone_display_top_hint", {L"Во время перетаскивания контуры зон рисуются поверх других окон и не перехватывают мышь.", L"While dragging, zone outlines appear above other windows and do not capture the mouse."}},
    {L"zone_display_desktop_hint", {L"Во время перетаскивания зоны видны на рабочем столе и остаются под окнами приложений.", L"While dragging, zones stay on the desktop under other windows."}},
    {L"zone_color", {L"Цвет зон", L"Zone color"}},
    {L"system_color", {L"Как в Windows", L"Match Windows"}},
    {L"system_on", {L"Системный", L"System"}},
    {L"system_off", {L"Свой", L"Custom"}},
    {L"zone_color_help", {L"Подсветка зон при перетаскивании. По умолчанию берётся цвет акцента Windows.", L"Zone highlight while dragging. Defaults to the Windows accent color."}},
    {L"accent_color", {L"Цвет акцентов приложения", L"App accent color"}},
    {L"accent_color_help", {L"Выделение макетов, переключатели и элементы редактора. По умолчанию совпадает с акцентом Windows. Нажмите образец или «Палитра», чтобы выбрать цвет кругом, RGB, HSV или HEX.", L"Layout selection, toggles, and editor controls. Defaults to the Windows accent. Click the swatch or Palette for wheel, RGB, HSV, or HEX."}},
    {L"create_layout", {L"+  Создать макет", L"+  Create layout"}},
    {L"palette", {L"Палитра", L"Palette"}},
    {L"monitor", {L"Монитор", L"Monitor"}},
    {L"no_monitors", {L"Мониторы не найдены", L"No monitors found"}},
    {L"title_editor", {L"Layrix — Редактор зон", L"Layrix — Zone editor"}},
    {L"title_settings", {L"Layrix — Настройки", L"Layrix — Settings"}},
    {L"tray_editor", {L"Редактор зон", L"Zone editor"}},
    {L"tray_settings", {L"Настройки", L"Settings"}},
    {L"tray_zones", {L"Зоны включены", L"Zones enabled"}},
    {L"tray_exit", {L"Выход", L"Exit"}},
    {L"editor_open_fail", {L"Не удалось открыть редактор зон.", L"Could not open the zone editor."}},
    {L"desktop_edit_fail", {L"Не удалось открыть редактирование зон на рабочем столе.", L"Could not open desktop zone editing."}},
    {L"custom_layout", {L"Свой макет", L"Custom layout"}},
    {L"layout_none", {L"Без макета", L"No layout"}},
    {L"layout_focus", {L"Фокус", L"Focus"}},
    {L"layout_columns", {L"Столбцы", L"Columns"}},
    {L"layout_rows", {L"Строки", L"Rows"}},
    {L"layout_grid", {L"Сетка", L"Grid"}},
    {L"layout_priority", {L"Приоритетная сетка", L"Priority grid"}},
    {L"edit_zones", {L"Редактировать зоны на экране", L"Edit zones on the screen"}},
    {L"edit_layout", {L"Редактировать", L"Edit"}},
    {L"rename_layout", {L"Переименовать", L"Rename"}},
    {L"edit_title", {L"Редактирование зон", L"Editing zones"}},
    {L"layout_name", {L"Название макета", L"Layout name"}},
    {L"layout_name_ph", {L"Свой макет", L"Custom layout"}},
    {L"edit_hint", {L"Тяните границы и зоны мышью. На пустом месте можно нарисовать новую зону.", L"Drag zone edges with the mouse. Draw on empty space to add a zone."}},
    {L"add_zone", {L"+  Зона", L"+  Zone"}},
    {L"split_h", {L"Разделить ↔", L"Split ↔"}},
    {L"split_v", {L"Разделить ↕", L"Split ↕"}},
    {L"delete_zone", {L"Удалить зону", L"Delete zone"}},
    {L"delete_layout", {L"Удалить макет", L"Delete layout"}},
    {L"cancel_esc", {L"Отмена · Esc", L"Cancel · Esc"}},
    {L"cancel", {L"Отмена", L"Cancel"}},
    {L"save", {L"Сохранить", L"Save"}},
    {L"save_tip", {L"Сохранить · Ctrl + S", L"Save · Ctrl + S"}},
    {L"zones_count", {L"Зон: ", L"Zones: "}},
    {L"canvas_hint", {L"Протяните мышь, чтобы создать зону. Перетащите зону или её край. Delete удаляет зону.", L"Drag to create a zone. Move a zone or its edge. Delete removes it."}},
    {L"name", {L"Название", L"Name"}},
    {L"snap_hint", {L"Удерживайте {hold} при перетаскивании окна. {hold} + {extra} позволяет занять несколько зон.", L"Hold {hold} while dragging a window. {hold} + {extra} selects several zones."}},
    {L"snap_footer", {L"Удерживайте {hold} при перетаскивании окна в зону", L"Hold {hold} while dragging a window into a zone"}},
};

std::wstring ReplaceToken(std::wstring text, std::wstring_view token, std::wstring_view value) {
    const std::wstring needle{token};
    for (size_t at = 0; (at = text.find(needle, at)) != std::wstring::npos; ) {
        text.replace(at, needle.size(), value);
        at += value.size();
    }
    return text;
}

}  // namespace

UiLanguage ParseUiLanguage(std::wstring_view text) {
    if (text == L"en" || text == L"english" || text == L"EN") {
        return UiLanguage::En;
    }
    return UiLanguage::Ru;
}

std::wstring_view UiLanguageCode(UiLanguage language) {
    return language == UiLanguage::En ? L"en" : L"ru";
}

UiLanguage CurrentUiLanguage() {
    return g_language;
}

void SetCurrentUiLanguage(UiLanguage language) {
    g_language = language;
}

std::wstring Tr(std::wstring_view key) {
    const auto it = kStrings.find(key);
    if (it == kStrings.end()) {
        return std::wstring{key};
    }
    return g_language == UiLanguage::En ? it->second.en : it->second.ru;
}

std::wstring ReplaceText(std::wstring text, std::wstring_view token, std::wstring_view value) {
    return ReplaceToken(std::move(text), token, value);
}

std::wstring TrReplace(std::wstring_view key, std::wstring_view token, std::wstring_view value) {
    return ReplaceToken(Tr(key), token, value);
}

std::wstring BuiltinLayoutTitle(std::wstring_view id) {
    if (id == L"none") return Tr(L"layout_none");
    if (id == L"focus") return Tr(L"layout_focus");
    if (id == L"columns") return Tr(L"layout_columns");
    if (id == L"rows") return Tr(L"layout_rows");
    if (id == L"grid") return Tr(L"layout_grid");
    if (id == L"priority") return Tr(L"layout_priority");
    return std::wstring{id};
}

}  // namespace layrix
