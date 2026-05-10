/**
 * @file math_solver.h
 * @brief HVAC箱体参数化系统 - 数学求解器
 * @details 四连杆运动学求解、Newton-Raphson迭代、Grashof条件校验。
 *          为Module-5运动机构联动模块提供底层数学支撑。
 *
 * @version 1.0
 * @date 2026-05-10
 */

#ifndef HVAC_MATH_SOLVER_H
#define HVAC_MATH_SOLVER_H

#include "hvac_common.h"

/* ============================================================
 * 四连杆机构数据结构
 * ============================================================ */

/**
 * @struct FourBarLinkage
 * @brief 四连杆机构参数
 * @details 定义曲柄-连杆-摇杆-机架的四杆机构。
 *
 *  P0 (机架固定点/执行器轴) ─── L1(曲柄) ─── P1 (曲柄端/连杆起点)
 *       │                                          │
 *  L4(机架)                                    L2(连杆/耦合杆)
 *       │                                          │
 *  P3 (机架固定点/风门轴)  ─── L3(摇杆) ─── P2 (摇杆端/连杆终点)
 */
struct FourBarLinkage {
    Vec3d P0;           ///< 执行器输出轴中心 (机架固定点1)
    Vec3d P3;           ///< 风门旋转轴中心 (机架固定点2)
    double L1 = 0.0;    ///< 曲柄长度 (执行器臂)
    double L2 = 0.0;    ///< 连杆/耦合杆长度
    double L3 = 0.0;    ///< 摇杆长度 (风门臂)
    double L4 = 0.0;    ///< 机架长度 = |P0-P3|

    double inputAngleMin = 0.0;   ///< 输入角最小值 (deg)
    double inputAngleMax = 90.0;  ///< 输入角最大值 (deg)
    double outputAngleMin = 0.0;  ///< 输出角最小值 (deg, 计算结果)
    double outputAngleMax = 0.0;  ///< 输出角最大值 (deg, 计算结果)
};

/**
 * @struct LinkageSolveResult
 * @brief 四连杆求解结果
 */
struct LinkageSolveResult {
    bool success = false;           ///< 是否求解成功
    FourBarLinkage linkage;         ///< 求解后的连杆参数
    double transmissionAngleMin = 0.0; ///< 最小传动角 (deg)
    double transmissionAngleMax = 0.0; ///< 最大传动角 (deg)
    std::string errorMessage;       ///< 失败原因
};

/* ============================================================
 * 数学求解器类
 * ============================================================ */

/**
 * @class HvacMathSolver
 * @brief 运动学/数学求解工具类
 * @details 所有方法为静态, 无状态。
 */
class HvacMathSolver {
public:

    /* ---- Grashof条件校验 ---- */

    /**
     * @brief 校验四连杆是否满足Grashof条件
     * @param L1 曲柄长度
     * @param L2 连杆长度
     * @param L3 摇杆长度
     * @param L4 机架长度
     * @return true=满足Grashof条件(存在曲柄), false=不满足
     *
     * Grashof条件: 最短杆+最长杆 ≤ 其余两杆之和
     */
    static bool checkGrashof(double L1, double L2, double L3, double L4);

    /* ---- 四连杆正运动学 ---- */

    /**
     * @brief 已知输入角求输出角(正运动学)
     * @param linkage 四连杆参数
     * @param inputAngleDeg 输入角(曲柄角, deg)
     * @param outputAngleDeg [out] 输出角(摇杆角, deg)
     * @return true=有解, false=无解(死点)
     *
     * 求解方法: 余弦定理 + 几何关系
     */
    static bool solveForwardKinematics(const FourBarLinkage& linkage,
                                       double inputAngleDeg,
                                       double& outputAngleDeg);

    /* ---- 四连杆综合设计 ---- */

    /**
     * @brief 根据输入输出角度要求设计四连杆尺寸
     * @param P0 执行器轴位置
     * @param P3 风门轴位置
     * @param inputRange 执行器旋转范围 (deg, 典型0~90)
     * @param outputRange 风门摆动范围 (deg)
     * @return 求解结果
     *
     * 采用经验公式初始化 + Newton-Raphson优化
     */
    static LinkageSolveResult designFourBarLinkage(
        const Vec3d& P0, const Vec3d& P3,
        double inputRange, double outputRange);

    /* ---- 传动角分析 ---- */

    /**
     * @brief 计算指定位置的传动角
     * @param linkage 四连杆参数
     * @param inputAngleDeg 输入角 (deg)
     * @return 传动角 (deg), 范围0~180, 越接近90越好
     *
     * 传动角 = 连杆与摇杆之间的夹角
     * μ < 40° 报警(力传递效率低, 接近死点)
     */
    static double calcTransmissionAngle(const FourBarLinkage& linkage,
                                        double inputAngleDeg);

    /**
     * @brief 全行程传动角分析
     * @param linkage 四连杆参数
     * @param steps 离散步数 (默认36, 即每2.5°一个点)
     * @return (最小传动角, 最大传动角) 单位deg
     */
    static std::pair<double, double> analyzeTransmissionAngleRange(
        const FourBarLinkage& linkage, int steps = 36);

    /* ---- Newton-Raphson求解器 ---- */

    /**
     * @brief 一维Newton-Raphson迭代
     * @param f 目标函数 f(x)=0
     * @param df f的导数 df/dx
     * @param x0 初始值
     * @param tol 收敛公差
     * @param maxIter 最大迭代次数
     * @param result [out] 求解结果
     * @return true=收敛, false=不收敛
     */
    static bool newtonRaphson1D(
        std::function<double(double)> f,
        std::function<double(double)> df,
        double x0, double tol, int maxIter,
        double& result);

    /* ---- 几何工具 ---- */

    /** 两点距离 (2D, 使用x和z分量) */
    static double distance2D(const Vec3d& a, const Vec3d& b);

    /** 两点距离 (3D) */
    static double distance3D(const Vec3d& a, const Vec3d& b);

    /** 角度标准化到 [0, 360) */
    static double normalizeAngle360(double angleDeg);

    /** 角度标准化到 [-180, 180) */
    static double normalizeAngle180(double angleDeg);

    /** 线性插值 */
    static double lerp(double a, double b, double t);

    /** 夹紧到范围 */
    static double clamp(double val, double minVal, double maxVal);
};

#endif /* HVAC_MATH_SOLVER_H */
