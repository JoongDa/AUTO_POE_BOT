; ================================================================
; POE BOT - Multi-function Tool (AHK v2)
; Alt+1  → 通货1  |  Alt+2 → 通货2  |  Alt+3 → 通货3
; Alt+0  → 基准物品 (0,0)
; Alt+8  → (x+1)  |  Alt+9 → (y+1)
; Home   → Start  |  End → Stop
; ================================================================
#Requires AutoHotkey v2.0
#SingleInstance Force
SendMode "Input"
SetDefaultMouseSpeed 0
CoordMode "Mouse", "Client"
CoordMode "Pixel", "Client"

; ==================== 版本 ====================
APP_VERSION := "0.3.0"
CONFIG_FILE := A_ScriptDir "\Bot.ini"

; ==================== 全局状态 ====================
; 公共
running := false
poeHwnd := 0
lang := "zh"
activeFunc := "config"
opCount := 0
hitCount := 0     ; 命中次数
moveSpeed := 18   ; 鼠标速度 15=标准 18=快

; 配置 - 统一坐标（通货+物品，所有功能共用）
orb1X := 0, orb1Y := 0   ; 通货1
orb2X := 0, orb2Y := 0   ; 通货2
orb3X := 0, orb3Y := 0   ; 通货3
baseX := 0, baseY := 0   ; 基准物品 (0,0)
p01X := 0, p01Y := 0     ; (x+1) 偏移参考
p10X := 0, p10Y := 0     ; (y+1) 偏移参考
offsetX := 0, offsetY := 0

; 改造
craftCols := 10, craftRows := 2

; 洗地图
mapMode := 1  ; 1=点金+重铸  2=混沌石
mapCols := 4, mapRows := 3

; 存包 - 背包格子坐标
invBaseX := 0, invBaseY := 0   ; 背包(0,0)
invP01X := 0, invP01Y := 0     ; 背包(x+1)
invP10X := 0, invP10Y := 0     ; 背包(y+1)
invOffsetX := 0, invOffsetY := 0
depositCols := 12, depositRows := 5

; ==================== 语言 / Language ====================
L := Map()

L["zh"] := Map(
    "title",            "POE BOT",
    "lang_btn",         "EN",
    ; 左侧
    "btn_config",       "配置",
    "btn_craft",        "改造",
    "btn_map",          "洗地图",
    ; 公共
    "grp_window",       "窗口",
    "window_none",      "未捕获 (记录坐标时自动捕获)",
    "grp_status",       "状态",
    "status_ready",     "就绪",
    "status_stopped",   "已停止",
    "op_count",         "操作: {1}  命中: {2}  平均: {3}",
    "grp_log",          "日志",
    "pos_none",         "未设置",
    "lbl_speed",        "速度:",
    "speed_normal",     "标准",
    "speed_fast",       "快速",
    "btn_record",       "记录",
    "btn_start",        "开始",
    "btn_stop",         "停止",
    "chk_batch",        "批量",
    "lbl_grid",         "网格:",
    "lbl_rows",         "行",
    "lbl_cols",         "列",
    "lbl_affix",        "目标词缀:",
    "tip_wait",         "3 秒后记录鼠标位置（请切到游戏窗口）...",
    "log_capture",      "捕获窗口: {1}",
    "log_window_lost",  "游戏窗口丢失，已停止",
    "log_window_gone",  "游戏窗口不存在，请重新记录坐标",
    "msg_no_window",    "游戏窗口不存在，请重新记录坐标！",
    "log_manual_stop",  "手动停止，共 {1} 次",
    ; 配置
    "lbl_orb1",         "通货1:",
    "lbl_orb2",         "通货2:",
    "lbl_orb3",         "通货3:",
    "lbl_base",         "物品(0,0):",
    "lbl_p01",          "物品(x+1):",
    "lbl_p10",          "物品(y+1):",
    "log_orb_pos",      "通货{1}: {2}, {3}",
    "log_base_pos",     "基准(0,0): {1}, {2}",
    "log_p01_pos",      "(x+1): {1}, {2}",
    "log_p10_pos",      "(y+1): {1}, {2}",
    "log_offset",       "偏移量: X={1}, Y={2}",
    "msg_set_pos",      "请先在配置中记录通货和物品坐标！",
    ; 改造
    "status_crafting",  "改造中...",
    "status_found",     "目标词缀达成！",
    "log_start",        "开始改造 - 目标: {1}",
    "log_copy_fail",    "#{1} 复制失败，重试",
    "log_applied",      "#{1} 已应用 (剩余{2})",
    "log_bad_copy",     "#{1} 复制异常，重试",
    "log_orb_empty",    "通货耗尽，停止",
    "log_affixes",      "  词条: {1}",
    "log_miss",         "  未命中 [{1}]",
    "log_found",        "命中！第 {1} 次 ★{2}★",
    "msg_found",        "目标词缀达成！共改造 {1} 次。`n`n{2}",
    "status_bc",        "改造中 [{1},{2}] 通货{3}",
    "log_bc_start",     "批量改造 [{1}x{2}] 目标: {3}",
    "log_bc_switch",    "通货{1}耗尽 → 通货{2}",
    "log_bc_depleted",  "所有通货耗尽，停止",
    "log_bc_hit",       "[{1},{2}] 命中 ★{3}★",
    "log_bc_next",      "→ 下一个 [{1},{2}]",
    "log_bc_done",      "完成！操作{1} 命中{2}",
    "msg_bc_done",      "改造完成！操作 {1} 次，命中 {2} 个。",
    ; 洗地图
    "lbl_map_mode",     "模式:",
    "map_mode_alch",    "点金+重铸",
    "map_mode_chaos",   "混沌石",
    "status_rolling",   "洗图中... [{1},{2}]",
    "status_map_done",  "洗图完成！",
    "log_map_start",    "开始洗图 [{1}x{2}] 模式: {3} 目标: {4}",
    "log_map_rolling",  "[{1},{2}] #{3} 洗...",
    "log_map_hit",      "[{1},{2}] 命中 ★{3}★",
    "log_map_done",     "洗图完成！共 {1} 次操作, 命中 {2}",
    "msg_map_done",     "洗图完成！操作 {1} 次, 命中 {2} 个。",
    ; 存包
    "btn_deposit",      "存包",
    "lbl_inv_base",     "背包(0,0):",
    "lbl_inv_p01",      "背包(x+1):",
    "lbl_inv_p10",      "背包(y+1):",
    "log_inv_base_pos", "背包(0,0): {1}, {2}",
    "log_inv_p01_pos",  "背包(x+1): {1}, {2}",
    "log_inv_p10_pos",  "背包(y+1): {1}, {2}",
    "log_inv_offset",   "背包偏移: X={1}, Y={2}",
    "status_depositing","存包中... [{1},{2}]",
    "status_deposit_done", "存包完成！",
    "log_deposit_start","开始存包 [{1}x{2}]",
    "log_deposit_done", "存包完成！共 {1} 格",
    "msg_deposit_done", "存包完成！共操作 {1} 格",
    "msg_no_inv",       "请先记录背包坐标！"
)

L["en"] := Map(
    "title",            "POE BOT",
    "lang_btn",         "中文",
    ; Left
    "btn_config",       "Config",
    "btn_craft",        "Craft",
    "btn_map",          "Map Roll",
    ; Common
    "grp_window",       "Window",
    "window_none",      "Not captured (auto on record)",
    "grp_status",       "Status",
    "status_ready",     "Ready",
    "status_stopped",   "Stopped",
    "op_count",         "Ops: {1}  Hits: {2}  Avg: {3}",
    "grp_log",          "Log",
    "pos_none",         "Not set",
    "lbl_speed",        "Speed:",
    "speed_normal",     "Normal",
    "speed_fast",       "Fast",
    "btn_record",       "Record",
    "btn_start",        "Start",
    "btn_stop",         "Stop",
    "chk_batch",        "Batch",
    "lbl_grid",         "Grid:",
    "lbl_rows",         "R",
    "lbl_cols",         "C",
    "lbl_affix",        "Affixes:",
    "tip_wait",         "Recording in 3s (switch to game)...",
    "log_capture",      "Captured: {1}",
    "log_window_lost",  "Window lost, stopped",
    "log_window_gone",  "Window not found, re-record",
    "msg_no_window",    "Game window not found!",
    "log_manual_stop",  "Manual stop, total: {1}",
    ; Config
    "lbl_orb1",         "Curr 1:",
    "lbl_orb2",         "Curr 2:",
    "lbl_orb3",         "Curr 3:",
    "lbl_base",         "Item(0,0):",
    "lbl_p01",          "Item(x+1):",
    "lbl_p10",          "Item(y+1):",
    "log_orb_pos",      "Currency{1}: {2}, {3}",
    "log_base_pos",     "Base(0,0): {1}, {2}",
    "log_p01_pos",      "(x+1): {1}, {2}",
    "log_p10_pos",      "(y+1): {1}, {2}",
    "log_offset",       "Offset: X={1}, Y={2}",
    "msg_set_pos",      "Record positions in Config first!",
    ; Craft
    "status_crafting",  "Crafting...",
    "status_found",     "Target found!",
    "log_start",        "Start craft - Target: {1}",
    "log_copy_fail",    "#{1} Copy failed, retry",
    "log_applied",      "#{1} Applied (rem:{2})",
    "log_bad_copy",     "#{1} Bad copy, retry",
    "log_orb_empty",    "Currency depleted, stop",
    "log_affixes",      "  Affixes: {1}",
    "log_miss",         "  No match [{1}]",
    "log_found",        "Hit! #{1} ★{2}★",
    "msg_found",        "Target found! Crafted {1} times.`n`n{2}",
    "status_bc",        "Crafting [{1},{2}] Curr{3}",
    "log_bc_start",     "Batch craft [{1}x{2}] Target: {3}",
    "log_bc_switch",    "Curr{1} depleted → Curr{2}",
    "log_bc_depleted",  "All currencies depleted",
    "log_bc_hit",       "[{1},{2}] Hit ★{3}★",
    "log_bc_next",      "→ next [{1},{2}]",
    "log_bc_done",      "Done! Ops:{1} Hits:{2}",
    "msg_bc_done",      "Craft done! {1} ops, {2} hits.",
    ; Map Roll
    "lbl_map_mode",     "Mode:",
    "map_mode_alch",    "Alch+Scour",
    "map_mode_chaos",   "Chaos",
    "status_rolling",   "Rolling... [{1},{2}]",
    "status_map_done",  "Map rolling done!",
    "log_map_start",    "Start rolling [{1}x{2}] mode: {3} target: {4}",
    "log_map_rolling",  "[{1},{2}] roll #{3}...",
    "log_map_hit",      "[{1},{2}] Hit ★{3}★",
    "log_map_done",     "Rolling done! Ops: {1} Hits: {2}",
    "msg_map_done",     "Rolling done! {1} ops, {2} hits.",
    ; Deposit
    "btn_deposit",      "Deposit",
    "lbl_inv_base",     "Inv(0,0):",
    "lbl_inv_p01",      "Inv(x+1):",
    "lbl_inv_p10",      "Inv(y+1):",
    "log_inv_base_pos", "Inv(0,0): {1}, {2}",
    "log_inv_p01_pos",  "Inv(x+1): {1}, {2}",
    "log_inv_p10_pos",  "Inv(y+1): {1}, {2}",
    "log_inv_offset",   "Inv offset: X={1}, Y={2}",
    "status_depositing","Depositing... [{1},{2}]",
    "status_deposit_done", "Deposit done!",
    "log_deposit_start","Start deposit [{1}x{2}]",
    "log_deposit_done", "Deposit done! {1} cells",
    "msg_deposit_done", "Deposit done! {1} cells",
    "msg_no_inv",       "Record inventory positions first!"
)

T(key, p1 := "", p2 := "", p3 := "", p4 := "") {
    global lang, L
    text := L[lang][key]
    if (p1 != "")
        text := StrReplace(text, "{1}", String(p1))
    if (p2 != "")
        text := StrReplace(text, "{2}", String(p2))
    if (p3 != "")
        text := StrReplace(text, "{3}", String(p3))
    if (p4 != "")
        text := StrReplace(text, "{4}", String(p4))
    return text
}

; ==================== GUI ====================
; +E0x08000000 = WS_EX_NOACTIVATE 点击窗口时不抢焦点，避免游戏从独占全屏退出
myGui := Gui("+AlwaysOnTop +E0x08000000", T("title") " v" APP_VERSION)
myGui.SetFont("s9")

; ========== 左侧：功能按钮列 ==========
leftX := 5
btnLang := myGui.Add("Button", "x" leftX " y5 w75 h25", T("lang_btn"))
btnLang.OnEvent("Click", OnToggleLang)

btnFuncConfig := myGui.Add("Button", "x" leftX " y35 w75 h30", T("btn_config"))
btnFuncConfig.OnEvent("Click", (*) => SwitchFunc("config"))

btnFuncCraft := myGui.Add("Button", "x" leftX " y70 w75 h30", T("btn_craft"))
btnFuncCraft.OnEvent("Click", (*) => SwitchFunc("craft"))

btnFuncMap := myGui.Add("Button", "x" leftX " y105 w75 h30", T("btn_map"))
btnFuncMap.OnEvent("Click", (*) => SwitchFunc("map"))

btnFuncDeposit := myGui.Add("Button", "x" leftX " y140 w75 h30", T("btn_deposit"))
btnFuncDeposit.OnEvent("Click", (*) => SwitchFunc("deposit"))

; ========== 中间：操作区 ==========
midX := 85
midW := 320
panelY := 55

; --- 游戏窗口 ---
grpWindow := myGui.Add("GroupBox", "x" midX " y5 w" midW " h42", T("grp_window"))
edtTitle := myGui.Add("Edit", "xp+10 yp+17 w" midW - 20 " h20 ReadOnly", T("window_none"))

; 固定列位置: 标签 colL, 输入 colE, 按钮 colB
colL := midX + 10
colE := midX + 75
colB := midX + 235

; ===== 配置面板 =====
cfgY := panelY

lblOrb1 := myGui.Add("Text", "x" colL " y" cfgY " w60 Right", T("lbl_orb1"))
edtOrb1Pos := myGui.Add("Edit", "x" colE " y" cfgY - 2 " w110 h20 ReadOnly", T("pos_none"))
txtOrb1Qty := myGui.Add("Text", "x" colE + 113 " y" cfgY " w40", "")
btnSetOrb1 := myGui.Add("Button", "x" colB " y" cfgY - 2 " w55 h20", "Alt+1")
btnSetOrb1.OnEvent("Click", OnSetOrb1)
cfgY += 22

lblOrb2 := myGui.Add("Text", "x" colL " y" cfgY " w60 Right", T("lbl_orb2"))
edtOrb2Pos := myGui.Add("Edit", "x" colE " y" cfgY - 2 " w110 h20 ReadOnly", T("pos_none"))
txtOrb2Qty := myGui.Add("Text", "x" colE + 113 " y" cfgY " w40", "")
btnSetOrb2 := myGui.Add("Button", "x" colB " y" cfgY - 2 " w55 h20", "Alt+2")
btnSetOrb2.OnEvent("Click", OnSetOrb2)
cfgY += 22

lblOrb3 := myGui.Add("Text", "x" colL " y" cfgY " w60 Right", T("lbl_orb3"))
edtOrb3Pos := myGui.Add("Edit", "x" colE " y" cfgY - 2 " w110 h20 ReadOnly", T("pos_none"))
txtOrb3Qty := myGui.Add("Text", "x" colE + 113 " y" cfgY " w40", "")
btnSetOrb3 := myGui.Add("Button", "x" colB " y" cfgY - 2 " w55 h20", "Alt+3")
btnSetOrb3.OnEvent("Click", OnSetOrb3)
cfgY += 26

lblBase := myGui.Add("Text", "x" colL " y" cfgY " w60 Right", T("lbl_base"))
edtBasePos := myGui.Add("Edit", "x" colE " y" cfgY - 2 " w150 h20 ReadOnly", T("pos_none"))
btnSetBase := myGui.Add("Button", "x" colB " y" cfgY - 2 " w55 h20", "Alt+0")
btnSetBase.OnEvent("Click", OnSetBase)
cfgY += 22

lblP01 := myGui.Add("Text", "x" colL " y" cfgY " w60 Right", T("lbl_p01"))
edtP01Pos := myGui.Add("Edit", "x" colE " y" cfgY - 2 " w150 h20 ReadOnly", T("pos_none"))
btnSetP01 := myGui.Add("Button", "x" colB " y" cfgY - 2 " w55 h20", "Alt+8")
btnSetP01.OnEvent("Click", OnSetP01)
cfgY += 22

lblP10 := myGui.Add("Text", "x" colL " y" cfgY " w60 Right", T("lbl_p10"))
edtP10Pos := myGui.Add("Edit", "x" colE " y" cfgY - 2 " w150 h20 ReadOnly", T("pos_none"))
btnSetP10 := myGui.Add("Button", "x" colB " y" cfgY - 2 " w55 h20", "Alt+9")
btnSetP10.OnEvent("Click", OnSetP10)
cfgY += 26

lblInvBase := myGui.Add("Text", "x" colL " y" cfgY " w60 Right", T("lbl_inv_base"))
edtInvBasePos := myGui.Add("Edit", "x" colE " y" cfgY - 2 " w150 h20 ReadOnly", T("pos_none"))
btnSetInvBase := myGui.Add("Button", "x" colB " y" cfgY - 2 " w55 h20", "Alt+4")
btnSetInvBase.OnEvent("Click", OnSetInvBase)
cfgY += 22

lblInvP01 := myGui.Add("Text", "x" colL " y" cfgY " w60 Right", T("lbl_inv_p01"))
edtInvP01Pos := myGui.Add("Edit", "x" colE " y" cfgY - 2 " w150 h20 ReadOnly", T("pos_none"))
btnSetInvP01 := myGui.Add("Button", "x" colB " y" cfgY - 2 " w55 h20", "Alt+5")
btnSetInvP01.OnEvent("Click", OnSetInvP01)
cfgY += 22

lblInvP10 := myGui.Add("Text", "x" colL " y" cfgY " w60 Right", T("lbl_inv_p10"))
edtInvP10Pos := myGui.Add("Edit", "x" colE " y" cfgY - 2 " w150 h20 ReadOnly", T("pos_none"))
btnSetInvP10 := myGui.Add("Button", "x" colB " y" cfgY - 2 " w55 h20", "Alt+6")
btnSetInvP10.OnEvent("Click", OnSetInvP10)

; ===== 改造面板 (初始隐藏) =====
craftY := panelY

chkCraftBatch := myGui.Add("Checkbox", "x" colL " y" craftY " Hidden", T("chk_batch"))
chkCraftBatch.OnEvent("Click", OnCraftBatchToggle)
edtCraftCols := myGui.Add("Edit", "x" colL + 55 " y" craftY - 2 " w35 h20 Number Hidden", "10")
lblCraftCols := myGui.Add("Text", "x" colL + 93 " y" craftY " Hidden", T("lbl_cols"))
edtCraftRows := myGui.Add("Edit", "x" colL + 115 " y" craftY - 2 " w35 h20 Number Hidden", "2")
lblCraftRows := myGui.Add("Text", "x" colL + 153 " y" craftY " Hidden", T("lbl_rows"))
craftY += 26

lblCraftAffix := myGui.Add("Text", "x" colL " y" craftY " w60 Right Hidden", T("lbl_affix"))
edtAffixes := myGui.Add("Edit", "x" colE " y" craftY - 2 " w215 h20 Hidden", "Flaring|Dictator's|Merciless")
craftY += 28

btnCraftStart := myGui.Add("Button", "x" colL " y" craftY " w145 h28 Hidden", T("btn_start"))
btnCraftStart.OnEvent("Click", OnCraftStart)
btnCraftStop := myGui.Add("Button", "x" colL + 155 " y" craftY " w145 h28 Hidden", T("btn_stop"))
btnCraftStop.OnEvent("Click", OnCraftStop)
btnCraftStop.Enabled := false

; ===== 洗地图面板 (初始隐藏) =====
mapY := panelY

; 模式选择
lblMapMode := myGui.Add("Text", "x" colL " y" mapY " w60 Right Hidden", T("lbl_map_mode"))
ddlMapMode := myGui.Add("DropDownList", "x" colE " y" mapY - 2 " w150 R3 Hidden", [T("map_mode_alch"), T("map_mode_chaos")])
ddlMapMode.Value := 1
mapY += 26

; 批量 + 网格
chkMapBatch := myGui.Add("Checkbox", "x" colL " y" mapY " Hidden", T("chk_batch"))
chkMapBatch.OnEvent("Click", OnMapBatchToggle)
edtMapCols := myGui.Add("Edit", "x" colL + 55 " y" mapY - 2 " w35 h20 Number Hidden", "4")
lblMapCols := myGui.Add("Text", "x" colL + 93 " y" mapY " Hidden", T("lbl_cols"))
edtMapRows := myGui.Add("Edit", "x" colL + 115 " y" mapY - 2 " w35 h20 Number Hidden", "3")
lblMapRows := myGui.Add("Text", "x" colL + 153 " y" mapY " Hidden", T("lbl_rows"))
mapY += 26

; 目标词缀
lblMapAffix := myGui.Add("Text", "x" colL " y" mapY " w60 Right Hidden", T("lbl_affix"))
edtMapAffixes := myGui.Add("Edit", "x" colE " y" mapY - 2 " w215 h20 Hidden", "")
mapY += 28

; 操作按钮
btnMapStart := myGui.Add("Button", "x" colL " y" mapY " w145 h28 Hidden", T("btn_start"))
btnMapStart.OnEvent("Click", OnMapStart)
btnMapStop := myGui.Add("Button", "x" colL + 155 " y" mapY " w145 h28 Hidden", T("btn_stop"))
btnMapStop.OnEvent("Click", OnMapStop)
btnMapStop.Enabled := false

; ===== 存包面板 (初始隐藏) =====
depY := panelY

; 网格行列
lblDepGrid := myGui.Add("Text", "x" colL " y" depY " w60 Right Hidden", T("lbl_grid"))
edtDepCols := myGui.Add("Edit", "x" colE " y" depY - 2 " w35 h20 Number Hidden", "12")
lblDepCols := myGui.Add("Text", "x" colE + 38 " y" depY " Hidden", T("lbl_cols"))
edtDepRows := myGui.Add("Edit", "x" colE + 60 " y" depY - 2 " w35 h20 Number Hidden", "5")
lblDepRows := myGui.Add("Text", "x" colE + 98 " y" depY " Hidden", T("lbl_rows"))
depY += 32

; 操作按钮
btnDepStart := myGui.Add("Button", "x" colL " y" depY " w145 h28 Hidden", T("btn_start"))
btnDepStart.OnEvent("Click", OnDepositStart)
btnDepStop := myGui.Add("Button", "x" colL + 155 " y" depY " w145 h28 Hidden", T("btn_stop"))
btnDepStop.OnEvent("Click", OnDepositStop)
btnDepStop.Enabled := false

; --- 速度选择 (公共) ---
speedY := 290
myGui.Add("Text", "x" midX + 10 " y" speedY + 3, T("lbl_speed"))
ddlSpeed := myGui.Add("DropDownList", "x" midX + 50 " y" speedY " w80", [T("speed_normal"), T("speed_fast")])
ddlSpeed.Value := 2
ddlSpeed.OnEvent("Change", OnSpeedChange)

; --- 状态栏 ---
statusY := 318
grpStatus := myGui.Add("GroupBox", "x" midX " y" statusY " w" midW " h42", T("grp_status"))
txtStatus := myGui.Add("Text", "xp+10 yp+17 w200", T("status_ready"))
txtCount := myGui.Add("Text", "x" midX + midW - 200 " yp w190 Right", T("op_count", "0", "0", "-"))

; ========== 右侧：日志 ==========
logX := 410
logW := 290
logH := statusY + 32

grpLog := myGui.Add("GroupBox", "x" logX " y5 w" logW " h" logH, T("grp_log"))
edtLog := myGui.Add("Edit", "xp+10 yp+18 w" logW - 20 " h" logH - 28 " ReadOnly Multi VScroll")

myGui.OnEvent("Close", OnGuiClose)
myGui.Show("w710 h" statusY + 50)

; 控件分组
configControls := [lblOrb1, edtOrb1Pos, txtOrb1Qty, btnSetOrb1, lblOrb2, edtOrb2Pos, txtOrb2Qty, btnSetOrb2, lblOrb3, edtOrb3Pos, txtOrb3Qty, btnSetOrb3, lblBase, edtBasePos, btnSetBase, lblP01, edtP01Pos, btnSetP01, lblP10, edtP10Pos, btnSetP10, lblInvBase, edtInvBasePos, btnSetInvBase, lblInvP01, edtInvP01Pos, btnSetInvP01, lblInvP10, edtInvP10Pos, btnSetInvP10]
craftControls := [chkCraftBatch, edtCraftCols, lblCraftCols, edtCraftRows, lblCraftRows, lblCraftAffix, edtAffixes, btnCraftStart, btnCraftStop]
mapControls := [lblMapMode, ddlMapMode, chkMapBatch, edtMapCols, lblMapCols, edtMapRows, lblMapRows, lblMapAffix, edtMapAffixes, btnMapStart, btnMapStop]
depositControls := [lblDepGrid, edtDepCols, lblDepCols, edtDepRows, lblDepRows, btnDepStart, btnDepStop]

HighlightFunc()
LoadConfig()
OnCraftBatchToggle()
OnMapBatchToggle()

; ==================== 功能切换 ====================
SwitchFunc(name) {
    global activeFunc
    if running
        return
    activeFunc := name
    for ctrl in configControls
        ctrl.Visible := (name = "config")
    for ctrl in craftControls
        ctrl.Visible := (name = "craft")
    for ctrl in mapControls
        ctrl.Visible := (name = "map")
    for ctrl in depositControls
        ctrl.Visible := (name = "deposit")
    HighlightFunc()
}

HighlightFunc() {
    btnFuncConfig.Opt((activeFunc = "config") ? "+Default" : "-Default")
    btnFuncCraft.Opt((activeFunc = "craft") ? "+Default" : "-Default")
    btnFuncMap.Opt((activeFunc = "map") ? "+Default" : "-Default")
    btnFuncDeposit.Opt((activeFunc = "deposit") ? "+Default" : "-Default")
}

; ==================== 语言切换 ====================
OnToggleLang(*) {
    global lang
    lang := (lang = "zh") ? "en" : "zh"
    RefreshUI()
    LogMsg("Language: " ((lang = "zh") ? "中文" : "English"))
}

RefreshUI() {
    myGui.Title := T("title") " v" APP_VERSION
    btnLang.Text := T("lang_btn")
    btnFuncConfig.Text := T("btn_config")
    btnFuncCraft.Text := T("btn_craft")
    btnFuncMap.Text := T("btn_map")
    btnFuncDeposit.Text := T("btn_deposit")
    grpWindow.Text := T("grp_window")
    if (poeHwnd = 0)
        edtTitle.Value := T("window_none")
    ; 配置
    lblOrb1.Text := T("lbl_orb1")
    lblOrb2.Text := T("lbl_orb2")
    lblOrb3.Text := T("lbl_orb3")
    if (orb1X = 0) edtOrb1Pos.Value := T("pos_none")
    if (orb2X = 0) edtOrb2Pos.Value := T("pos_none")
    if (orb3X = 0) edtOrb3Pos.Value := T("pos_none")
    lblBase.Text := T("lbl_base")
    if (baseX = 0) edtBasePos.Value := T("pos_none")
    lblP01.Text := T("lbl_p01")
    lblP10.Text := T("lbl_p10")
    if (p01X = 0) edtP01Pos.Value := T("pos_none")
    if (p10X = 0) edtP10Pos.Value := T("pos_none")
    lblInvBase.Text := T("lbl_inv_base")
    lblInvP01.Text := T("lbl_inv_p01")
    lblInvP10.Text := T("lbl_inv_p10")
    if (invBaseX = 0) edtInvBasePos.Value := T("pos_none")
    if (invP01X = 0) edtInvP01Pos.Value := T("pos_none")
    if (invP10X = 0) edtInvP10Pos.Value := T("pos_none")
    ; 改造
    chkCraftBatch.Text := T("chk_batch")
    lblCraftCols.Text := T("lbl_cols")
    lblCraftRows.Text := T("lbl_rows")
    lblCraftAffix.Text := T("lbl_affix")
    btnCraftStart.Text := T("btn_start")
    btnCraftStop.Text := T("btn_stop")
    ; 洗地图
    lblMapMode.Text := T("lbl_map_mode")
    ddlMapMode.Delete()
    ddlMapMode.Add([T("map_mode_alch"), T("map_mode_chaos")])
    ddlMapMode.Value := mapMode
    chkMapBatch.Text := T("chk_batch")
    lblMapCols.Text := T("lbl_cols")
    lblMapRows.Text := T("lbl_rows")
    lblMapAffix.Text := T("lbl_affix")
    btnMapStart.Text := T("btn_start")
    btnMapStop.Text := T("btn_stop")
    ; 存包
    lblDepGrid.Text := T("lbl_grid")
    lblDepCols.Text := T("lbl_cols")
    lblDepRows.Text := T("lbl_rows")
    btnDepStart.Text := T("btn_start")
    btnDepStop.Text := T("btn_stop")
    ; 状态
    grpStatus.Text := T("grp_status")
    if !running
        txtStatus.Value := T("status_ready")
    UpdateCount()
    grpLog.Text := T("grp_log")
}

; ==================== 工具函数 ====================
CaptureWindow() {
    global poeHwnd
    hwnd := WinGetID("A")
    title := WinGetTitle("A")
    if (hwnd = myGui.Hwnd)
        return false
    poeHwnd := hwnd
    edtTitle.Value := title
    LogMsg(T("log_capture", title))
    return true
}

ActivatePoe() {
    global poeHwnd
    if !poeHwnd || !WinExist("ahk_id " poeHwnd) {
        LogMsg(T("log_window_gone"))
        return false
    }
    WinActivate "ahk_id " poeHwnd
    WinWaitActive "ahk_id " poeHwnd,, 2
    return true
}

CheckWindow() {
    global poeHwnd
    if !WinActive("ahk_id " poeHwnd) {
        if !ActivatePoe() {
            LogMsg(T("log_window_lost"))
            return false
        }
    }
    return true
}

RecordPos(&outX, &outY) {
    ToolTip T("tip_wait")
    Sleep 3000
    if !CaptureWindow() {
        ToolTip ""
        return false
    }
    MouseGetPos &outX, &outY
    ToolTip ""
    return true
}

UpdateStatus(msg) {
    txtStatus.Value := msg
}

LogMsg(msg) {
    timeStr := FormatTime(, "HH:mm:ss")
    edtLog.Value := "[" timeStr "] " msg "`r`n" edtLog.Value
}

SetMapRunning(state) {
    global running
    running := state
    btnMapStart.Enabled := !state
    btnMapStop.Enabled := state
    ddlMapMode.Enabled := !state
    chkMapBatch.Enabled := !state
    edtMapRows.Enabled := !state
    edtMapCols.Enabled := !state
    edtMapAffixes.Enabled := !state
}

; ==================== 配置 - 坐标记录 ====================
RecordOrbBtn(orbIdx, &ox, &oy, posCtrl, qtyCtrl) {
    if RecordPos(&ox, &oy) {
        posCtrl.Value := ox ", " oy
        LogMsg(T("log_orb_pos", orbIdx, ox, oy))
        ShowOrbQty(qtyCtrl)
    }
}

OnSetOrb1(*) {
    global orb1X, orb1Y
    RecordOrbBtn(1, &orb1X, &orb1Y, edtOrb1Pos, txtOrb1Qty)
}
OnSetOrb2(*) {
    global orb2X, orb2Y
    RecordOrbBtn(2, &orb2X, &orb2Y, edtOrb2Pos, txtOrb2Qty)
}
OnSetOrb3(*) {
    global orb3X, orb3Y
    RecordOrbBtn(3, &orb3X, &orb3Y, edtOrb3Pos, txtOrb3Qty)
}
OnSetBase(*) {
    global baseX, baseY
    if RecordPos(&baseX, &baseY) {
        edtBasePos.Value := baseX ", " baseY
        LogMsg(T("log_base_pos", baseX, baseY))
        CalcOffset()
    }
}
OnSetP01(*) {
    global p01X, p01Y
    if RecordPos(&p01X, &p01Y) {
        edtP01Pos.Value := p01X ", " p01Y
        LogMsg(T("log_p01_pos", p01X, p01Y))
        CalcOffset()
    }
}
OnSetP10(*) {
    global p10X, p10Y
    if RecordPos(&p10X, &p10Y) {
        edtP10Pos.Value := p10X ", " p10Y
        LogMsg(T("log_p10_pos", p10X, p10Y))
        CalcOffset()
    }
}
CalcOffset() {
    global baseX, baseY, p01X, p01Y, p10X, p10Y, offsetX, offsetY
    if (baseX != 0 && p01X != 0)
        offsetX := p01X - baseX
    if (baseX != 0 && p10X != 0)
        offsetY := p10Y - baseY
    if (offsetX != 0 || offsetY != 0)
        LogMsg(T("log_offset", offsetX, offsetY))
}

GetItemPos(row, col) {
    global baseX, baseY, offsetX, offsetY
    return {x: baseX + col * offsetX, y: baseY + row * offsetY}
}

; ===== 背包坐标记录 =====
OnSetInvBase(*) {
    global invBaseX, invBaseY
    if RecordPos(&invBaseX, &invBaseY) {
        edtInvBasePos.Value := invBaseX ", " invBaseY
        LogMsg(T("log_inv_base_pos", invBaseX, invBaseY))
        CalcInvOffset()
    }
}
OnSetInvP01(*) {
    global invP01X, invP01Y
    if RecordPos(&invP01X, &invP01Y) {
        edtInvP01Pos.Value := invP01X ", " invP01Y
        LogMsg(T("log_inv_p01_pos", invP01X, invP01Y))
        CalcInvOffset()
    }
}
OnSetInvP10(*) {
    global invP10X, invP10Y
    if RecordPos(&invP10X, &invP10Y) {
        edtInvP10Pos.Value := invP10X ", " invP10Y
        LogMsg(T("log_inv_p10_pos", invP10X, invP10Y))
        CalcInvOffset()
    }
}
CalcInvOffset() {
    global invBaseX, invBaseY, invP01X, invP01Y, invP10X, invP10Y, invOffsetX, invOffsetY
    if (invBaseX != 0 && invP01X != 0)
        invOffsetX := invP01X - invBaseX
    if (invBaseX != 0 && invP10X != 0)
        invOffsetY := invP10Y - invBaseY
    if (invOffsetX != 0 || invOffsetY != 0)
        LogMsg(T("log_inv_offset", invOffsetX, invOffsetY))
}

GetInvPos(row, col) {
    global invBaseX, invBaseY, invOffsetX, invOffsetY
    return {x: invBaseX + col * invOffsetX, y: invBaseY + row * invOffsetY}
}

OnCraftBatchToggle(*) {
    isBatch := chkCraftBatch.Value
    edtCraftCols.Enabled := isBatch
    edtCraftRows.Enabled := isBatch
}

OnMapBatchToggle(*) {
    isBatch := chkMapBatch.Value
    edtMapCols.Enabled := isBatch
    edtMapRows.Enabled := isBatch
}

; ==================== 改造功能 ====================
OnCraftStart(*) {
    global running, opCount, hitCount
    global orb1X, orb1Y, orb2X, orb2Y, orb3X, orb3Y
    global baseX, baseY, offsetX, offsetY, craftCols, craftRows

    isBatch := chkCraftBatch.Value

    if (!isBatch) {
        ; 单个改造：只需 orb1 + base
        if (orb1X = 0 || baseX = 0) {
            MsgBox T("msg_set_pos")
            return
        }
    } else {
        ; 批量改造：需要 orb1 + base + offset
        craftCols := Integer(edtCraftCols.Value)
        craftRows := Integer(edtCraftRows.Value)
        if (orb1X = 0 || baseX = 0 || (craftCols > 1 && offsetX = 0) || (craftRows > 1 && offsetY = 0)) {
            MsgBox T("msg_set_pos")
            return
        }
    }

    if !ActivatePoe() {
        MsgBox T("msg_no_window")
        return
    }

    running := true
    opCount := 0
    hitCount := 0
    btnCraftStart.Enabled := false
    btnCraftStop.Enabled := true
    edtAffixes.Enabled := false

    if (!isBatch) {
        ; ====== 单个改造模式 ======
        UpdateStatus(T("status_crafting"))
        LogMsg(T("log_start", edtAffixes.Value))

        ; 开始前读取通货数量
        orbRemain := ReadOrbStack(orb1X, orb1Y)
        if (orbRemain >= 0)
            txtOrb1Qty.Value := "×" orbRemain

        ; 预检查：使用通货前先检查物品是否已符合条件
        if (edtAffixes.Value != "") {
            preClip := ReadItemClip(baseX, baseY)
            if (preClip != "") {
                preHit := MatchAffixes(preClip, edtAffixes.Value)
                if (preHit != "") {
                    SoundPlay "*48"
                    running := false
                    hitCount++
                    UpdateCount()
                    LogMsg(T("log_found", 0, preHit))
                    UpdateStatus(T("status_found"))
                    MsgBox T("msg_found", 0, preClip)
                    btnCraftStart.Enabled := true
                    btnCraftStop.Enabled := false
                    edtAffixes.Enabled := true
                    return
                }
            }
        }

        loop {
            if !running
                break
            if !CheckWindow() {
                running := false
                break
            }

            ; 快速阶段：右键取通货 → 左键应用
            HumanClick "Right", orb1X, orb1Y
            if (orbRemain > 0) {
                orbRemain--
                txtOrb1Qty.Value := "×" orbRemain
            }
            HumanClick "Left", baseX, baseY

            ; 关键阶段：复制并验证是物品信息
            clip := ReadItemClip(baseX, baseY)
            if (clip = "" || !running)
                break

            opCount++
            UpdateCount()
            LogMsg(T("log_applied", opCount, (orbRemain >= 0) ? orbRemain : "?"))

            ; 通货耗尽检测
            if (orbRemain = 0) {
                LogMsg(T("log_orb_empty"))
                running := false
            }

            ; 提取词条行
            affixLines := []
            for line in StrSplit(clip, "`n", "`r") {
                line := Trim(line)
                if (line != "" && !RegExMatch(line, "^(Item Class|Rarity|Item Level|--------|\{|Unidentified|Corrupted)"))
                    affixLines.Push(line)
            }
            LogMsg(T("log_affixes", StrJoin(affixLines, " | ")))

            ; 比对阶段：命中立即停止
            hitAffix := MatchAffixes(clip, edtAffixes.Value)

            if (hitAffix != "") {
                SoundPlay "*48"
                running := false
                hitCount++
                UpdateCount()
                LogMsg(T("log_found", opCount, hitAffix))
                UpdateStatus(T("status_found"))
                MsgBox T("msg_found", opCount, clip)
                break
            }

            LogMsg(T("log_miss", edtAffixes.Value))
        }
    } else {
        ; ====== 批量改造模式 ======
        LogMsg(T("log_bc_start", craftCols, craftRows, edtAffixes.Value))

        ; 构建通货队列
        orbQueue := []
        if (orb1X != 0)
            orbQueue.Push({x: orb1X, y: orb1Y, id: 1, qtyCtrl: txtOrb1Qty})
        if (orb2X != 0)
            orbQueue.Push({x: orb2X, y: orb2Y, id: 2, qtyCtrl: txtOrb2Qty})
        if (orb3X != 0)
            orbQueue.Push({x: orb3X, y: orb3Y, id: 3, qtyCtrl: txtOrb3Qty})

        if (orbQueue.Length = 0) {
            MsgBox T("msg_set_pos")
            btnCraftStart.Enabled := true
            btnCraftStop.Enabled := false
            edtAffixes.Enabled := true
            running := false
            return
        }

        orbIdx := 1
        curOrb := orbQueue[1]

        ; 开始前读取当前通货数量
        orbRemain := ReadOrbStack(curOrb.x, curOrb.y)
        if (orbRemain >= 0)
            curOrb.qtyCtrl.Value := "×" orbRemain

        row := 0
        while (row < craftRows && running) {
            col := 0
            while (col < craftCols && running) {
                pos := GetItemPos(row, col)
                UpdateStatus(T("status_bc", row, col, curOrb.id))

                ; 预检查：使用通货前先检查物品是否已符合条件
                itemDone := false
                if (edtAffixes.Value != "") {
                    preClip := ReadItemClip(pos.x, pos.y)
                    if (preClip != "") {
                        preHit := MatchAffixes(preClip, edtAffixes.Value)
                        if (preHit != "") {
                            hitCount++
                            UpdateCount()
                            LogMsg(T("log_bc_hit", row, col, preHit))
                            SoundPlay "*48"
                            itemDone := true
                        }
                    }
                }

                ; 对当前物品持续改造直到命中
                while (!itemDone && running) {
                    if !CheckWindow() {
                        running := false
                        break
                    }

                    ; 快速阶段：取通货 + 应用
                    HumanClick "Right", curOrb.x, curOrb.y
                    if (orbRemain > 0) {
                        orbRemain--
                        curOrb.qtyCtrl.Value := "×" orbRemain
                    }
                    HumanClick "Left", pos.x, pos.y

                    opCount++
                    UpdateCount()

                    ; 关键阶段：复制并验证是物品信息
                    clip := ReadItemClip(pos.x, pos.y)
                    if (clip = "" || !running)
                        break

                    ; 比对阶段
                    hitAffix := MatchAffixes(clip, edtAffixes.Value)

                    if (hitAffix != "") {
                        hitCount++
                        UpdateCount()
                        LogMsg(T("log_bc_hit", row, col, hitAffix))
                        SoundPlay "*48"
                        itemDone := true
                    }

                    ; 通货耗尽检查
                    if (orbRemain = 0 && !itemDone) {
                        if (orbIdx < orbQueue.Length) {
                            oldId := curOrb.id
                            orbIdx++
                            curOrb := orbQueue[orbIdx]
                            orbRemain := ReadOrbStack(curOrb.x, curOrb.y)
                            if (orbRemain >= 0)
                                curOrb.qtyCtrl.Value := "×" orbRemain
                            LogMsg(T("log_bc_switch", oldId, curOrb.id))
                        } else {
                            LogMsg(T("log_bc_depleted"))
                            running := false
                            break
                        }
                    }
                }

                col++
                if (running && itemDone && (col < craftCols || row + 1 < craftRows)) {
                    LogMsg(T("log_bc_next", (col >= craftCols) ? row + 1 : row, (col >= craftCols) ? 0 : col))
                    ThinkPause()
                }
            }
            row++
        }

        LogMsg(T("log_bc_done", opCount, hitCount))
        if (row >= craftRows && hitCount = craftRows * craftCols) {
            UpdateStatus(T("status_found"))
            MsgBox T("msg_bc_done", opCount, hitCount)
        } else {
            UpdateStatus(T("status_stopped"))
        }
    }

    btnCraftStart.Enabled := true
    btnCraftStop.Enabled := false
    edtAffixes.Enabled := true
    running := false
}

OnCraftStop(*) {
    global running
    running := false
    LogMsg(T("log_manual_stop", opCount))
    UpdateStatus(T("status_stopped"))
    btnCraftStart.Enabled := true
    btnCraftStop.Enabled := false
    edtAffixes.Enabled := true
}

UpdateCount() {
    global opCount, hitCount, txtCount
    avg := hitCount > 0 ? Round(opCount / hitCount, 1) : "-"
    txtCount.Value := T("op_count", opCount, hitCount, avg)
}

; ==================== 剪贴板工具 ====================
; 检查剪贴板内容是否是物品（不是通货）
IsItemClip(clip) {
    return (clip != "" && InStr(clip, "Rarity:") && !InStr(clip, "Stackable Currency"))
}

; 从剪贴板读取 Stack Size，返回剩余数量，失败返回 -1
GetStackSize(clip) {
    if RegExMatch(clip, "Stack Size: ([\d,]+)", &m) {
        return Integer(StrReplace(m[1], ",", ""))
    }
    return -1
}

; 读取并显示通货数量到指定控件
ShowOrbQty(ctrl) {
    A_Clipboard := ""
    Send "^c"
    if ClipWait(0.3) {
        qty := GetStackSize(A_Clipboard)
        ctrl.Value := (qty >= 0) ? "×" qty : ""
    }
}

; 复制通货信息并返回剩余数量，失败返回 -1
ReadOrbStack(orbX, orbY) {
    BezierMove(orbX, orbY - 40)
    BezierMove(orbX, orbY)
    A_Clipboard := ""
    Send "^c"
    if ClipWait(0.3)
        return GetStackSize(A_Clipboard)
    return -1
}

; 读取并验证物品剪贴板信息，带重试上限
ReadItemClip(ix, iy, maxRetries := 5) {
    global running
    clip := ""
    retries := 0
    GaussSleep(120, 25, 80)
    while (!IsItemClip(clip) && running && retries < maxRetries) {
        BezierMove(ix, iy - 50)
        BezierMove(ix, iy)
        A_Clipboard := ""
        Send "^c"
        if ClipWait(0.3) {
            clip := A_Clipboard
            if !IsItemClip(clip) {
                Click "Left " ix " " iy
                GaussSleep(25, 6, 12)
                clip := ""
            }
        } else {
            Click "Left " ix " " iy
            GaussSleep(25, 6, 12)
        }
        retries++
    }
    return clip
}

; ==================== 仿真鼠标移动 ====================
; 速度档位: 慢=5  标准=10  快=15，整体提速20%
MS(base) {
    global moveSpeed
    scale := Max(0.2, (21 - moveSpeed) / 11 * 0.8)
    return Round(base * scale)
}

; 带最低下限的延时，叠加 12% 高斯噪声让节奏更像人手
MSF(base, floor) {
    val := MS(base)
    val := Round(val + GaussRand(val * 0.12))
    return Max(floor, val)
}

; ==================== 高斯随机 ====================
; Box-Muller 变换：返回服从 N(0, stdDev) 的随机样本
GaussRand(stdDev) {
    u1 := Random(0.0001, 1.0)
    u2 := Random(0.0, 1.0)
    z := Sqrt(-2 * Ln(u1)) * Cos(2 * 3.14159265 * u2)
    return z * stdDev
}

; 高斯延时：均值 ± 标准差，clamp 到 floor 防止过短
; stdDev 默认 mean*15%，floor 默认 mean*0.5
GaussSleep(mean, stdDev := 0, floor := 0) {
    if (stdDev = 0)
        stdDev := mean * 0.15
    if (floor = 0)
        floor := Round(mean * 0.5)
    val := Round(mean + GaussRand(stdDev))
    if (val < floor)
        val := floor
    Sleep val
}

; 偶发"思考停顿"：以 probability 概率插入一段较长延时，模拟人偶尔走神
ThinkPause(probability := 0.01) {
    if (Random(0.0, 1.0) < probability)
        Sleep Random(200, 800)
}

OnSpeedChange(*) {
    global moveSpeed, ddlSpeed
    speeds := [15, 18]
    moveSpeed := speeds[ddlSpeed.Value]
}

StrJoin(arr, sep) {
    out := ""
    for i, v in arr
        out .= (i > 1 ? sep : "") v
    return out
}

; 词缀匹配（正则模式，兼容 poe.re）
; 输入格式: "Flaring|Dictator's|Merciless" 用 | 分隔
MatchAffixes(clip, affixText) {
    affixText := Trim(affixText)
    if (affixText = "")
        return ""
    try {
        if RegExMatch(clip, "i)(" affixText ")", &m)
            return m[0]
    }
    return ""
}

; 二次贝塞尔曲线移动，模拟人手轨迹
BezierMove(x2, y2) {
    global moveSpeed
    steps := Max(4, 22 - moveSpeed)   ; speed1→21步  speed20→2→clamped 4步
    MouseGetPos &x1, &y1
    ; 随机控制点，在起终点中点附近偏移
    cpX := (x1 + x2) / 2 + Random(-45, 45)
    cpY := (y1 + y2) / 2 + Random(-45, 45)
    loop steps {
        t := A_Index / steps
        px := Round((1 - t) ** 2 * x1 + 2 * t * (1 - t) * cpX + t ** 2 * x2)
        py := Round((1 - t) ** 2 * y1 + 2 * t * (1 - t) * cpY + t ** 2 * y2)
        MouseMove px, py, 0
        ; 两端慢中间快（ease in/out），用 Sin 模拟，整体受速度缩放
        Sleep MS(Round(5 + 6 * Sin(t * 3.14159)))
    }
}

; 仿真点击：贝塞尔移动 + 高斯位置抖动 + 高斯延时
HumanClick(button, x, y) {
    jx := x + Round(GaussRand(1.5))
    jy := y + Round(GaussRand(1.5))
    BezierMove(jx, jy)
    Click button " " jx " " jy
    GaussSleep(25, 6, 12)
}

; 仿真 Ctrl+左键：用于存包
HumanCtrlClick(x, y) {
    jx := x + Round(GaussRand(1.5))
    jy := y + Round(GaussRand(1.5))
    BezierMove(jx, jy)
    Send "{Ctrl down}"
    GaussSleep(15, 4, 8)
    Click "Left " jx " " jy
    GaussSleep(15, 4, 8)
    Send "{Ctrl up}"
    GaussSleep(25, 6, 12)
}

; ==================== 洗地图功能 ====================
MapReady(batch) {
    global orb1X, orb2X, orb3X, baseX, p01X, p10X, offsetX, offsetY, mapMode
    ; 模式1需要点金+重铸, 模式2需要混沌
    if (mapMode = 1 && (orb1X = 0 || orb2X = 0)) {
        MsgBox T("msg_set_pos")
        return false
    }
    if (mapMode = 2 && orb3X = 0) {
        MsgBox T("msg_set_pos")
        return false
    }
    if (baseX = 0) {
        MsgBox T("msg_set_pos")
        return false
    }
    ; 批量模式需要偏移定位
    if (batch && (p01X = 0 || p10X = 0 || offsetX = 0 || offsetY = 0)) {
        MsgBox T("msg_set_pos")
        return false
    }
    if !ActivatePoe() {
        MsgBox T("msg_no_window")
        return false
    }
    return true
}

; 对单个地图执行洗图，返回洗的次数
RollSingleMap(mx, my, row, col) {
    global running, orb1X, orb1Y, orb2X, orb2Y, orb3X, orb3Y
    global opCount, mapMode, hitCount
    rollCount := 0
    affixText := edtMapAffixes.Value

    ; 预检查：使用通货前先检查地图是否已符合条件
    if (affixText != "") {
        preClip := ReadItemClip(mx, my)
        if (preClip != "") {
            preHit := MatchAffixes(preClip, affixText)
            if (preHit != "") {
                hitCount++
                UpdateCount()
                LogMsg(T("log_map_hit", row, col, preHit))
                SoundPlay "*48"
                return 0
            }
        }
    }

    loop {
        if !running
            break
        if !CheckWindow() {
            running := false
            break
        }

        rollCount++
        opCount++
        UpdateCount()

        if (mapMode = 1) {
            ; 点金+重铸模式: 右键点金 → 左键地图
            HumanClick "Right", orb1X, orb1Y
            Sleep MSF(200, 80)
            HumanClick "Left", mx, my
        } else {
            ; 混沌石模式: 右键混沌 → 左键地图
            HumanClick "Right", orb3X, orb3Y
            Sleep MSF(200, 80)
            HumanClick "Left", mx, my
        }

        ; 读取物品信息
        clip := ReadItemClip(mx, my)
        if (clip = "" || !running)
            break

        ; 匹配词缀
        hitAffix := MatchAffixes(clip, affixText)

        if (hitAffix != "") {
            hitCount++
            UpdateCount()
            LogMsg(T("log_map_hit", row, col, hitAffix))
            SoundPlay "*48"
            break
        }

        LogMsg(T("log_map_rolling", row, col, rollCount))

        if (mapMode = 1) {
            ; 未命中 → 右键重铸 → 左键地图(去除属性)
            HumanClick "Right", orb2X, orb2Y
            Sleep MSF(200, 80)
            HumanClick "Left", mx, my
            Sleep MSF(300, 100)
        }

        GaussSleep(100, 25, 60)
    }

    return rollCount
}

OnMapStart(*) {
    global running, opCount, hitCount, mapMode, mapRows, mapCols
    mapMode := ddlMapMode.Value
    isBatch := chkMapBatch.Value

    if !MapReady(isBatch)
        return

    SetMapRunning(true)
    opCount := 0
    hitCount := 0
    modeName := (mapMode = 1) ? T("map_mode_alch") : T("map_mode_chaos")

    if isBatch {
        mapRows := Integer(edtMapRows.Value)
        mapCols := Integer(edtMapCols.Value)
        LogMsg(T("log_map_start", mapCols, mapRows, modeName, edtMapAffixes.Value))

        row := 0
        while (row < mapRows && running) {
            col := 0
            while (col < mapCols && running) {
                UpdateStatus(T("status_rolling", row, col))
                pos := GetItemPos(row, col)
                RollSingleMap(pos.x, pos.y, row, col)
                col++
                ThinkPause()
            }
            row++
        }
    } else {
        LogMsg(T("log_map_start", 1, 1, modeName, edtMapAffixes.Value))
        UpdateStatus(T("status_rolling", "0", "0"))
        pos := GetItemPos(0, 0)
        RollSingleMap(pos.x, pos.y, 0, 0)
    }

    SetMapRunning(false)
    if running {
        running := false
        SoundPlay "*48"
        LogMsg(T("log_map_done", opCount, hitCount))
        UpdateStatus(T("status_map_done"))
        MsgBox T("msg_map_done", opCount, hitCount)
    } else {
        UpdateStatus(T("status_stopped"))
    }
}

OnMapStop(*) {
    global running
    running := false
    LogMsg(T("log_manual_stop", opCount))
    UpdateStatus(T("status_stopped"))
    SetMapRunning(false)
}

; ==================== 存包功能 ====================
SetDepositRunning(state) {
    global running
    running := state
    btnDepStart.Enabled := !state
    btnDepStop.Enabled := state
    edtDepCols.Enabled := !state
    edtDepRows.Enabled := !state
}

OnDepositStart(*) {
    global running, opCount, hitCount, depositCols, depositRows
    global invBaseX, invBaseY, invOffsetX, invOffsetY

    depositCols := Integer(edtDepCols.Value)
    depositRows := Integer(edtDepRows.Value)

    ; 校验坐标：单格无需偏移，多格需要
    if (invBaseX = 0
        || (depositCols > 1 && invOffsetX = 0)
        || (depositRows > 1 && invOffsetY = 0)) {
        MsgBox T("msg_no_inv")
        return
    }

    if !ActivatePoe() {
        MsgBox T("msg_no_window")
        return
    }

    SetDepositRunning(true)
    opCount := 0
    hitCount := 0
    LogMsg(T("log_deposit_start", depositCols, depositRows))

    row := 0
    while (row < depositRows && running) {
        col := 0
        while (col < depositCols && running) {
            if !CheckWindow() {
                running := false
                break
            }
            UpdateStatus(T("status_depositing", row, col))
            pos := GetInvPos(row, col)
            HumanCtrlClick(pos.x, pos.y)
            opCount++
            UpdateCount()
            col++
        }
        row++
    }

    if running {
        SoundPlay "*48"
        LogMsg(T("log_deposit_done", opCount))
        UpdateStatus(T("status_deposit_done"))
        MsgBox T("msg_deposit_done", opCount)
    } else {
        UpdateStatus(T("status_stopped"))
    }
    SetDepositRunning(false)
}

OnDepositStop(*) {
    global running
    running := false
    LogMsg(T("log_manual_stop", opCount))
    UpdateStatus(T("status_stopped"))
    SetDepositRunning(false)
}

; ==================== 配置保存/加载 ====================
SaveConfig() {
    global
    IniWrite lang, CONFIG_FILE, "General", "lang"
    IniWrite activeFunc, CONFIG_FILE, "General", "activeFunc"
    IniWrite moveSpeed, CONFIG_FILE, "General", "moveSpeed"
    IniWrite edtAffixes.Value, CONFIG_FILE, "Craft", "affixes"
    IniWrite chkCraftBatch.Value, CONFIG_FILE, "Craft", "batch"
    IniWrite edtCraftCols.Value, CONFIG_FILE, "Craft", "cols"
    IniWrite edtCraftRows.Value, CONFIG_FILE, "Craft", "rows"
    IniWrite ddlMapMode.Value, CONFIG_FILE, "Map", "mode"
    IniWrite chkMapBatch.Value, CONFIG_FILE, "Map", "batch"
    IniWrite edtMapRows.Value, CONFIG_FILE, "Map", "rows"
    IniWrite edtMapCols.Value, CONFIG_FILE, "Map", "cols"
    IniWrite edtMapAffixes.Value, CONFIG_FILE, "Map", "affixes"
    IniWrite edtDepCols.Value, CONFIG_FILE, "Deposit", "cols"
    IniWrite edtDepRows.Value, CONFIG_FILE, "Deposit", "rows"
    ; 坐标
    IniWrite orb1X, CONFIG_FILE, "Coords", "orb1X"
    IniWrite orb1Y, CONFIG_FILE, "Coords", "orb1Y"
    IniWrite orb2X, CONFIG_FILE, "Coords", "orb2X"
    IniWrite orb2Y, CONFIG_FILE, "Coords", "orb2Y"
    IniWrite orb3X, CONFIG_FILE, "Coords", "orb3X"
    IniWrite orb3Y, CONFIG_FILE, "Coords", "orb3Y"
    IniWrite baseX, CONFIG_FILE, "Coords", "baseX"
    IniWrite baseY, CONFIG_FILE, "Coords", "baseY"
    IniWrite p01X, CONFIG_FILE, "Coords", "p01X"
    IniWrite p01Y, CONFIG_FILE, "Coords", "p01Y"
    IniWrite p10X, CONFIG_FILE, "Coords", "p10X"
    IniWrite p10Y, CONFIG_FILE, "Coords", "p10Y"
    IniWrite invBaseX, CONFIG_FILE, "Coords", "invBaseX"
    IniWrite invBaseY, CONFIG_FILE, "Coords", "invBaseY"
    IniWrite invP01X, CONFIG_FILE, "Coords", "invP01X"
    IniWrite invP01Y, CONFIG_FILE, "Coords", "invP01Y"
    IniWrite invP10X, CONFIG_FILE, "Coords", "invP10X"
    IniWrite invP10Y, CONFIG_FILE, "Coords", "invP10Y"
}

LoadConfig() {
    global
    if !FileExist(CONFIG_FILE)
        return
    lang := IniRead(CONFIG_FILE, "General", "lang", "zh")
    activeFunc := IniRead(CONFIG_FILE, "General", "activeFunc", "config")
    moveSpeed := Integer(IniRead(CONFIG_FILE, "General", "moveSpeed", "18"))
    speeds := [15, 18]
    for i, s in speeds {
        if (s = moveSpeed) {
            ddlSpeed.Value := i
            break
        }
    }
    edtAffixes.Value := IniRead(CONFIG_FILE, "Craft", "affixes", "Flaring|Dictator's|Merciless")
    chkCraftBatch.Value := Integer(IniRead(CONFIG_FILE, "Craft", "batch", "0"))
    edtCraftCols.Value := IniRead(CONFIG_FILE, "Craft", "cols", "10")
    edtCraftRows.Value := IniRead(CONFIG_FILE, "Craft", "rows", "2")
    ddlMapMode.Value := Integer(IniRead(CONFIG_FILE, "Map", "mode", "1"))
    mapMode := ddlMapMode.Value
    chkMapBatch.Value := Integer(IniRead(CONFIG_FILE, "Map", "batch", "0"))
    edtMapRows.Value := IniRead(CONFIG_FILE, "Map", "rows", "3")
    edtMapCols.Value := IniRead(CONFIG_FILE, "Map", "cols", "4")
    edtMapAffixes.Value := IniRead(CONFIG_FILE, "Map", "affixes", "")
    edtDepCols.Value := IniRead(CONFIG_FILE, "Deposit", "cols", "12")
    edtDepRows.Value := IniRead(CONFIG_FILE, "Deposit", "rows", "5")
    ; 坐标
    orb1X := Integer(IniRead(CONFIG_FILE, "Coords", "orb1X", "0"))
    orb1Y := Integer(IniRead(CONFIG_FILE, "Coords", "orb1Y", "0"))
    orb2X := Integer(IniRead(CONFIG_FILE, "Coords", "orb2X", "0"))
    orb2Y := Integer(IniRead(CONFIG_FILE, "Coords", "orb2Y", "0"))
    orb3X := Integer(IniRead(CONFIG_FILE, "Coords", "orb3X", "0"))
    orb3Y := Integer(IniRead(CONFIG_FILE, "Coords", "orb3Y", "0"))
    baseX := Integer(IniRead(CONFIG_FILE, "Coords", "baseX", "0"))
    baseY := Integer(IniRead(CONFIG_FILE, "Coords", "baseY", "0"))
    p01X := Integer(IniRead(CONFIG_FILE, "Coords", "p01X", "0"))
    p01Y := Integer(IniRead(CONFIG_FILE, "Coords", "p01Y", "0"))
    p10X := Integer(IniRead(CONFIG_FILE, "Coords", "p10X", "0"))
    p10Y := Integer(IniRead(CONFIG_FILE, "Coords", "p10Y", "0"))
    invBaseX := Integer(IniRead(CONFIG_FILE, "Coords", "invBaseX", "0"))
    invBaseY := Integer(IniRead(CONFIG_FILE, "Coords", "invBaseY", "0"))
    invP01X := Integer(IniRead(CONFIG_FILE, "Coords", "invP01X", "0"))
    invP01Y := Integer(IniRead(CONFIG_FILE, "Coords", "invP01Y", "0"))
    invP10X := Integer(IniRead(CONFIG_FILE, "Coords", "invP10X", "0"))
    invP10Y := Integer(IniRead(CONFIG_FILE, "Coords", "invP10Y", "0"))
    if (orb1X != 0)
        edtOrb1Pos.Value := orb1X ", " orb1Y
    if (orb2X != 0)
        edtOrb2Pos.Value := orb2X ", " orb2Y
    if (orb3X != 0)
        edtOrb3Pos.Value := orb3X ", " orb3Y
    if (baseX != 0)
        edtBasePos.Value := baseX ", " baseY
    if (p01X != 0)
        edtP01Pos.Value := p01X ", " p01Y
    if (p10X != 0)
        edtP10Pos.Value := p10X ", " p10Y
    if (invBaseX != 0)
        edtInvBasePos.Value := invBaseX ", " invBaseY
    if (invP01X != 0)
        edtInvP01Pos.Value := invP01X ", " invP01Y
    if (invP10X != 0)
        edtInvP10Pos.Value := invP10X ", " invP10Y
    CalcOffset()
    CalcInvOffset()
    SwitchFunc(activeFunc)
    RefreshUI()
}

; ==================== 通用 ====================
OnGuiClose(*) {
    global running
    running := false
    SaveConfig()
    Send "{Shift Up}"
    Send "{Alt Up}"
    ExitApp
}

; ==================== 热键 ====================
HkCapture() {
    if !poeHwnd || !WinActive("ahk_id " poeHwnd)
        CaptureWindow()
}

RecordOrbHotkey(orbIdx, &ox, &oy, posCtrl, qtyCtrl) {
    HkCapture()
    MouseGetPos &ox, &oy
    posCtrl.Value := ox ", " oy
    LogMsg(T("log_orb_pos", orbIdx, ox, oy))
    ShowOrbQty(qtyCtrl)
    ToolTip T("lbl_orb" . orbIdx) " " ox ", " oy
    SetTimer () => ToolTip(), -2000
}

RecordPosHotkey(&outX, &outY, posCtrl, logKey, calcFn, tipLabel) {
    HkCapture()
    MouseGetPos &outX, &outY
    posCtrl.Value := outX ", " outY
    LogMsg(T(logKey, outX, outY))
    calcFn()
    ToolTip tipLabel ": " outX ", " outY
    SetTimer () => ToolTip(), -2000
}

!1:: {
    global orb1X, orb1Y
    RecordOrbHotkey(1, &orb1X, &orb1Y, edtOrb1Pos, txtOrb1Qty)
}

!2:: {
    global orb2X, orb2Y
    RecordOrbHotkey(2, &orb2X, &orb2Y, edtOrb2Pos, txtOrb2Qty)
}

!3:: {
    global orb3X, orb3Y
    RecordOrbHotkey(3, &orb3X, &orb3Y, edtOrb3Pos, txtOrb3Qty)
}

!0:: {
    global baseX, baseY
    RecordPosHotkey(&baseX, &baseY, edtBasePos, "log_base_pos", CalcOffset, "(0,0)")
}

!8:: {
    global p01X, p01Y
    RecordPosHotkey(&p01X, &p01Y, edtP01Pos, "log_p01_pos", CalcOffset, "(x+1)")
}

!9:: {
    global p10X, p10Y
    RecordPosHotkey(&p10X, &p10Y, edtP10Pos, "log_p10_pos", CalcOffset, "(y+1)")
}

!4:: {
    global invBaseX, invBaseY
    RecordPosHotkey(&invBaseX, &invBaseY, edtInvBasePos, "log_inv_base_pos", CalcInvOffset, "Inv(0,0)")
}

!5:: {
    global invP01X, invP01Y
    RecordPosHotkey(&invP01X, &invP01Y, edtInvP01Pos, "log_inv_p01_pos", CalcInvOffset, "Inv(x+1)")
}

!6:: {
    global invP10X, invP10Y
    RecordPosHotkey(&invP10X, &invP10Y, edtInvP10Pos, "log_inv_p10_pos", CalcInvOffset, "Inv(y+1)")
}

Home:: {
    if (activeFunc = "craft")
        OnCraftStart()
    else if (activeFunc = "map")
        OnMapStart()
    else if (activeFunc = "deposit")
        OnDepositStart()
}

End:: {
    if (activeFunc = "craft")
        OnCraftStop()
    else if (activeFunc = "map")
        OnMapStop()
    else if (activeFunc = "deposit")
        OnDepositStop()
}
