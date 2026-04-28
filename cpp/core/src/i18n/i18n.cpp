#include <poebot/i18n/i18n.hpp>

#include <string>
#include <unordered_map>

namespace poebot::i18n {

namespace {

using Table = std::unordered_map<std::string_view, const char*>;

// English table — also acts as the fallback when a key is missing from
// another language.
const Table& enTable() {
    static const Table t = {
        // Top bar (the legacy Profile / Language / App menus collapsed:
        //   - Profile is now a segmented control rendered inline.
        //   - Language and Appearance live in the FAB popup.
        //   - The standalone App > Exit was redundant with the title-bar X.)
        {"menu.profile",                   "Profile"},
        {"menu.language",                  "Language"},
        {"menu.language.zh",               "中文"},
        {"menu.language.en",               "English"},
        {"menu.app.appearance",            "Appearance"},
        {"menu.app.appearance.light",      "Light"},
        {"menu.app.appearance.dark",       "Dark"},

        // Sidebar tabs. The internal id of the first tab is still "Config"
        // (Panel::name()), but the user-facing label is "Settings" — same
        // wording as PoE Overlay's settings page, where this panel grew its
        // hotkey-customization section.
        {"panel.config",                   "Settings"},
        {"panel.craft",                    "Craft"},
        {"panel.map",                      "Map"},
        {"panel.deposit",                  "Deposit"},
        {"panel.log",                      "Log"},

        // Right-aligned status
        {"status.poe.found",               "POE: FOUND"},
        {"status.poe.missing",             "POE: ---"},
        {"status.recording_fmt",           "Recording '%.*s' %.1fs — switch to game"},

        // Common
        {"common.no_active_profile",       "No active profile."},
        {"common.stop",                    "Stop"},
        {"common.stopping",                "Stopping..."},
        {"common.other_task_running_fmt",  "Another task is running: %s"},
        {"common.ok",                      "OK"},
        {"common.cancel",                  "Cancel"},

        // Affix library widget (shared by Craft and Map panels). Editing
        // moved out of the GUI — the widget only binds, reloads, and reveals
        // the folder. New / Rename / Delete go through the file system.
        {"affix_lib.label",                "Library"},
        {"affix_lib.none",                 "(none)"},
        {"affix_lib.edit",                 "Edit"},
        {"affix_lib.reload",               "Reload"},
        {"affix_lib.folder",               "Folder"},
        {"affix_lib.lines_fmt",            "%d rules"},
        {"affix_lib.poere_prefix",         "Supports"},

        // Settings panel — Save is gone (auto-save handles it); Reset stays
        // but now goes through a confirm modal. The hotkey rows live above
        // the coord rows.
        {"config.no_settings",             "No settings loaded."},
        {"config.no_active_profile_fmt",   "No active profile ('%s' not found)."},
        {"config.active_profile_fmt",      "Active profile: %s  [%s]"},
        {"config.section.orbs",            "Currency / orb slots"},
        {"config.section.anchors",         "Craft / map item grid anchors"},
        {"config.section.inventory",       "Inventory deposit anchors"},
        {"config.unset",                   "(unset)"},
        {"config.button.reset_profile",    "Reset profile to defaults"},
        {"config.confirm_reset",           "Reset this profile to defaults?\nAll coords, craft / map / deposit settings, stats, and capture hotkeys will be reset."},

        // Settings tabs (top of the panel — Hotkeys is global, Coords is
        // per-profile).
        {"settings.tab.hotkeys",                      "Hotkeys"},
        {"settings.tab.coords",                       "Coordinates"},

        // Hotkeys section (Settings panel, top). Action labels are short
        // imperatives so the modal's "Rebind: <label>" reads naturally.
        {"settings.section.hotkeys",                  "Hotkeys"},
        {"settings.hotkeys.reset_all",                "Reset all hotkeys to defaults"},
        {"settings.hotkeys.rebind_title_fmt",         "Rebind: %s"},
        {"settings.hotkeys.rebind_prompt",            "(press a key combination)"},
        {"settings.hotkeys.rebind_hint",              "Press a non-modifier key to commit  •  ESC to cancel"},
        {"settings.hotkeys.rebind_err_conflict_fmt",  "%s is already bound to: %s"},
        {"settings.hotkeys.rebind_err_unavailable_fmt","%s is unavailable — another app may own it"},

        {"hotkey.action.task.start.craft",   "Start craft"},
        {"hotkey.action.task.start.map",     "Start map roll"},
        {"hotkey.action.task.start.deposit", "Start deposit"},
        {"hotkey.action.task.stop",          "Stop task"},
        {"hotkey.action.overlay.toggle",     "Toggle overlay"},
        {"hotkey.action.capture.orb1",       "Capture orb1"},
        {"hotkey.action.capture.orb2",       "Capture orb2"},
        {"hotkey.action.capture.orb3",       "Capture orb3"},
        {"hotkey.action.capture.baseItem",   "Capture base item"},
        {"hotkey.action.capture.p01Item",    "Capture p01 item"},
        {"hotkey.action.capture.p10Item",    "Capture p10 item"},
        {"hotkey.action.capture.invBase",    "Capture inv base"},
        {"hotkey.action.capture.invP01",     "Capture inv p01"},
        {"hotkey.action.capture.invP10",     "Capture inv p10"},

        // Craft panel — affix textbox is gone (rules now live in the bound
        // .txt) and so are Start / Stop buttons (those are hotkey-only via
        // task.start.craft / task.stop now).
        {"craft.batch_mode",               "Batch mode"},
        {"craft.columns",                  "Columns"},
        {"craft.rows",                     "Rows"},
        {"craft.stats_fmt",                "Stats  ops=%d  hits=%d"},
        {"craft.reset_stats",              "Reset stats"},
        {"craft.progress_fmt",             "Crafting: item %d / %d  |  ops=%d  hits=%d"},

        // Map panel — same shape as Craft; Start / Stop are hotkeys.
        {"map.mode.alch_scour",            "Alch + Scour"},
        {"map.mode.chaos",                 "Chaos"},
        {"map.batch_mode",                 "Batch mode"},
        {"map.columns",                    "Columns"},
        {"map.rows",                       "Rows"},
        {"map.stats_fmt",                  "Stats  ops=%d  hits=%d"},
        {"map.reset_stats",                "Reset stats"},
        {"map.progress_fmt",               "Rolling: map %d / %d  |  ops=%d  hits=%d"},

        // Deposit panel — Start / Stop are hotkeys (task.start.deposit /
        // task.stop); the panel just shows config + progress.
        {"deposit.inv_columns",            "Inventory columns"},
        {"deposit.inv_rows",               "Inventory rows"},
        {"deposit.description_fmt",        "Scans the inventory grid (%d x %d) and Ctrl+left-clicks each occupied cell into the open stash tab."},
        {"deposit.progress_fmt",           "Depositing: %d / %d cells"},
        {"deposit.finished",               "Deposit finished."},

        // Hotkey hint shown at the bottom of every task panel. Two %s
        // slots: start binding label, stop binding label. Read live so
        // it tracks Settings rebinds in real time.
        {"task.hotkey_hint_fmt",           "%s to start   ·   %s to stop"},

        // Log panel
        {"log.title",                      "Log"},
        {"log.auto_scroll",                "Auto scroll"},
        {"log.clear",                      "Clear"},
        {"log.entries_fmt",                "(%zu entries)"},
    };
    return t;
}

const Table& zhTable() {
    static const Table t = {
        // 顶栏（原 Profile / Language / App 三菜单已合并）
        {"menu.profile",                   "配置档"},
        {"menu.language",                  "语言"},
        {"menu.language.zh",               "中文"},
        {"menu.language.en",               "English"},
        {"menu.app.appearance",            "外观"},
        {"menu.app.appearance.light",      "浅色"},
        {"menu.app.appearance.dark",       "深色"},

        // 侧栏（与 PoE Overlay 一致，将原"配置"改为"设置"，承载热键自定义）
        {"panel.config",                   "设置"},
        {"panel.craft",                    "改造"},
        {"panel.map",                      "地图"},
        {"panel.deposit",                  "入库"},
        {"panel.log",                      "日志"},

        // 状态
        {"status.poe.found",               "POE: 已连接"},
        {"status.poe.missing",             "POE: 未连接"},
        {"status.recording_fmt",           "记录中 '%.*s' 剩 %.1f 秒 — 请切到游戏窗口"},

        // 通用
        {"common.no_active_profile",       "没有活动配置档。"},
        {"common.stop",                    "停止"},
        {"common.stopping",                "正在停止…"},
        {"common.other_task_running_fmt",  "另一任务运行中：%s"},
        {"common.ok",                      "确定"},
        {"common.cancel",                  "取消"},

        // 词条库组件（改造与地图面板共用）— 编辑下放到外部编辑器，
        // 控件只负责绑定 / 重载 / 打开文件夹。
        {"affix_lib.label",                "词条库"},
        {"affix_lib.none",                 "（未选）"},
        {"affix_lib.edit",                 "编辑"},
        {"affix_lib.reload",               "重载"},
        {"affix_lib.folder",               "打开目录"},
        {"affix_lib.lines_fmt",            "%d 条规则"},
        {"affix_lib.poere_prefix",         "支持"},

        // 设置面板（Save 已去掉 — 自动保存；Reset 走二次确认）
        {"config.no_settings",             "未加载配置。"},
        {"config.no_active_profile_fmt",   "没有活动配置档（找不到 '%s'）。"},
        {"config.active_profile_fmt",      "当前配置档：%s  [%s]"},
        {"config.section.orbs",            "通货 / 球类坐标"},
        {"config.section.anchors",         "改造 / 地图物品网格锚点"},
        {"config.section.inventory",       "背包入库锚点"},
        {"config.unset",                   "(未设)"},
        {"config.button.reset_profile",    "重置为默认值"},
        {"config.confirm_reset",           "确认将该配置档重置为默认值？\n所有坐标、改造 / 地图 / 入库设置、统计以及捕获热键都会回到默认。"},

        // 设置面板顶部分页
        {"settings.tab.hotkeys",                      "热键"},
        {"settings.tab.coords",                       "坐标"},

        // 热键自定义（设置面板顶部）
        {"settings.section.hotkeys",                  "热键"},
        {"settings.hotkeys.reset_all",                "全部恢复为默认热键"},
        {"settings.hotkeys.rebind_title_fmt",         "重新绑定：%s"},
        {"settings.hotkeys.rebind_prompt",            "（请按下组合键）"},
        {"settings.hotkeys.rebind_hint",              "按下非修饰键确认  •  按 ESC 取消"},
        {"settings.hotkeys.rebind_err_conflict_fmt",  "%s 已绑定到：%s"},
        {"settings.hotkeys.rebind_err_unavailable_fmt","%s 当前不可用 — 可能被别的程序占用"},

        {"hotkey.action.task.start.craft",   "开始改造"},
        {"hotkey.action.task.start.map",     "开始洗图"},
        {"hotkey.action.task.start.deposit", "开始入库"},
        {"hotkey.action.task.stop",          "停止任务"},
        {"hotkey.action.overlay.toggle",     "显示/隐藏叠加层"},
        {"hotkey.action.capture.orb1",       "捕获 orb1"},
        {"hotkey.action.capture.orb2",       "捕获 orb2"},
        {"hotkey.action.capture.orb3",       "捕获 orb3"},
        {"hotkey.action.capture.baseItem",   "捕获基础物品锚点"},
        {"hotkey.action.capture.p01Item",    "捕获 p01 物品锚点"},
        {"hotkey.action.capture.p10Item",    "捕获 p10 物品锚点"},
        {"hotkey.action.capture.invBase",    "捕获背包基础锚点"},
        {"hotkey.action.capture.invP01",     "捕获背包 p01 锚点"},
        {"hotkey.action.capture.invP10",     "捕获背包 p10 锚点"},

        // 改造面板 — 词缀文本框已去掉；开始 / 停止改为热键
        // （task.start.craft / task.stop）。
        {"craft.batch_mode",               "批量模式"},
        {"craft.columns",                  "列数"},
        {"craft.rows",                     "行数"},
        {"craft.stats_fmt",                "统计  操作=%d  命中=%d"},
        {"craft.reset_stats",              "清空统计"},
        {"craft.progress_fmt",             "改造中:  第 %d / %d 件  |  操作=%d  命中=%d"},

        // 地图面板 — 同上。
        {"map.mode.alch_scour",            "点金+重铸"},
        {"map.mode.chaos",                 "混沌石"},
        {"map.batch_mode",                 "批量模式"},
        {"map.columns",                    "列数"},
        {"map.rows",                       "行数"},
        {"map.stats_fmt",                  "统计  操作=%d  命中=%d"},
        {"map.reset_stats",                "清空统计"},
        {"map.progress_fmt",               "洗图中:  第 %d / %d 张  |  操作=%d  命中=%d"},

        // 入库面板 — 开始 / 停止改为热键。
        {"deposit.inv_columns",            "背包列数"},
        {"deposit.inv_rows",               "背包行数"},
        {"deposit.description_fmt",        "扫描背包（%d x %d 格），对每个占用的格子按 Ctrl+鼠标左键，转入已打开的仓库页。"},
        {"deposit.progress_fmt",           "入库中:  %d / %d 格"},
        {"deposit.finished",               "入库完成。"},

        // 任务面板底部的热键提示。两个 %s：开始 / 停止当前绑定。
        {"task.hotkey_hint_fmt",           "%s 开始   ·   %s 停止"},

        // 日志面板
        {"log.title",                      "日志"},
        {"log.auto_scroll",                "自动滚动"},
        {"log.clear",                      "清空"},
        {"log.entries_fmt",                "(%zu 条)"},
    };
    return t;
}

// The currently selected table. Pointer so setLanguage is a cheap swap.
const Table* g_active = &enTable();
std::string  g_lang   = "en";

}  // namespace

void setLanguage(std::string_view lang) {
    if (lang == "zh") {
        g_active = &zhTable();
        g_lang   = "zh";
    } else {
        g_active = &enTable();
        g_lang   = "en";  // normalize unknown input to English
    }
}

std::string_view language() { return g_lang; }

const char* tr(const char* key) {
    if (!key) return "";
    const std::string_view k{key};
    if (g_active) {
        if (auto it = g_active->find(k); it != g_active->end()) {
            return it->second;
        }
    }
    // Fall back to English, then to the key itself.
    const Table& en = enTable();
    if (auto it = en.find(k); it != en.end()) return it->second;
    return key;
}

}  // namespace poebot::i18n
