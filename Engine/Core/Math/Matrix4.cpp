#include "Matrix4.h"
#include "Quaternion.h"
#include <cmath>
#include <cstring>

namespace Engine::Math {

Matrix4::Matrix4() {
    m.fill(0.0f);
}

Matrix4 Matrix4::identity() {
    Matrix4 mat;
    mat.m[0] = 1.0f; mat.m[5] = 1.0f; mat.m[10] = 1.0f; mat.m[15] = 1.0f;
    return mat;
}

Matrix4 Matrix4::translation(const Vector3& t) {
    Matrix4 mat = identity();
    mat.m[12] = t.x;
    mat.m[13] = t.y;
    mat.m[14] = t.z;
    return mat;
}

Matrix4 Matrix4::scale(const Vector3& s) {
    Matrix4 mat = identity();
    mat.m[0] = s.x;
    mat.m[5] = s.y;
    mat.m[10] = s.z;
    return mat;
}

Matrix4 Matrix4::fromPositionAndSize(const Vector3& pos, const Vector3& size) {
    Matrix4 mat = identity();
    // Scale
    mat.m[0] = size.x;
    mat.m[5] = size.y;
    mat.m[10] = size.z;
    // Translate
    mat.m[12] = pos.x;
    mat.m[13] = pos.y;
    mat.m[14] = pos.z;
    return mat;
}

Matrix4 Matrix4::lookAt(const Vector3& eye, const Vector3& at, const Vector3& up) {
    // Left-handed lookat? Or right-handed? bgfx expects left-handed usually if using specific functions, but let's do standard right-handed.
    // Actually, bgfx uses bx::mtxLookAt, but since we are writing our own, we will do right-handed.
    Vector3 zaxis = { eye.x - at.x, eye.y - at.y, eye.z - at.z };
    
    // Normalize zaxis
    float zlen = std::sqrt(zaxis.x * zaxis.x + zaxis.y * zaxis.y + zaxis.z * zaxis.z);
    if (zlen > 0.0f) { zaxis.x /= zlen; zaxis.y /= zlen; zaxis.z /= zlen; }
    
    // Cross product of up and zaxis -> xaxis
    Vector3 xaxis = {
        up.y * zaxis.z - up.z * zaxis.y,
        up.z * zaxis.x - up.x * zaxis.z,
        up.x * zaxis.y - up.y * zaxis.x
    };
    
    // Normalize xaxis
    float xlen = std::sqrt(xaxis.x * xaxis.x + xaxis.y * xaxis.y + xaxis.z * xaxis.z);
    if (xlen > 0.0f) { xaxis.x /= xlen; xaxis.y /= xlen; xaxis.z /= xlen; }
    
    // Cross product of zaxis and xaxis -> yaxis
    Vector3 yaxis = {
        zaxis.y * xaxis.z - zaxis.z * xaxis.y,
        zaxis.z * xaxis.x - zaxis.x * xaxis.z,
        zaxis.x * xaxis.y - zaxis.y * xaxis.x
    };
    
    Matrix4 mat;
    mat.m[0] = xaxis.x; mat.m[4] = xaxis.y; mat.m[8] = xaxis.z;
    mat.m[1] = yaxis.x; mat.m[5] = yaxis.y; mat.m[9] = yaxis.z;
    mat.m[2] = zaxis.x; mat.m[6] = zaxis.y; mat.m[10] = zaxis.z;
    
    mat.m[12] = -(xaxis.x * eye.x + xaxis.y * eye.y + xaxis.z * eye.z);
    mat.m[13] = -(yaxis.x * eye.x + yaxis.y * eye.y + yaxis.z * eye.z);
    mat.m[14] = -(zaxis.x * eye.x + zaxis.y * eye.y + zaxis.z * eye.z);
    mat.m[15] = 1.0f;
    return mat;
}

Matrix4 Matrix4::perspective(float fovDegrees, float aspect, float nearPlane, float farPlane) {
    // bgfx uses specific clip space (NDC) depending on API. However, bx library handles this.
    // Since we are writing our own, we will output standard right-handed D3D-style projection (Z: 0 to 1), 
    // bgfx requires specific handling if we render to specific APIs, but wait, bgfx abstracts this if we pass standard GL or D3D matrices. 
    // bgfx expects a specific format for projection matrices. Usually, homogenous depth is [0, 1].
    // Right-handed projection, depth [0, 1]
    
    float fovRad = fovDegrees * 3.14159265359f / 180.0f;
    float yScale = 1.0f / std::tan(fovRad * 0.5f);
    float xScale = yScale / aspect;
    
    Matrix4 mat;
    mat.m[0] = xScale;
    mat.m[5] = yScale;
    mat.m[10] = farPlane / (nearPlane - farPlane);
    mat.m[11] = -1.0f;
    mat.m[14] = (nearPlane * farPlane) / (nearPlane - farPlane);
    mat.m[15] = 0.0f;
    
    // NOTE: bgfx has `bgfx::getCaps()->homogeneousDepth` which is true for OpenGL (-1 to 1) and false for D3D (0 to 1).
    // The correct way in bgfx is to use bx::mtxProj, but we can do a basic D3D one, bgfx handles some of it, or we use bx.
    // For simplicity, we implement a standard D3D RH projection.
    
    return mat;
}

Matrix4 Matrix4::orthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
    Matrix4 mat;
    mat.m[0] = 2.0f / (right - left);
    mat.m[5] = 2.0f / (top - bottom);
    mat.m[10] = -1.0f / (farPlane - nearPlane); // D3D style [0,1]
    mat.m[12] = -(right + left) / (right - left);
    mat.m[13] = -(top + bottom) / (top - bottom);
    mat.m[14] = -nearPlane / (farPlane - nearPlane);
    mat.m[15] = 1.0f;
    return mat;
}

Vector3 Matrix4::getTranslation() const {
    return Vector3(m[12], m[13], m[14]);
}

Matrix4 Matrix4::fromRotation(const Quaternion& rot) {
    Matrix4 res = identity();
    float xx = rot.x * rot.x;
    float yy = rot.y * rot.y;
    float zz = rot.z * rot.z;
    float xy = rot.x * rot.y;
    float xz = rot.x * rot.z;
    float yz = rot.y * rot.z;
    float wx = rot.w * rot.x;
    float wy = rot.w * rot.y;
    float wz = rot.w * rot.z;

    res.m[0] = 1.0f - 2.0f * (yy + zz);
    res.m[1] = 2.0f * (xy + wz);
    res.m[2] = 2.0f * (xz - wy);

    res.m[4] = 2.0f * (xy - wz);
    res.m[5] = 1.0f - 2.0f * (xx + zz);
    res.m[6] = 2.0f * (yz + wx);

    res.m[8] = 2.0f * (xz + wy);
    res.m[9] = 2.0f * (yz - wx);
    res.m[10] = 1.0f - 2.0f * (xx + yy);

    return res;
}

Matrix4 Matrix4::fromTRS(const Vector3& pos, const Quaternion& rot, const Vector3& scaleVector) {
    Matrix4 mRot = fromRotation(rot);
    Matrix4 mScale = scale(scaleVector);
    Matrix4 mTrans = translation(pos);
    return mTrans * mRot * mScale;
}

Matrix4 Matrix4::inverse() const {
    // simplified for brevity: general 4x4 inverse using cramer's rule
    float inv[16], det;
    Matrix4 out;

    inv[0] = m[5]  * m[10] * m[15] - 
             m[5]  * m[11] * m[14] - 
             m[9]  * m[6]  * m[15] + 
             m[9]  * m[7]  * m[14] +
             m[13] * m[6]  * m[11] - 
             m[13] * m[7]  * m[10];

    inv[4] = -m[4]  * m[10] * m[15] + 
              m[4]  * m[11] * m[14] + 
              m[8]  * m[6]  * m[15] - 
              m[8]  * m[7]  * m[14] - 
              m[12] * m[6]  * m[11] + 
              m[12] * m[7]  * m[10];

    inv[8] = m[4]  * m[9] * m[15] - 
             m[4]  * m[11] * m[13] - 
             m[8]  * m[5] * m[15] + 
             m[8]  * m[7] * m[13] + 
             m[12] * m[5] * m[11] - 
             m[12] * m[7] * m[9];

    inv[12] = -m[4]  * m[9] * m[14] + 
               m[4]  * m[10] * m[13] +
               m[8]  * m[5] * m[14] - 
               m[8]  * m[6] * m[13] - 
               m[12] * m[5] * m[10] + 
               m[12] * m[6] * m[9];

    inv[1] = -m[1]  * m[10] * m[15] + 
              m[1]  * m[11] * m[14] + 
              m[9]  * m[2] * m[15] - 
              m[9]  * m[3] * m[14] - 
              m[13] * m[2] * m[11] + 
              m[13] * m[3] * m[10];

    inv[5] = m[0]  * m[10] * m[15] - 
             m[0]  * m[11] * m[14] - 
             m[8]  * m[2] * m[15] + 
             m[8]  * m[3] * m[14] + 
             m[12] * m[2] * m[11] - 
             m[12] * m[3] * m[10];

    inv[9] = -m[0]  * m[9] * m[15] + 
              m[0]  * m[11] * m[13] + 
              m[8]  * m[1] * m[15] - 
              m[8]  * m[3] * m[13] - 
              m[12] * m[1] * m[11] + 
              m[12] * m[3] * m[9];

    inv[13] = m[0]  * m[9] * m[14] - 
              m[0]  * m[10] * m[13] - 
              m[8]  * m[1] * m[14] + 
              m[8]  * m[2] * m[13] + 
              m[12] * m[1] * m[10] - 
              m[12] * m[2] * m[9];

    inv[2] = m[1]  * m[6] * m[15] - 
             m[1]  * m[7] * m[14] - 
             m[5]  * m[2] * m[15] + 
             m[5]  * m[3] * m[14] + 
             m[13] * m[2] * m[7] - 
             m[13] * m[3] * m[6];

    inv[6] = -m[0]  * m[6] * m[15] + 
              m[0]  * m[7] * m[14] + 
              m[4]  * m[2] * m[15] - 
              m[4]  * m[3] * m[14] - 
              m[12] * m[2] * m[7] + 
              m[12] * m[3] * m[6];

    inv[10] = m[0]  * m[5] * m[15] - 
              m[0]  * m[7] * m[13] - 
              m[4]  * m[1] * m[15] + 
              m[4]  * m[3] * m[13] + 
              m[12] * m[1] * m[7] - 
              m[12] * m[3] * m[5];

    inv[14] = -m[0]  * m[5] * m[14] + 
               m[0]  * m[6] * m[13] + 
               m[4]  * m[1] * m[14] - 
               m[4]  * m[2] * m[13] - 
               m[12] * m[1] * m[6] + 
               m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] + 
              m[1] * m[7] * m[10] + 
              m[5] * m[2] * m[11] - 
              m[5] * m[3] * m[10] - 
              m[9] * m[2] * m[7] + 
              m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] - 
             m[0] * m[7] * m[10] - 
             m[4] * m[2] * m[11] + 
             m[4] * m[3] * m[10] + 
             m[8] * m[2] * m[7] - 
             m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] + 
               m[0] * m[7] * m[9] + 
               m[4] * m[1] * m[11] - 
               m[4] * m[3] * m[9] - 
               m[8] * m[1] * m[7] + 
               m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] - 
              m[0] * m[6] * m[9] - 
              m[4] * m[1] * m[10] + 
              m[4] * m[2] * m[9] + 
              m[8] * m[1] * m[6] - 
              m[8] * m[2] * m[5];

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0) return identity();

    det = 1.0f / det;
    for (int i = 0; i < 16; i++) {
        out.m[i] = inv[i] * det;
    }
    return out;
}

Matrix4 Matrix4::operator*(const Matrix4& rhs) const {
    Matrix4 result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[i * 4 + j] = 
                m[0 * 4 + j] * rhs.m[i * 4 + 0] +
                m[1 * 4 + j] * rhs.m[i * 4 + 1] +
                m[2 * 4 + j] * rhs.m[i * 4 + 2] +
                m[3 * 4 + j] * rhs.m[i * 4 + 3];
        }
    }
    return result;
}

} // namespace Engine::Math
