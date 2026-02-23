#pragma once

// STL
#include <cmath>

// Core
#include "vector2.hpp"
#include "vector3.hpp"

namespace gcep
{
    /**
     * @class Mat3
     * @brief Represents a 3x3 matrix for linear algebra operations.
     *
     * This class supports matrix creation, multiplication, inversion,
     * and transformations such as rotation and scaling.
     * The matrix is stored in **row-major order**.
     *
     * @tparam T Numeric type (float, double, etc.)
     */
    template<typename T>
    class Mat3
    {
    public:
        // ======================
        // Public Members
        // ======================

        /** @brief Matrix data in row-major order. */
        T m[3][3];

    public:
        // ======================
        // Public Constructors
        // ======================

        /** @brief Default constructor. Initializes the matrix as the identity. */
        constexpr Mat3();

        /**
         * @brief Constructs a matrix from 9 components (row-major order).
         *
         * @param m00 First row, first column.
         * @param m01 First row, second column.
         * @param m02 First row, third column.
         * @param m10 Second row, first column.
         * @param m11 Second row, second column.
         * @param m12 Second row, third column.
         * @param m20 Third row, first column.
         * @param m21 Third row, second column.
         * @param m22 Third row, third column.
         */
        constexpr Mat3(
            T m00, T m01, T m02,
            T m10, T m11, T m12,
            T m20, T m21, T m22
        );

        // ======================
        // Public Static Factory Methods
        // ======================

        /** @brief Returns the 3x3 identity matrix. */
        static Mat3 Identity();

        /**
         * @brief Creates a rotation matrix around the X axis.
         * @param angle Angle in radians.
         * @return Rotation matrix.
         */
        static Mat3 RotationX(T angle);

        /**
         * @brief Creates a rotation matrix around the Y axis.
         * @param angle Angle in radians.
         * @return Rotation matrix.
         */
        static Mat3 RotationY(T angle);

        /**
         * @brief Creates a rotation matrix around the Z axis.
         * @param angle Angle in radians.
         * @return Rotation matrix.
         */
        static Mat3 RotationZ(T angle);

        /**
         * @brief Creates a scaling matrix.
         * @param x Scale factor along the X axis.
         * @param y Scale factor along the Y axis.
         * @param z Scale factor along the Z axis.
         * @return Scaling matrix.
         */
        static Mat3 Scale(T x, T y, T z);

        // ======================
        // Public Math Operations
        // ======================

        /** @brief Returns the transposed version of this matrix. */
        Mat3 Transposed() const;

        /** @brief Computes the determinant of the matrix. */
        T Determinant() const;

        /**
         * @brief Returns the inverse of this matrix.
         * If the determinant is zero, returns Mat3::ZERO.
         */
        Mat3 Inverted() const;

        // ======================
        // Public Operators
        // ======================

        /** @brief Multiplies two matrices together. */
        Mat3 operator*(const Mat3& rhs) const;

        /** @brief Multiplies the matrix by a 3D vector. */
        Vector3<T> operator*(const Vector3<T>& v) const;

        // ======================
        // Public Element Access
        // ======================

        /**
         * @brief Accesses a specific matrix element (mutable).
         * @param row Row index (0-2).
         * @param col Column index (0-2).
         * @return Reference to the element.
         */
        T& operator()(int row, int col);

        /**
         * @brief Accesses a specific matrix element (read-only).
         * @param row Row index (0-2).
         * @param col Column index (0-2).
         * @return Constant reference to the element.
         */
        const T& operator()(int row, int col) const;

        /**
         * @brief Returns a specific row as a Vector3.
         * @param i Row index (0-2).
         * @return The row as a Vector3.
         */
        Vector3<T> GetRow(int i) const;

        /**
         * @brief Sets a specific row using a Vector3.
         * @param i Row index (0-2).
         * @param row Vector3 representing the new row.
         */
        void SetRow(int i, const Vector3<T>& row);

        /**
         * @brief Returns a specific column as a Vector3.
         * @param i Column index (0-2).
         * @return The column as a Vector3.
         */
        Vector3<T> GetColumn(int i) const;

        /**
         * @brief Sets a specific column using a Vector3.
         * @param i Column index (0-2).
         * @param column Vector3 representing the new column.
         */
        void SetColumn(int i, const Vector3<T>& column);

    private:
        // ======================
        // Private Helper
        // ======================

        /**
         * @brief Converts an angle from degrees to radians.
         * @param degrees Angle in degrees.
         * @return Angle in radians.
         */
        static T ToRadians(T degrees);
    };
} // gcep

#include "mat3.inl"
