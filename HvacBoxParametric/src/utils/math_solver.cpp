/**
 * @file math_solver.cpp
 * @brief 数学求解器实现
 * @version 1.0
 * @date 2026-05-10
 */

#include "utils/math_solver.h"
#include <cmath>
#include <algorithm>

/* ============================================================
 * Grashof条件
 * ============================================================ */

bool HvacMathSolver::checkGrashof(double L1, double L2, double L3, double L4)
{
    // 找最短杆和最长杆
    double lengths[4] = {L1, L2, L3, L4};
    double shortest = *std::min_element(lengths, lengths + 4);
    double longest = *std::max_element(lengths, lengths + 4);
    double sum = L1 + L2 + L3 + L4;
    double otherTwo = sum - shortest - longest;

    // Grashof条件: S + L <= P + Q
    return (shortest + longest) <= otherTwo;
}

/* ============================================================
 * 四连杆正运动学
 * ============================================================ */

bool HvacMathSolver::solveForwardKinematics(const FourBarLinkage& linkage,
                                            double inputAngleDeg,
                                            double& outputAngleDeg)
{
    // 已知L1, L2, L3, L4和输入角θ2, 求输出角θ4
    //
    // 使用向量闭合方程:
    // L1*e^(iθ2) + L2*e^(iθ3) = L4 + L3*e^(iθ4)
    //
    // 转化为:
    // P1 = P0 + L1*(cos(θ2), sin(θ2))
    // |P1 - P2| = L2, |P3 - P2| = L3
    // 其中P2 = P3 + L3*(cos(θ4), sin(θ4))

    double theta2 = degToRad(inputAngleDeg);
    double L1 = linkage.L1;
    double L2 = linkage.L2;
    double L3 = linkage.L3;
    double L4 = linkage.L4;

    if (L4 < HvacConst::TOLERANCE_LINEAR) return false;

    // P1坐标 (以P0为原点, P3在x轴正方向L4处)
    double P1x = L1 * std::cos(theta2);
    double P1y = L1 * std::sin(theta2);

    // P1到P3的距离
    double dx = L4 - P1x;
    double dy = -P1y;
    double d = std::sqrt(dx * dx + dy * dy);

    // 检查三角形是否可构成 (P1, P2, P3)
    // |L2 - L3| <= d <= L2 + L3
    if (d > (L2 + L3) || d < std::abs(L2 - L3)) {
        return false; // 无解(死点或不可达)
    }

    // 余弦定理求P3P2与P3P1的夹角
    double cosAlpha = (L3 * L3 + d * d - L2 * L2) / (2.0 * L3 * d);
    cosAlpha = clamp(cosAlpha, -1.0, 1.0);
    double alpha = std::acos(cosAlpha);

    // P3到P1的方向角
    double beta = std::atan2(dy, dx);

    // 输出角 (取一个解, 通常选"交叉"解)
    double theta4 = beta + alpha; // 或 beta - alpha
    outputAngleDeg = radToDeg(theta4);

    return true;
}

/* ============================================================
 * 四连杆综合设计
 * ============================================================ */

LinkageSolveResult HvacMathSolver::designFourBarLinkage(
    const Vec3d& P0, const Vec3d& P3,
    double inputRange, double outputRange)
{
    LinkageSolveResult result;
    result.linkage.P0 = P0;
    result.linkage.P3 = P3;
    result.linkage.inputAngleMin = 0.0;
    result.linkage.inputAngleMax = inputRange;

    // 机架长度
    double L4 = distance2D(P0, P3);
    if (L4 < 10.0) {
        result.success = false;
        result.errorMessage = "Frame length too short (P0-P3 < 10mm)";
        return result;
    }
    result.linkage.L4 = L4;

    // 传动比
    double ratio = outputRange / inputRange;
    if (ratio < 0.1 || ratio > 5.0) {
        result.success = false;
        result.errorMessage = "Transmission ratio out of feasible range";
        return result;
    }

    // 经验公式初始尺寸设计:
    // 曲柄 L1 ≈ L4 × 0.15~0.3 (取0.2)
    // 摇杆 L3 ≈ L1 × ratio × 1.2
    // 连杆 L2 ≈ sqrt(L4^2 - (L1-L3)^2) × 0.8
    double L1 = L4 * 0.2;
    double L3 = L1 * ratio * 1.2;
    double L2 = std::sqrt(L4 * L4 + L1 * L1) * 0.6;

    // 确保满足Grashof条件
    // 如果不满足, 调整L1(减小)
    int attempts = 0;
    while (!checkGrashof(L1, L2, L3, L4) && attempts < 20) {
        L1 *= 0.9;
        L3 = L1 * ratio * 1.2;
        L2 = std::sqrt(L4 * L4 + L1 * L1) * 0.6;
        attempts++;
    }

    if (!checkGrashof(L1, L2, L3, L4)) {
        result.success = false;
        result.errorMessage = "Cannot satisfy Grashof condition with given constraints";
        return result;
    }

    result.linkage.L1 = L1;
    result.linkage.L2 = L2;
    result.linkage.L3 = L3;

    // 验证: 全行程运动学正解
    double outMin = 0.0, outMax = 0.0;
    bool kinOk1 = solveForwardKinematics(result.linkage, 0.0, outMin);
    bool kinOk2 = solveForwardKinematics(result.linkage, inputRange, outMax);

    if (!kinOk1 || !kinOk2) {
        result.success = false;
        result.errorMessage = "Forward kinematics failed at extreme positions";
        return result;
    }

    result.linkage.outputAngleMin = outMin;
    result.linkage.outputAngleMax = outMax;

    // 传动角分析
    auto transRange = analyzeTransmissionAngleRange(result.linkage);
    result.transmissionAngleMin = transRange.first;
    result.transmissionAngleMax = transRange.second;

    if (result.transmissionAngleMin < HvacConst::LINKAGE_TRANSMISSION_ANGLE_MIN) {
        result.errorMessage = "Warning: Min transmission angle "
                            + std::to_string(result.transmissionAngleMin)
                            + " deg < " + std::to_string(HvacConst::LINKAGE_TRANSMISSION_ANGLE_MIN);
        // 不设为失败, 只是警告
    }

    result.success = true;
    return result;
}

/* ============================================================
 * 传动角分析
 * ============================================================ */

double HvacMathSolver::calcTransmissionAngle(const FourBarLinkage& linkage,
                                             double inputAngleDeg)
{
    // 传动角 = 连杆与摇杆的夹角
    // 使用余弦定理在三角形 P1-P2-P3 中计算
    double theta2 = degToRad(inputAngleDeg);

    double P1x = linkage.L1 * std::cos(theta2);
    double P1y = linkage.L1 * std::sin(theta2);

    double dx = linkage.L4 - P1x;
    double dy = -P1y;
    double d = std::sqrt(dx * dx + dy * dy);

    if (d < HvacConst::TOLERANCE_LINEAR) return 0.0;

    // 传动角 = L2与L3的夹角
    double cosMu = (linkage.L2 * linkage.L2 + linkage.L3 * linkage.L3 - d * d)
                 / (2.0 * linkage.L2 * linkage.L3);
    cosMu = clamp(cosMu, -1.0, 1.0);

    return radToDeg(std::acos(cosMu));
}

std::pair<double, double> HvacMathSolver::analyzeTransmissionAngleRange(
    const FourBarLinkage& linkage, int steps)
{
    double minMu = 180.0;
    double maxMu = 0.0;

    double range = linkage.inputAngleMax - linkage.inputAngleMin;
    double step = range / static_cast<double>(steps);

    for (int i = 0; i <= steps; ++i) {
        double angle = linkage.inputAngleMin + i * step;
        double mu = calcTransmissionAngle(linkage, angle);
        if (mu < minMu) minMu = mu;
        if (mu > maxMu) maxMu = mu;
    }

    return {minMu, maxMu};
}

/* ============================================================
 * Newton-Raphson
 * ============================================================ */

bool HvacMathSolver::newtonRaphson1D(
    std::function<double(double)> f,
    std::function<double(double)> df,
    double x0, double tol, int maxIter,
    double& result)
{
    double x = x0;

    for (int i = 0; i < maxIter; ++i) {
        double fx = f(x);
        double dfx = df(x);

        if (std::abs(dfx) < 1e-12) {
            return false; // 导数接近零, 无法继续
        }

        double xNew = x - fx / dfx;

        if (std::abs(xNew - x) < tol) {
            result = xNew;
            return true; // 收敛
        }

        x = xNew;
    }

    result = x;
    return false; // 未收敛
}

/* ============================================================
 * 几何工具
 * ============================================================ */

double HvacMathSolver::distance2D(const Vec3d& a, const Vec3d& b)
{
    double dx = b.x - a.x;
    double dz = b.z - a.z;
    return std::sqrt(dx * dx + dz * dz);
}

double HvacMathSolver::distance3D(const Vec3d& a, const Vec3d& b)
{
    return (b - a).length();
}

double HvacMathSolver::normalizeAngle360(double angleDeg)
{
    double result = std::fmod(angleDeg, 360.0);
    if (result < 0.0) result += 360.0;
    return result;
}

double HvacMathSolver::normalizeAngle180(double angleDeg)
{
    double result = std::fmod(angleDeg + 180.0, 360.0);
    if (result < 0.0) result += 360.0;
    return result - 180.0;
}

double HvacMathSolver::lerp(double a, double b, double t)
{
    return a + (b - a) * t;
}

double HvacMathSolver::clamp(double val, double minVal, double maxVal)
{
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}
