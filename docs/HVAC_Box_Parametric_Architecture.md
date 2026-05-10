# 汽车空调箱体全参数化NX二次开发 —— 整体架构设计

## 文档版本

| 版本 | 日期 | 说明 |
|------|------|------|
| V1.0 | 2026-05-10 | 初版架构设计 |

## 一、设计哲学与技术背景

### 1.1 正向设计体系定位

本架构严格遵循国际Tier1正向研发流程（Valeo TDS/Denso DMS/Hanon HPDS），
核心理念：**参数驱动几何 → 几何约束运动 → 运动校验合规 → 合规反馈参数**。

拒绝逆向仿制路径（扫描→逆向→修面→拟合），采用纯正向参数化构建：
- 基于SAE J639（车用空调系统性能试验标准）定义性能边界
- 基于ISO 8708 / DIN 1946-3 定义舒适性指标
- 基于各OEM整车安装硬点Package数据定义空间约束
- 基于模具DFM（Design for Manufacturability）定义壁厚/拔模/分型面规则

### 1.2 为什么选择NX Open参数化二次开发

| 维度 | 传统手工建模 | NX Open参数化 |
|------|------------|---------------|
| 设计变更响应 | 2-5天 | < 30分钟全模型联动 |
| 风门联动校验 | 手动逐一检查 | 自动运动仿真+干涉检测 |
| 合规性验证 | 依赖经验 | 规则引擎自动校验 |
| 多车型平台化 | 重新建模 | 参数切换即生成 |

---

## 二、核心参数体系层级定义

### 2.1 参数层级总览

```
┌─────────────────────────────────────────────────────────────────┐
│                    Level-0: 整车输入参数（外部约束）                │
│  来源：OEM Package数据、性能目标、法规要求                          │
├─────────────────────────────────────────────────────────────────┤
│                    Level-1: 驱动参数（Design Driver）              │
│  定义箱体核心几何骨架，由设计师主动设定                              │
├─────────────────────────────────────────────────────────────────┤
│                    Level-2: 关联参数（Derived/Linked）             │
│  由Level-1通过映射公式/规则自动计算得出                             │
├─────────────────────────────────────────────────────────────────┤
│                    Level-3: 校验参数（Validation）                 │
│  用于合规性/工艺性/运动学检查，反馈修正Level-1                      │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Level-0：整车输入参数

| 参数编号 | 参数名称 | 单位 | 典型范围 | 来源 |
|---------|---------|------|---------|------|
| L0-001 | 仪表板下方可用空间包络 XYZ | mm | X:350-500, Y:280-400, Z:200-320 | OEM Package |
| L0-002 | 蒸发器芯体尺寸 (W×H×D) | mm | 宽200-280, 高180-240, 深38-58 | 热系统规格书 |
| L0-003 | 加热器芯体尺寸 (W×H×D) | mm | 宽180-250, 高150-200, 深26-42 | 热系统规格书 |
| L0-004 | 鼓风机蜗壳外径 | mm | Φ140-Φ180 | 鼓风机选型 |
| L0-005 | 制冷量目标 | kW | 3.5-7.0 | SAE J639性能目标 |
| L0-006 | 制热量目标 | kW | 4.0-8.0 | 性能目标 |
| L0-007 | 最大风量 | m³/h | 400-600 | 性能目标 |
| L0-008 | 安装硬点坐标系原点 | mm | OEM定义 | 整车坐标系 |
| L0-009 | 出风口数量及朝向 | - | Face/Foot/Def/Rear | 平台定义 |
| L0-010 | 目标车型左右驾定义 | - | LHD/RHD | 平台定义 |

### 2.3 Level-1：驱动参数（Design Driver）

**划分规则**：直接决定箱体几何骨架形态、可由设计师在合理范围内自主设定、
变更后触发全模型联动重建的参数。

| 参数编号 | 参数名称 | 单位 | 约束范围 | 设计依据 |
|---------|---------|------|---------|---------|
| L1-001 | 箱体总长度 (X方向) | mm | 350-500 | L0-001约束 |
| L1-002 | 箱体总宽度 (Y方向) | mm | 280-400 | L0-001约束 |
| L1-003 | 箱体总高度 (Z方向) | mm | 200-320 | L0-001约束 |
| L1-004 | 蒸发器安装倾角 | deg | 0-15 | 冷凝水排放+风阻优化 |
| L1-005 | 加热器安装倾角 | deg | 30-75 | 温度混合效率 |
| L1-006 | 蒸发器至加热器中心距 | mm | 60-120 | 混合风道长度 |
| L1-007 | 主分型面Z坐标 | mm | 箱体高度40%-60%处 | 模具开模方向 |
| L1-008 | 壳体公称壁厚 | mm | 2.0-3.5 | PP+TD20材料/注塑工艺 |
| L1-009 | 密封槽宽度 | mm | 3.0-5.0 | 密封条截面匹配 |
| L1-010 | 风门轴线至箱体侧壁距离 | mm | 15-35 | 连杆空间+密封接触 |
| L1-011 | DEF出风口中心高度 | mm | 箱体顶面下30-80mm | 除霜性能 |
| L1-012 | FACE出风口中心高度 | mm | 箱体中部 | 吹面舒适性 |
| L1-013 | FOOT出风口中心高度 | mm | 箱体底部上20-60mm | 暖足性能 |
| L1-014 | 温度风门旋转中心X坐标 | mm | 蒸发器与加热器之间 | 混合比控制 |
| L1-015 | 温度风门旋转中心Z坐标 | mm | 加热器芯体上沿附近 | 气流分配 |
| L1-016 | 模式风门旋转中心坐标(X,Z) | mm | 出风通道汇合区 | 模式切换 |
| L1-017 | 执行器安装面Y坐标偏移 | mm | 箱体侧壁外5-25mm | 执行器选型 |

### 2.4 Level-2：关联参数（Derived/Linked）

**划分规则**：由Level-1参数通过确定性公式/查表/规则引擎自动计算，
设计师不直接编辑，但可查看验证。

| 参数编号 | 参数名称 | 计算规则 | 依赖参数 |
|---------|---------|---------|---------|
| L2-001 | 蒸发器腔室内廓尺寸 | L0-002 + 2×(间隙1.5-3.0mm) | L0-002, L1-008 |
| L2-002 | 加热器腔室内廓尺寸 | L0-003 + 2×(间隙1.5-3.0mm) | L0-003, L1-008 |
| L2-003 | 温度风门有效弧长 | π×R_door×(sweep_angle/360) | L1-014, L1-015, 加热器高度 |
| L2-004 | 温度风门轴长度 | 箱体内宽 - 2×轴承座宽 | L1-002, 轴承座参数 |
| L2-005 | 模式风门连杆铰接点坐标 | 基于四连杆机构正运动学计算 | L1-016, 执行器行程 |
| L2-006 | 密封面总长度 | 分型面周长 + 功能口周长 | L1-001~003, L1-007 |
| L2-007 | 卡扣分布坐标 | 密封面等分+应力集中点加密 | L2-006, 卡扣间距规则(60-100mm) |
| L2-008 | 拔模方向向量 | 基于分型面法线+开模方向 | L1-007, 模具结构 |
| L2-009 | 加强筋高度/间距 | 壁厚×(0.5~0.7) / 间距30-50mm | L1-008 |
| L2-010 | 执行器连杆长度 | 四连杆逆运动学求解 | L1-016, L1-017, 执行器旋转角 |

### 2.5 Level-3：校验参数（Validation）

**划分规则**：不参与几何构建，仅用于验证设计合规性。
校验失败时反馈警告，指导设计师修正Level-1参数。

| 参数编号 | 校验项 | 合格判据 | 校验方法 |
|---------|--------|---------|---------|
| L3-001 | 风门全行程干涉检查 | 间隙 ≥ 0.5mm (全角度) | 运动仿真+距离分析 |
| L3-002 | 壁厚均匀性 | 最薄处/公称壁厚 ≥ 0.7 | 截面分析 |
| L3-003 | 拔模角度合规 | 所有面拔模角 ≥ 1.5° (外观面≥3°) | NX拔模分析 |
| L3-004 | 密封面平面度 | ≤ 0.3mm/100mm | 基准面偏差分析 |
| L3-005 | 卡扣受力均匀性 | 单扣拉脱力偏差 ≤ 15% | FEA简化计算 |
| L3-006 | 出风面积比校验 | 各出风口面积/蒸发器迎风面积在规范范围 | 截面面积测量 |
| L3-007 | 执行器力矩校验 | 风门操作力矩 ≤ 执行器额定×0.7 | 力矩计算 |
| L3-008 | 冷凝水排放坡度 | 蒸发器底部坡度 ≥ 3° | 几何角度测量 |
| L3-009 | 整车安装硬点偏差 | 安装点坐标偏差 ≤ ±0.5mm | 坐标对比 |
| L3-010 | 模具可行性(无倒扣) | 分型面无死角区域 | 拔模方向分析 |

---


## 三、模块拆分逻辑与依赖关系

### 3.1 七大模块总览

```
┌────────────────────────────────────────────────────────────────────────────┐
│                        HVAC Box Parametric System                          │
├────────────┬────────────┬────────────┬────────────┬──────────┬────────────┤
│  Module-1  │  Module-2  │  Module-3  │  Module-4  │ Module-5 │  Module-6  │
│  箱体壳体   │  风道系统   │  安装接口   │  密封结构   │ 运动机构  │  合规校验   │
│  Shell     │  Duct      │  Interface │  Seal      │ Motion   │  Validate  │
├────────────┴────────────┴────────────┴────────────┴──────────┴────────────┤
│                            Module-7: 自动出图                               │
│                            AutoDrawing                                      │
└────────────────────────────────────────────────────────────────────────────┘
```

### 3.2 各模块职责与接口

#### Module-1：箱体壳体模块 (HvacShellBuilder)

**职责**：
- 基于Level-1驱动参数构建箱体外壳基础几何体
- 实现上下壳体分型（主分型面生成）
- 壁厚偏置、加强筋阵列、安装凸台
- 拔模角自动施加
- 材料属性绑定（PP+TD20/PA6+GF30等）

**输入接口**：
- L1-001~003（总体尺寸）
- L1-007（主分型面位置）
- L1-008（壁厚）
- L0-002/003（芯体尺寸，定义内腔）

**输出接口**：
- 上壳体Body / 下壳体Body
- 分型面Sheet
- 内腔参考曲面（供Module-2使用）
- 安装凸台坐标（供Module-3使用）

#### Module-2：风道系统模块 (AirDuctBuilder)

**职责**：
- 蒸发器腔→混合腔→各出风口的风道路径定义
- 基于CFD简化准则的风道截面渐变控制
- 导流板/导流片参数化生成
- 出风口截面形状定义

**输入接口**：
- Module-1内腔参考曲面
- L1-004/005（芯体倾角）
- L1-006（芯体间距）
- L1-011~013（出风口位置）
- L0-007（风量目标→截面积计算）

**输出接口**：
- 风道内壁曲面（与壳体布尔运算）
- 出风口截面轮廓（供Module-3定义法兰接口）
- 风道中心线（供Module-5定义风门位置）

#### Module-3：安装接口模块 (InterfaceBuilder)

**职责**：
- 蒸发器/加热器芯体安装导轨与定位特征
- 鼓风机蜗壳安装法兰
- 出风口连接法兰（对接风管）
- 整车安装支架/硬点接口
- 执行器安装座
- 线束过孔/传感器安装孔

**输入接口**：
- L0-002~004（芯体/鼓风机尺寸）
- L0-008（整车安装硬点）
- L1-017（执行器安装面偏移）
- Module-1壳体外表面

**输出接口**：
- 各安装特征的精确坐标与法兰面
- 紧固件规格表（螺钉M5/M6/卡扣类型）
- 安装公差带定义

#### Module-4：密封结构模块 (SealBuilder)

**职责**：
- 上下壳体合箱密封槽截面定义
- 芯体周围密封结构（泡棉/橡胶条）
- 风门端部密封面（叶片与壳壁的接触线）
- 密封压缩量计算与验证

**输入接口**：
- L1-009（密封槽宽度）
- Module-1分型面
- Module-5风门端部轮廓

**输出接口**：
- 密封槽特征（扫掠体）
- 密封条规格选型建议
- 密封面连续性报告

#### Module-5：运动机构联动模块 (MotionLinkageBuilder) ★核心模块

**职责**：
- 温度风门（Temperature Door）参数化：旋转轴、叶片弧面、端部密封
- 模式风门（Mode Door）参数化：DEF/FACE/FOOT各模式切换
- 内外循环风门（Intake Door）参数化
- 四连杆/曲柄滑块机构的运动学正逆解
- 连杆铰接点坐标自动计算
- 执行器输出轴与风门轴的传动比匹配
- 全行程运动仿真与干涉检测

**输入接口**：
- L1-010（风门轴线至侧壁距离）
- L1-014~016（各风门旋转中心坐标）
- L1-017（执行器安装面）
- Module-2风道中心线（定义风门应处位置）
- 执行器规格（行程角度、输出力矩）

**输出接口**：
- 风门体Body（参与壳体装配）
- 连杆机构Body
- 运动包络面（供Module-6干涉校验）
- 各位置角度-力矩曲线数据

#### Module-6：合规性校验模块 (ValidationEngine)

**职责**：
- 执行Level-3全部校验规则
- 输出合规性报告（PASS/WARN/FAIL）
- FAIL项自动定位问题参数并给出修正建议
- DFM（模具可行性）专项检查
- 性能预估（基于简化经验公式）

**输入接口**：
- 所有模块生成的几何体
- Level-0/1/2全部参数当前值
- Level-3校验规则库

**输出接口**：
- 校验报告（JSON/HTML）
- 问题特征高亮显示
- 参数修正建议值

#### Module-7：自动出图模块 (AutoDraftingBuilder)

**职责**：
- 基于NX Drafting自动生成2D工程图
- 标准三视图+关键截面视图自动布局
- GD&T标注自动施加（基于规则库）
- BOM表/技术要求自动填写
- 图框/标题栏自动匹配企业标准

**输入接口**：
- 全部3D模型
- 尺寸标注规则库
- 企业图框模板

**输出接口**：
- 符合GB/ISO标准的工程图.prt
- PDF导出

### 3.3 模块依赖关系图

```
Level-0/1 参数输入
       │
       ▼
 ┌─────────────┐
 │  Module-1   │ 箱体壳体（基础几何骨架）
 │  Shell      │
 └──────┬──────┘
        │ 内腔曲面、分型面、壳体外表面
        ├──────────────────────────────────┐
        ▼                                  ▼
 ┌─────────────┐                    ┌─────────────┐
 │  Module-2   │ 风道系统            │  Module-3   │ 安装接口
 │  Duct       │                    │  Interface  │
 └──────┬──────┘                    └──────┬──────┘
        │ 风道中心线、出风口截面              │ 执行器安装座
        ▼                                  │
 ┌─────────────┐                           │
 │  Module-5   │ 运动机构联动  ◄────────────┘
 │  Motion     │
 └──────┬──────┘
        │ 风门端部轮廓、运动包络
        ▼
 ┌─────────────┐
 │  Module-4   │ 密封结构
 │  Seal       │
 └──────┬──────┘
        │ 所有几何体完成
        ▼
 ┌─────────────┐
 │  Module-6   │ 合规性校验
 │  Validate   │
 └──────┬──────┘
        │ 校验通过
        ▼
 ┌─────────────┐
 │  Module-7   │ 自动出图
 │  Drawing    │
 └─────────────┘
```

### 3.4 模块间数据传递机制

采用**NX Expression + Part Attribute + Interpart Reference**三级数据通道：

| 通道类型 | 适用场景 | NX Open API |
|---------|---------|-------------|
| Expression | 标量参数传递(尺寸/角度) | UF_MODL_create_exp / NXOpen::Expression | 
| Part Attribute | 元数据/状态标记 | UF_ATTR_assign / NXOpen::NXObject::SetUserAttribute |
| Interpart Reference (WAVE) | 几何体跨Part引用 | UF_WAVE_create_linked_body / NXOpen::Features::WaveLink |

---


## 四、全参数联动规则

### 4.1 联动触发机制

```
设计师修改 Level-1 参数
       │
       ▼
┌──────────────────┐
│  参数变更事件监听  │  NXOpen::Expression::ValueChanged callback
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  依赖关系图遍历   │  拓扑排序确定重建顺序
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Level-2参数重算  │  映射公式引擎执行
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  几何体重建       │  各Module按依赖顺序Update
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Level-3校验执行  │  Validation Engine全量检查
└────────┬─────────┘
         │
    ┌────┴────┐
    ▼         ▼
 [PASS]    [FAIL]
    │         │
    ▼         ▼
  完成      报告问题参数+建议修正值
```

### 4.2 箱体核心参数与风门运动机构的映射逻辑

#### 4.2.1 温度风门（Temp Door）联动映射

当Level-1箱体参数变更时，温度风门必须自动适配：

```
┌─────────────────────────────────────────────────────────────────────┐
│ 映射规则 T-01: 温度风门旋转半径                                        │
│                                                                     │
│ R_temp_door = f(L1-006, L1-005, L0-003_H)                          │
│                                                                     │
│ 具体公式:                                                            │
│ R = (Heater_H / 2) / cos(L1-005 × π/180) + clearance(2mm)         │
│                                                                     │
│ 含义: 风门弧面半径必须覆盖加热器芯体迎风面，                             │
│       考虑加热器倾角后的投影高度                                        │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│ 映射规则 T-02: 温度风门摆动角度范围                                     │
│                                                                     │
│ θ_sweep = arctan(Heater_H × sin(L1-005) / L1-006) + margin(5°)    │
│                                                                     │
│ 约束: 45° ≤ θ_sweep ≤ 90° (超出范围触发L3校验FAIL)                   │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│ 映射规则 T-03: 温度风门轴坐标自动跟随                                   │
│                                                                     │
│ X_axis = L1-014 (直接驱动)                                           │
│ Z_axis = L1-015 (直接驱动)                                           │
│ Y_axis = L1-010 (轴端距侧壁)                                        │
│                                                                     │
│ 当L1-001(箱体总长)变化:                                               │
│   → L1-014按比例缩放: L1-014_new = L1-014_old × (L1-001_new/L1-001_old)│
│   → 触发R_temp_door重算                                              │
│   → 触发连杆铰接点重算                                                │
└─────────────────────────────────────────────────────────────────────┘
```

#### 4.2.2 模式风门（Mode Door）联动映射

```
┌─────────────────────────────────────────────────────────────────────┐
│ 映射规则 M-01: 模式风门位置跟随出风口                                   │
│                                                                     │
│ 模式风门旋转中心 = 出风通道汇合点质心                                    │
│ X_mode = (X_def + X_face + X_foot) / 3 + offset                    │
│ Z_mode = weighted_average(L1-011, L1-012, L1-013)                  │
│                                                                     │
│ 当任一出风口位置变更 → 模式风门中心自动重定位                             │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│ 映射规则 M-02: 模式切换连杆机构自适应                                   │
│                                                                     │
│ 四连杆参数求解:                                                       │
│   输入: 执行器旋转角(0°~90°), 风门需求行程角                           │
│   约束: Grashof条件(最短杆+最长杆 < 其余两杆之和)                      │
│   求解: 连杆L1, L2, L3, L4长度 + 铰接点坐标                          │
│                                                                     │
│ 当L1-016变化:                                                        │
│   → 重新求解四连杆几何                                                │
│   → 更新铰接点安装凸台位置                                            │
│   → 更新执行器安装座相对偏移                                           │
└─────────────────────────────────────────────────────────────────────┘
```

#### 4.2.3 连杆铰接点坐标自动计算

```
┌─────────────────────────────────────────────────────────────────────┐
│ 映射规则 LK-01: 铰接点坐标计算                                         │
│                                                                     │
│ 已知:                                                                │
│   P0 = 执行器输出轴中心 (由L1-017 + 执行器规格确定)                    │
│   P3 = 风门轴中心 (由L1-014~016确定)                                  │
│   θ_in = 执行器旋转角范围 (执行器规格, 典型0°~90°)                     │
│   θ_out = 风门需求摆动角 (由T-02计算)                                 │
│   传动比 i = θ_out / θ_in                                           │
│                                                                     │
│ 求解:                                                                │
│   P1 = P0 + L_crank × [cos(α0), sin(α0)]  (曲柄端点)               │
│   P2 = P3 + L_rocker × [cos(β0), sin(β0)] (摇杆端点)               │
│   |P1-P2| = L_coupler (连杆长度约束)                                  │
│                                                                     │
│ 采用Newton-Raphson迭代求解满足Grashof条件的最优解                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 4.3 联动容错机制

#### 4.3.1 参数越界保护

```cpp
// 参数变更前的范围校验
enum class ParamStatus {
    VALID,          // 参数在合理范围内
    WARN_NEAR_LIMIT,// 接近边界(90%~100%范围)，黄色警告
    FAIL_OUT_RANGE, // 超出硬限制，拒绝赋值并回退
    FAIL_CONFLICT   // 与其他参数冲突(几何不可实现)
};
```

#### 4.3.2 几何重建失败回滚

```
重建流程:
1. 保存当前模型状态快照 (NXOpen::Session::UndoMark)
2. 执行参数变更
3. 触发模型Update
4. 检查Update状态:
   - 成功 → 执行Level-3校验
   - 失败 → 回滚到快照，报告失败原因
5. Level-3校验:
   - 全部PASS → 确认变更
   - 存在FAIL → 保留几何但标记警告，建议修正
```

#### 4.3.3 运动学死点规避

```
四连杆求解容错:
1. 主求解: Newton-Raphson (初值由经验公式给出)
2. 若不收敛: 切换到遗传算法全局搜索
3. 若无解: 报告"当前参数组合下四连杆不可实现"
           给出最接近可行解及需调整的参数
4. 死点检查: 在全行程离散点检查传动角μ
           μ < 40° 报警 (接近死点，力传递效率低)
```

### 4.4 参数联动矩阵（关键映射汇总）

| 变更参数 | 影响范围 | 联动动作 |
|---------|---------|---------|
| L1-001 箱体总长 | 风道长度、风门位置、安装点 | 比例缩放X方向所有特征 |
| L1-002 箱体总宽 | 风门轴长度、密封面宽度 | 重算轴长+密封路径 |
| L1-003 箱体总高 | 出风口位置、分型面 | 按比例调整Z坐标 |
| L1-005 加热器倾角 | 温度风门半径、混合腔形状 | 重算T-01/T-02 |
| L1-006 芯体间距 | 混合风道、温度风门位置 | 重算风道截面+风门坐标 |
| L1-014/015 温度风门中心 | 连杆机构全部参数 | 重新求解四连杆 |
| L1-016 模式风门中心 | 模式连杆、出风分配 | 重新求解+出风面积校验 |
| L1-017 执行器安装偏移 | 连杆长度、铰接点 | 重新求解传动比 |

---

## 五、NX版本适配与编译环境

### 5.1 目标NX版本矩阵

| NX版本 | 内部版本号 | API层级 | 适配策略 |
|--------|----------|---------|---------|
| NX 12.0 | V12.0.0 | NXOpen C++ / UF (legacy) | 基线兼容版本，UF为主 |
| NX 1980 | V1980 (相当于NX15) | NXOpen C++ | 过渡版本，NXOpen为主+UF补充 |
| NX 2206 | V2206 (相当于NX17) | NXOpen C++ (目标版本) | 全功能版本，充分利用新API |

### 5.2 版本兼容策略

```cpp
// 版本适配宏定义
#ifndef HVAC_NX_VERSION_H
#define HVAC_NX_VERSION_H

// 编译时版本检测
#if NX_VERSION_NUMBER >= 2206000
    #define HVAC_USE_JOURNALIDENTIFIER 1
    #define HVAC_USE_MODERN_EXPRESSION 1
    #define HVAC_USE_NXOPEN_MOTION 1
#elif NX_VERSION_NUMBER >= 1980000
    #define HVAC_USE_JOURNALIDENTIFIER 1
    #define HVAC_USE_MODERN_EXPRESSION 1
    #define HVAC_USE_NXOPEN_MOTION 0  // 1980的Motion API有限制
#else // NX12
    #define HVAC_USE_JOURNALIDENTIFIER 0
    #define HVAC_USE_MODERN_EXPRESSION 0
    #define HVAC_USE_NXOPEN_MOTION 0
#endif

#endif // HVAC_NX_VERSION_H
```

### 5.3 关键API版本兼容对照

| 功能 | NX12 API | NX1980 API | NX2206 API |
|------|----------|-----------|------------|
| 创建表达式 | UF_MODL_create_exp() | NXOpen::ExpressionCollection::CreateExpression() | 同1980，增强错误信息 |
| 创建拉伸体 | UF_MODL_create_extrusion() | NXOpen::Features::ExtrudeBuilder | 同1980 |
| 创建旋转体 | UF_MODL_create_revolved() | NXOpen::Features::RevolveBuilder | 同1980 |
| 布尔运算 | UF_MODL_boolean() | NXOpen::Features::BooleanBuilder | 同1980 |
| 曲面偏置 | UF_MODL_create_offset_surface() | NXOpen::Features::OffsetSurfaceBuilder | 同1980 |
| WAVE链接 | UF_WAVE_create_linked_body() | NXOpen::Features::WaveLinkBuilder | 同1980 |
| 装配约束 | UF_ASSEM_add_constraint() | NXOpen::Positioning::ComponentConstraint | 增强约束类型 |
| 运动仿真 | UF_MOTION_* (有限) | NXOpen::Motion (部分) | NXOpen::Motion (完整) |
| 拔模分析 | UF_MODL_ask_draft_analysis() | NXOpen::GeometricAnalysis::DraftAnalysis | 同1980 |
| 工程图创建 | UF_DRAW_* | NXOpen::Drawings::DrawingSheet | 同1980 |

### 5.4 编译环境配置

#### 5.4.1 必需环境变量

```bash
# Windows环境 (主开发环境)
set UGII_BASE_DIR=C:\Siemens\NX2206
set UGII_ROOT_DIR=%UGII_BASE_DIR%\NXBIN
set UGOPEN_DIR=%UGII_BASE_DIR%\UGOPEN

# 编译器: Visual Studio 2019 (NX2206推荐) / VS2015 (NX12兼容)
# 注意: NX2206严格要求VS2019 v16.x，不兼容VS2022
```

#### 5.4.2 CMakeLists.txt 核心配置

```cmake
cmake_minimum_required(VERSION 3.16)
project(HvacBoxParametric VERSION 1.0.0 LANGUAGES CXX)

# C++标准: NX2206支持C++17, NX12需要C++11
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# NX环境检测
if(NOT DEFINED ENV{UGII_BASE_DIR})
    message(FATAL_ERROR "UGII_BASE_DIR not set. Source NX environment first.")
endif()

set(NX_BASE_DIR $ENV{UGII_BASE_DIR})
set(NX_UGOPEN_DIR "${NX_BASE_DIR}/UGOPEN")

# 头文件路径
include_directories(
    ${NX_UGOPEN_DIR}
    ${NX_UGOPEN_DIR}/NXOpen
    ${NX_UGOPEN_DIR}/NXOpen/Features
    ${NX_UGOPEN_DIR}/uf_defs
    ${PROJECT_SOURCE_DIR}/include
)

# NX库路径
link_directories(
    ${NX_BASE_DIR}/NXBIN
    ${NX_BASE_DIR}/NXBIN/managed
)

# 核心NX链接库
set(NX_LIBS
    libufun      # UF legacy API
    libnxopencpp # NXOpen C++ API
    libugopenint # NX Open internal
    libvmathpp   # 向量数学库
)

# 模块源文件
set(HVAC_SOURCES
    src/main/hvac_entry_point.cpp
    src/core/parameter_engine.cpp
    src/core/dependency_graph.cpp
    src/modules/shell_builder.cpp
    src/modules/duct_builder.cpp
    src/modules/interface_builder.cpp
    src/modules/seal_builder.cpp
    src/modules/motion_linkage_builder.cpp
    src/modules/validation_engine.cpp
    src/modules/auto_drafting_builder.cpp
    src/utils/nx_version_compat.cpp
    src/utils/error_handler.cpp
    src/utils/math_solver.cpp
)

# 生成动态库 (NX插件为.dll/.so)
add_library(HvacBoxParametric SHARED ${HVAC_SOURCES})
target_link_libraries(HvacBoxParametric ${NX_LIBS})

# NX要求的导出宏
target_compile_definitions(HvacBoxParametric PRIVATE
    WNT          # Windows平台标记
    _UNICODE
    UNICODE
)

# 输出到NX可加载路径
set_target_properties(HvacBoxParametric PROPERTIES
    OUTPUT_NAME "hvac_box_parametric"
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
```

### 5.5 工程化编码规范

#### 5.5.1 文件组织结构

```
HvacBoxParametric/
├── CMakeLists.txt
├── include/
│   ├── hvac_common.h              // 全局类型定义、枚举、常量
│   ├── hvac_parameters.h          // 参数定义与管理
│   ├── hvac_nx_version.h          // 版本兼容宏
│   ├── core/
│   │   ├── parameter_engine.h     // 参数引擎接口
│   │   └── dependency_graph.h     // 依赖图管理
│   ├── modules/
│   │   ├── shell_builder.h
│   │   ├── duct_builder.h
│   │   ├── interface_builder.h
│   │   ├── seal_builder.h
│   │   ├── motion_linkage_builder.h
│   │   ├── validation_engine.h
│   │   └── auto_drafting_builder.h
│   └── utils/
│       ├── error_handler.h
│       └── math_solver.h
├── src/
│   ├── main/
│   │   └── hvac_entry_point.cpp   // NX入口点 (ufusr/ufsta)
│   ├── core/
│   ├── modules/
│   └── utils/
├── config/
│   ├── param_defaults.json        // 默认参数值
│   ├── validation_rules.json      // 校验规则库
│   └── material_library.json      // 材料数据库
├── templates/
│   ├── drawing_frame_A3.prt       // 图框模板
│   └── seal_profiles/             // 密封条截面库
└── docs/
    └── HVAC_Box_Parametric_Architecture.md  // 本文档
```

#### 5.5.2 命名规范

```cpp
// 类名: PascalCase，模块前缀
class HvacShellBuilder;
class HvacMotionLinkageBuilder;
class HvacParameterEngine;

// 成员函数: camelCase
void buildUpperShell();
void calculateLinkageKinematics();

// 成员变量: m_ 前缀 + camelCase
double m_boxTotalLength;
NXOpen::Body* m_pUpperShellBody;

// 常量: k前缀 + PascalCase
const double kMinWallThickness = 2.0;
const double kMaxDraftAngle = 5.0;

// 枚举: 全大写下划线
enum class DoorType {
    TEMP_DOOR,
    MODE_DOOR_DEF,
    MODE_DOOR_FACE,
    MODE_DOOR_FOOT,
    INTAKE_DOOR
};

// NX对象指针: p前缀，使用后必须判空
NXOpen::Part* pWorkPart = nullptr;
NXOpen::Features::Feature* pExtrudeFeature = nullptr;
```

#### 5.5.3 异常处理规范

```cpp
// 所有NX API调用必须包裹在try-catch中
// 使用统一的错误处理器记录日志

try {
    NXOpen::Features::Feature* feature = builder->CommitFeature();
    if (feature == nullptr) {
        throw HvacBuildException("Feature creation returned null",
                                  HvacErrorCode::FEATURE_CREATE_FAILED);
    }
} catch (const NXOpen::NXException& nxEx) {
    HvacErrorHandler::LogError(nxEx.GetMessage(), __FILE__, __LINE__);
    // 回滚到UndoMark
    session->UndoToMark(undoMark, nullptr);
    throw; // 重新抛出供上层处理
}
```

#### 5.5.4 内存管理规范

```cpp
// NX Builder模式: 必须Destroy未Commit的Builder
NXOpen::Features::ExtrudeBuilder* extrudeBuilder = nullptr;
try {
    extrudeBuilder = workPart->Features()->CreateExtrudeBuilder(nullptr);
    // ... 设置参数 ...
    NXOpen::NXObject* committed = extrudeBuilder->Commit();
    extrudeBuilder->Destroy();  // Commit后Destroy
    extrudeBuilder = nullptr;
} catch (...) {
    if (extrudeBuilder != nullptr) {
        extrudeBuilder->Destroy();  // 异常时也必须Destroy
        extrudeBuilder = nullptr;
    }
    throw;
}

// UF API: tag_t资源不需要手动释放，但数组需要UF_free
tag_t* bodyArray = nullptr;
int bodyCount = 0;
UF_MODL_ask_feat_body(featTag, &bodyCount, &bodyArray);
// 使用bodyArray...
UF_free(bodyArray);  // 必须释放
```

---

## 六、编译步骤与调试方法

### 6.1 编译步骤

```bash
# 1. 配置NX环境
source /opt/Siemens/NX2206/UGII/ugii_env.sh  # Linux
# 或 Windows: 运行NX Command Prompt

# 2. CMake配置
mkdir build && cd build
cmake .. -G "Visual Studio 16 2019" -A x64 \
    -DCMAKE_BUILD_TYPE=Release

# 3. 编译
cmake --build . --config Release

# 4. 部署到NX
copy bin\hvac_box_parametric.dll %UGII_BASE_DIR%\application\
```

### 6.2 NX内加载方式

```
方式1: File → Execute → NX Open → 选择dll
方式2: 注册到菜单 (通过.men文件 + application文件夹)
方式3: startup自动加载 (放入startup文件夹)
```

### 6.3 调试方法

```
1. Attach到ugraf.exe进程 (VS → Debug → Attach to Process)
2. NX Syslog日志: %UGII_TMP_DIR%\syslog.txt
3. 自定义日志: 写入 %APPDATA%\hvac_parametric\debug.log
4. NX Journal回放: 录制操作对比API调用序列
```

### 6.4 NX二次开发专属坑位规避

| 坑位编号 | 问题描述 | 规避方案 |
|---------|---------|---------|
| PIT-001 | Builder.Commit()后不Destroy导致内存泄漏 | RAII封装，见上文内存管理规范 |
| PIT-002 | Expression名称含特殊字符导致崩溃 | 仅允许 [a-zA-Z0-9_] |
| PIT-003 | UF_MODL_update()不调用导致几何不刷新 | 每次批量修改后调用一次 |
| PIT-004 | 多Part场景下WorkPart未切换 | 操作前显式SetWorkPart |
| PIT-005 | Body tag在Update后失效 | 通过Feature重新获取Body tag |
| PIT-006 | 单位系统不一致(mm vs inch) | 入口处统一设为mm: UF_PART_set_units |
| PIT-007 | NXOpen::Point3d默认构造非零 | 显式初始化(0,0,0) |
| PIT-008 | Undo/Redo后对象指针失效 | UndoMark前后重新获取引用 |
| PIT-009 | 大批量特征创建卡顿 | 包裹在UF_MODL_set_feature_edit_mode(DELAY) |
| PIT-010 | Interpart Expression链接版本不兼容 | 使用Part Attribute + 重建而非直接Interpart Exp |
| PIT-011 | VS2022编译NX2206插件运行崩溃 | 必须使用VS2019 v16.x，平台工具集v142 |
| PIT-012 | Release/Debug混用NX库导致堆损坏 | 全程Release编译，Debug用日志代替 |

---

## 七、后续实现路线图

| 阶段 | 内容 | 产出 |
|------|------|------|
| Phase-1 | 参数引擎 + 依赖图 + Module-1壳体 | 可参数化生成箱体外壳 |
| Phase-2 | Module-2风道 + Module-3安装接口 | 完整箱体内部结构 |
| Phase-3 | Module-5运动机构联动(核心) | 风门全参数化+运动仿真 |
| Phase-4 | Module-4密封 + Module-6校验 | 合规性闭环 |
| Phase-5 | Module-7自动出图 | 一键生成工程图 |

---

## 八、参考标准与文献

- SAE J639: Vehicle Air Conditioning System Performance Test
- ISO 8708: Air Distribution and Diffusion
- DIN 1946-3: Ventilation and Air Conditioning (Vehicles)
- Valeo TDS (Thermal Design Specification) 内部标准框架
- Denso DMS (Design Management Standard) 设计管理标准框架
- ASTM D3763: 塑料高速穿刺冲击试验（箱体材料验证）
- 注塑模具DFM准则（壁厚/拔模角/分型面/顶出）

---

*文档结束 — 下一步将基于此架构逐模块输出工业级完整代码*
