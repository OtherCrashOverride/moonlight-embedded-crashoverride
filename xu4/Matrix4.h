#pragma once

#include "Vector3.h"


struct Matrix4
{
public:
    float M11;   //0
    float M12;   //1
    float M13;   //2
    float M14;   //3

    float M21;   //4
    float M22;   //5
    float M23;   //6
    float M24;   //7

    float M31;   //8
    float M32;   //9
    float M33;   //10
    float M34;   //11

    float M41;   //12
    float M42;   //13
    float M43;   //14
    float M44;   //15


    static const Matrix4 Identity;



    Matrix4()
    {
    }

    Matrix4(float m11, float m12, float m13, float m14,
            float m21, float m22, float m23, float m24,
            float m31, float m32, float m33, float m34,
            float m41, float m42, float m43, float m44)
        : M11(m11), M12(m12), M13(m13), M14(m14),
          M21(m21), M22(m22), M23(m23), M24(m24),
          M31(m31), M32(m32), M33(m33), M34(m34),
          M41(m41), M42(m42), M43(m43), M44(m44)
    {
    }


    static Matrix4 CreatePerspectiveFieldOfView(float fieldOfView, float aspectRatio, float nearPlaneDistance, float farPlaneDistance);

    static Matrix4 CreateTranspose(const Matrix4& matrix);

    static Matrix4 CreateLookAt(const Vector3& cameraPosition, const Vector3& cameraTarget, const Vector3& cameraUpVector);

    static Matrix4 CreateRotationX(float radians);

    static Matrix4 CreateRotationY(float radians);

    static Matrix4 CreateRotationZ(float radians);



    Matrix4& operator*=(const Matrix4& rhs);
    const Matrix4 operator*(const Matrix4 &other) const;


};

