#include "Matrix4x4.h"

Matrix4x4& Matrix4x4::operator+=(const Matrix4x4& rhs) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            m[i][j] += rhs.m[i][j];
        }
    }
    return *this;
}

Matrix4x4& Matrix4x4::operator-=(const Matrix4x4& rhs) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            m[i][j] -= rhs.m[i][j];
        }
    }
    return *this;
}

Matrix4x4& Matrix4x4::operator*=(const Matrix4x4& rhs) {
    Matrix4x4 result{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            for (int k = 0; k < 4; ++k) {
                result.m[i][j] += m[i][k] * rhs.m[k][j];
            }
        }
    }
    *this = result;
    return *this;
}

Matrix4x4 operator+(const Matrix4x4& lhs, const Matrix4x4& rhs) {
    Matrix4x4 result = lhs;
    return result += rhs;
}

Matrix4x4 operator-(const Matrix4x4& lhs, const Matrix4x4& rhs) {
    Matrix4x4 result = lhs;
    return result -= rhs;
}

Matrix4x4 operator*(const Matrix4x4& lhs, const Matrix4x4& rhs) {
    Matrix4x4 result = lhs;
    return result *= rhs;
}

Matrix4x4 operator+(const Matrix4x4& m) {
    return m;
}

Matrix4x4 operator-(const Matrix4x4& m) {
    Matrix4x4 result{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[i][j] = -m.m[i][j];
        }
    }
    return result;
}