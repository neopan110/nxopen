/**
 * @file hvac_entry_point.cpp
 * @brief HVAC箱体参数化系统 - NX插件入口点 + 用户界面
 * @details 实现NX Open DLL必须的入口函数(ufusr/ufsta)和参数输入对话框。
 *
 * 【NX插件入口函数说明】
 * - ufusr(): 用户手动执行时调用 (File→Execute→NX Open)
 * - ufsta(): NX启动时自动加载 (放在startup目录时)
 * - ufusr_ask_unload(): 控制DLL卸载时机
 *
 * 【对话框方案】
 * - Phase-1: 使用UF_UI_message_dialog + UF_STYLER (全版本兼容)
 * - Phase-2: 升级为Block Styler对话框 (NX1980+ 更美观)
 *
 * 【坑位规避】
 * - PIT-004: 操作前显式获取WorkPart
 * - PIT-006: 入口处设置mm单位
 * - PIT-007: Point3d显式初始化
 *
 * @version 1.0
 * @date 2026-05-10
 */

/* ============================================================
 * 系统头文件
 * ============================================================ */
#include "hvac_common.h"
#include "hvac_parameters.h"
#include "core/parameter_engine.h"
#include "modules/shell_builder.h"
#include "utils/error_handler.h"

#include <uf.h>
#include <uf_ui.h>
#include <uf_part.h>
#include <uf_modl.h>
#include <uf_exit.h>

/* ============================================================
 * 内部函数前向声明
 * ============================================================ */

static void doHvacBuild();
static bool showParameterDialog(HvacParameterSet& params);
static void showResultReport(const ShellBuildResult& result,
                             const std::vector<ValidationItem>& validations);

/* ============================================================
 * NX入口函数: ufusr
 * ============================================================
 * 用户通过 File→Execute→NX Open 选择本DLL时调用。
 * 这是插件的主入口。
 */
extern "C" HVAC_API void ufusr(char* param, int* retcod, int paramLen)
{
    *retcod = 0;

    // 初始化UF环境
    if (UF_initialize() != 0) {
        *retcod = 1;
        return;
    }

    try {
        doHvacBuild();
    } catch (const std::exception& ex) {
        // 最外层异常兜底
        UF_UI_open_listing_window();
        std::string msg = std::string("[HVAC FATAL] ") + ex.what();
        UF_UI_write_listing_window(msg.c_str());
        UF_UI_write_listing_window("\n");
        *retcod = 2;
    }

    UF_terminate();
}

/* ============================================================
 * NX入口函数: ufusr_ask_unload
 * ============================================================
 * 控制DLL卸载策略:
 * - UF_UNLOAD_IMMEDIATELY: 执行完立即卸载(开发调试用, 方便重编译)
 * - UF_UNLOAD_SEL_DIALOG: NX关闭时卸载
 * - UF_UNLOAD_UG_TERMINATE: 永不自动卸载
 */
extern "C" int ufusr_ask_unload(void)
{
    return UF_UNLOAD_IMMEDIATELY; // 开发阶段用IMMEDIATELY, 发布改为SEL_DIALOG
}

/* ============================================================
 * NX入口函数: ufsta (可选, startup自动加载)
 * ============================================================ */
extern "C" void ufsta(char* param, int* retcod, int paramLen)
{
    *retcod = 0;
    // startup模式: 仅注册菜单, 不执行构建
    // Phase-2: 注册自定义菜单项
}

/* ============================================================
 * 主业务逻辑
 * ============================================================ */

static void doHvacBuild()
{
    HvacErrorHandler::logInfo("========================================");
    HvacErrorHandler::logInfo("  HVAC Box Parametric Builder v1.0");
    HvacErrorHandler::logInfo("  Target: NX2306 (compatible NX12+)");
    HvacErrorHandler::logInfo("========================================");

    // 1. 检查WorkPart (PIT-004)
    tag_t workPartTag = NULL_TAG;
    UF_PART_ask_display_part(&workPartTag);

    if (workPartTag == NULL_TAG) {
        UF_UI_open_listing_window();
        UF_UI_write_listing_window("[ERROR] No work part open. Please open or create a part first.\n");

        // 提示用户
        int response = 0;
        UF_UI_message_dialog("HVAC Builder",
                              UF_UI_MESSAGE_ERROR,
                              "Please open or create a Part file before running HVAC Builder.",
                              1, /* button count */
                              (char*[]){"OK"},
                              &response);
        return;
    }

    // 2. 初始化参数引擎
    HvacParameterEngine& engine = HvacParameterEngine::instance();
    if (!engine.initialize(nullptr)) {
        HvacErrorHandler::logError("Parameter engine initialization failed");
        return;
    }

    // 3. 加载默认参数(从JSON, 如果存在)
    // 尝试从DLL同目录加载 param_defaults.json
    engine.loadParameters("param_defaults.json"); // 失败也没关系, 使用代码内默认值

    // 4. 弹出参数输入对话框
    HvacParameterSet& params = engine.paramsMutable();
    bool userConfirmed = showParameterDialog(params);

    if (!userConfirmed) {
        HvacErrorHandler::logInfo("User cancelled. Build aborted.");
        engine.shutdown();
        return;
    }

    // 5. 参数校验
    auto failures = params.validateLevel1();
    if (!failures.empty()) {
        std::string errMsg = "Parameter validation failed:\n";
        for (const auto& f : failures) {
            errMsg += "  " + f.first + " -> ";
            if (f.second == ParamStatus::FAIL_OUT_RANGE) errMsg += "OUT OF RANGE\n";
            else errMsg += "CONFLICT\n";
        }
        HvacErrorHandler::logError(errMsg);

        int response = 0;
        UF_UI_message_dialog("HVAC Builder",
                              UF_UI_MESSAGE_WARNING,
                              const_cast<char*>(errMsg.c_str()),
                              1, (char*[]){"OK"}, &response);
        engine.shutdown();
        return;
    }

    // 6. 计算Level-2参数
    params.recalculateLevel2();
    HvacErrorHandler::logInfo("Level-2 parameters calculated successfully.");

    // 7. 创建Undo标记
    int undoMark = engine.createUndoMark("HVAC_ShellBuild");

    // 8. 构建上下文
    BuildContext ctx;
    ctx.workPartTag = workPartTag;
    ctx.status = BuildStatus::IN_PROGRESS;

#if HVAC_USE_NXOPEN_CPP
    ctx.pSession = NXOpen::Session::GetSession();
    ctx.pWorkPart = ctx.pSession->Parts()->Work();
#endif

    // 9. 执行Module-1壳体构建
    HvacShellBuilder shellBuilder;
    ShellBuildResult shellResult = shellBuilder.build(params, ctx);

    // 10. 运行校验
    std::vector<ValidationItem> validations = engine.runValidation();

    // 11. 处理结果
    if (shellResult.status == BuildStatus::SUCCESS) {
        HvacErrorHandler::logInfo("BUILD SUCCESSFUL!");
        engine.deleteUndoMark(undoMark);

        // 导出参数到NX Expression(方便用户在NX里查看/修改)
        params.exportToNxExpressions(workPartTag);
    } else {
        HvacErrorHandler::logError("BUILD FAILED: " + shellResult.errorMessage);
        engine.rollbackToMark(undoMark);
    }

    // 12. 显示结果报告
    showResultReport(shellResult, validations);

    // 13. 清理
    engine.shutdown();
}

/* ============================================================
 * 参数输入对话框 (UF_STYLER简化版)
 * ============================================================
 * Phase-1: 使用连续的UF_UI_ask_double对话框逐一输入关键参数
 * Phase-2: 替换为Block Styler整合界面
 */

static bool showParameterDialog(HvacParameterSet& params)
{
    auto& L1 = params.level1();

    // 确认是否使用默认值
    int response = 0;
    UF_UI_message_dialog("HVAC Box Builder",
                          UF_UI_MESSAGE_QUESTION,
                          "Use default parameters?\n\n"
                          "YES = Build with defaults\n"
                          "NO = Input custom parameters",
                          2, (char*[]){"Use Defaults", "Custom Input"},
                          &response);

    if (response == 1) {
        // 使用默认值, 直接返回
        HvacErrorHandler::logInfo("Using default parameters.");
        return true;
    }

    // 自定义输入: 逐一询问关键参数
    double value = 0.0;
    char title[128];
    int status = 0;

    // 箱体总长
    snprintf(title, sizeof(title), "Box Length X (mm) [%.1f-%.1f]", 350.0, 500.0);
    status = UF_UI_ask_double(title, &L1.boxLengthX);
    if (status == UF_UI_CANCEL) return false;

    // 箱体总宽
    snprintf(title, sizeof(title), "Box Width Y (mm) [%.1f-%.1f]", 280.0, 400.0);
    status = UF_UI_ask_double(title, &L1.boxWidthY);
    if (status == UF_UI_CANCEL) return false;

    // 箱体总高
    snprintf(title, sizeof(title), "Box Height Z (mm) [%.1f-%.1f]", 200.0, 320.0);
    status = UF_UI_ask_double(title, &L1.boxHeightZ);
    if (status == UF_UI_CANCEL) return false;

    // 壁厚
    snprintf(title, sizeof(title), "Wall Thickness (mm) [%.1f-%.1f]", 2.0, 3.5);
    status = UF_UI_ask_double(title, &L1.wallThickness);
    if (status == UF_UI_CANCEL) return false;

    // 加热器倾角
    snprintf(title, sizeof(title), "Heater Tilt Angle (deg) [%.0f-%.0f]", 30.0, 75.0);
    status = UF_UI_ask_double(title, &L1.heaterTiltAngle);
    if (status == UF_UI_CANCEL) return false;

    // 芯体间距
    snprintf(title, sizeof(title), "Core Spacing (mm) [%.0f-%.0f]", 60.0, 120.0);
    status = UF_UI_ask_double(title, &L1.coreSpacing);
    if (status == UF_UI_CANCEL) return false;

    // 分型面高度
    snprintf(title, sizeof(title), "Parting Plane Z (mm) [40%%-60%% of height]");
    status = UF_UI_ask_double(title, &L1.partingPlaneZ);
    if (status == UF_UI_CANCEL) return false;

    HvacErrorHandler::logInfo("Custom parameters input completed.");
    return true;
}

/* ============================================================
 * 结果报告显示
 * ============================================================ */

static void showResultReport(const ShellBuildResult& result,
                             const std::vector<ValidationItem>& validations)
{
    UF_UI_open_listing_window();
    UF_UI_write_listing_window("\n");
    UF_UI_write_listing_window("╔══════════════════════════════════════════╗\n");
    UF_UI_write_listing_window("║    HVAC Box Build Report                 ║\n");
    UF_UI_write_listing_window("╠══════════════════════════════════════════╣\n");

    // 构建状态
    const char* statusStr = "UNKNOWN";
    switch (result.status) {
        case BuildStatus::SUCCESS:     statusStr = "SUCCESS"; break;
        case BuildStatus::FAILED:      statusStr = "FAILED"; break;
        case BuildStatus::ROLLED_BACK: statusStr = "ROLLED BACK"; break;
        default: break;
    }

    char line[256];
    snprintf(line, sizeof(line), "║  Build Status: %-24s ║\n", statusStr);
    UF_UI_write_listing_window(line);

    if (result.status == BuildStatus::FAILED) {
        snprintf(line, sizeof(line), "║  Error: %-31s ║\n", result.errorMessage.c_str());
        UF_UI_write_listing_window(line);
    }

    // 校验结果
    UF_UI_write_listing_window("╠══════════════════════════════════════════╣\n");
    UF_UI_write_listing_window("║  Validation Results:                     ║\n");

    for (const auto& item : validations) {
        const char* resultStr = "???";
        switch (item.result) {
            case ValidationResult::PASS: resultStr = "PASS"; break;
            case ValidationResult::WARN: resultStr = "WARN"; break;
            case ValidationResult::FAIL: resultStr = "FAIL"; break;
        }

        snprintf(line, sizeof(line), "║  [%s] %s: %s\n",
                 resultStr, item.id.c_str(), item.description.c_str());
        UF_UI_write_listing_window(line);

        if (!item.message.empty()) {
            snprintf(line, sizeof(line), "║         %s\n", item.message.c_str());
            UF_UI_write_listing_window(line);
        }
    }

    UF_UI_write_listing_window("╚══════════════════════════════════════════╝\n");
    UF_UI_write_listing_window("\n");

    // 弹出完成提示
    int resp = 0;
    if (result.status == BuildStatus::SUCCESS) {
        UF_UI_message_dialog("HVAC Builder",
                              UF_UI_MESSAGE_INFORMATION,
                              "HVAC box shell built successfully!\n"
                              "Check Listing Window for details.",
                              1, (char*[]){"OK"}, &resp);
    } else {
        UF_UI_message_dialog("HVAC Builder",
                              UF_UI_MESSAGE_ERROR,
                              "Build failed. Check Listing Window for error details.",
                              1, (char*[]){"OK"}, &resp);
    }
}
