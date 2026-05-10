/**
 * @file dynamic_param_registry.h
 * @brief HVAC箱体参数化系统 - 动态参数注册表
 * @details 根据HvacTopologyConfig动态生成参数列表。
 *          不同拓扑配置(桶型/扇形/模式盘等)拥有不同的参数集，
 *          本注册表在运行时根据配置组装完整参数集。
 *
 * 【设计思路】
 * 静态参数(Level-0/1基础参数): 所有配置共用，固定存在
 * 动态参数(策略专属参数): 由选中的策略通过getRequiredParams()注册
 *
 * 例如双温区+桶型风门:
 *   静态: boxLengthX, boxWidthY, wallThickness... (17个固定)
 *   动态Left: barrelRadius_L, barrelArcAngle_L, barrelLength_L... (8个)
 *   动态Right: barrelRadius_R, barrelArcAngle_R, barrelLength_R... (8个)
 *   动态ModeDisc: discOuterRadius, discThickness, defWindowSpan... (9个)
 *   合计: 17 + 8 + 8 + 9 = 42个参数
 *
 * @version 2.0
 * @date 2026-05-10
 */

#ifndef HVAC_DYNAMIC_PARAM_REGISTRY_H
#define HVAC_DYNAMIC_PARAM_REGISTRY_H

#include "hvac_common.h"
#include "hvac_topology_config.h"
#include <map>
#include <vector>
#include <string>

/* ============================================================
 * 参数条目(运行时)
 * ============================================================ */

/**
 * @struct DynamicParamEntry
 * @brief 注册表中的单个参数条目
 */
struct DynamicParamEntry {
    std::string id;             ///< 全局唯一ID (如 "barrel_L_radius")
    std::string displayName;    ///< 对话框显示名 (如 "Left Barrel Radius")
    std::string unit;           ///< 单位
    double minVal = 0.0;
    double maxVal = 0.0;
    double defaultVal = 0.0;
    double currentVal = 0.0;    ///< 当前值(用户输入后更新)
    std::string group;          ///< 分组名(对话框分组显示)
    std::string sourceStrategy; ///< 来源策略名 (如 "BarrelDoor_Left")
    std::string description;
    bool isReadOnly = false;    ///< Level-2参数设为只读
};

/* ============================================================
 * 参数分组
 * ============================================================ */

/**
 * @struct ParamGroup
 * @brief 参数分组(对话框一个折叠面板)
 */
struct ParamGroup {
    std::string groupId;        ///< 分组ID
    std::string displayName;    ///< 显示名称
    int order = 0;              ///< 显示顺序
    std::vector<std::string> paramIds; ///< 本组包含的参数ID列表
};

/* ============================================================
 * 动态参数注册表
 * ============================================================ */

/**
 * @class DynamicParamRegistry
 * @brief 根据拓扑配置动态组装参数集
 *
 * 使用流程:
 * 1. configure(topologyConfig) - 传入拓扑配置
 * 2. 内部自动调用各策略的getRequiredParams()收集参数
 * 3. 为多温区参数自动添加Left/Right/Rear后缀
 * 4. getParamList() / getParamGroups() - 获取完整参数集
 * 5. setParamValue() / getParamValue() - 用户输入/读取
 */
class DynamicParamRegistry {
public:
    DynamicParamRegistry() = default;
    ~DynamicParamRegistry() = default;

    /**
     * @brief 根据拓扑配置初始化注册表
     * @param config 拓扑配置
     * @details 清空旧数据，重新收集所有策略所需参数，
     *          为多温区自动生成带后缀的参数副本。
     */
    void configure(const HvacTopologyConfig& config);

    /**
     * @brief 获取当前配置的拓扑
     */
    const HvacTopologyConfig& getTopology() const { return m_config; }

    /* ---- 参数查询 ---- */

    /** 获取所有已注册参数(按组排序) */
    std::vector<DynamicParamEntry> getAllParams() const;

    /** 获取指定分组的参数 */
    std::vector<DynamicParamEntry> getParamsByGroup(const std::string& groupId) const;

    /** 获取所有分组定义 */
    std::vector<ParamGroup> getParamGroups() const;

    /** 获取单个参数条目(按ID) */
    const DynamicParamEntry* getParam(const std::string& paramId) const;

    /** 参数是否存在 */
    bool hasParam(const std::string& paramId) const;

    /** 获取参数总数 */
    int getParamCount() const { return static_cast<int>(m_params.size()); }

    /* ---- 参数值操作 ---- */

    /**
     * @brief 设置参数值(带范围校验)
     * @return VALID/WARN_NEAR_LIMIT/FAIL_OUT_RANGE
     */
    ParamStatus setParamValue(const std::string& paramId, double value);

    /** 获取参数当前值 */
    double getParamValue(const std::string& paramId) const;

    /** 重置所有参数为默认值 */
    void resetToDefaults();

    /** 将所有参数导出为map(供策略build()使用) */
    std::map<std::string, double> exportAsMap() const;

    /**
     * @brief 导出指定策略的参数(去除前缀后缀)
     * @param strategyPrefix 策略前缀 (如 "barrel_L_")
     * @return 去前缀后的参数map (如 "barrelRadius"=65.0)
     */
    std::map<std::string, double> exportForStrategy(const std::string& strategyPrefix) const;

    /* ---- 序列化 ---- */

    /** 导出为JSON格式字符串 */
    std::string toJson() const;

    /** 从JSON字符串加载值 */
    bool fromJson(const std::string& jsonStr);

private:
    HvacTopologyConfig m_config;
    std::map<std::string, DynamicParamEntry> m_params;  ///< ID→条目
    std::vector<ParamGroup> m_groups;

    /* ---- 内部构建方法 ---- */

    /** 注册固定基础参数(Level-1静态参数，所有配置共用) */
    void registerBaseParams();

    /** 注册温度风门参数(根据类型和温区数) */
    void registerTempDoorParams();

    /** 注册模式门参数 */
    void registerModeDoorParams();

    /** 注册加热器相关参数 */
    void registerHeaterParams();

    /** 注册新能源相关参数(如果配置了PTC/热泵) */
    void registerEnergyParams();

    /** 添加单个参数条目 */
    void addParam(const std::string& id, const std::string& displayName,
                  const std::string& unit, double minVal, double maxVal,
                  double defaultVal, const std::string& group,
                  const std::string& source = "",
                  const std::string& desc = "");

    /** 添加一个分组 */
    void addGroup(const std::string& id, const std::string& name, int order);

    /** 温区后缀列表 */
    std::vector<std::string> getZoneSuffixes() const;

    /** 温区显示名 */
    std::string getZoneDisplayName(const std::string& suffix) const;
};

#endif /* HVAC_DYNAMIC_PARAM_REGISTRY_H */
