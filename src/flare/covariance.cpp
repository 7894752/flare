/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Covariance matrices and principal axes.
===========================================================================
*/
#define MAX_QL_ITERATIONS 32

#include "flare/mathlib.h"

void SwapInt(int *a, int *b);

/*
=============
ComputePrincipalAxes

Principal axes of the covariance about the supplied centroid, in increasing
eigenvalue order. The symmetric matrix is reduced to tridiagonal form, then
diagonalized by implicit QL iteration.
=============
*/
bool ComputePrincipalAxes(const Vector3d *points, int count, const Vector3d &centroid,
                          Vector3d axes[3], double eigenvalues[3])
{
    double fSumXX = 0, fSumXY = 0, fSumXZ = 0, fSumYY = 0, fSumYZ = 0, fSumZZ = 0;
    int index, j, k, iter, iRow;
    double fDiagonal[3], subdiag[3], matrix[3][3];

    /* Normalizing this symmetric scatter matrix cannot change its axes. */
    for (index = 0; index < count; index++)
    {
        Vector3d diff = points[index] - centroid;
        fSumXX += diff.x * diff.x;
        fSumXY += diff.x * diff.y;
        fSumXZ += diff.x * diff.z;
        fSumYY += diff.y * diff.y;
        fSumYZ += diff.y * diff.z;
        fSumZZ += diff.z * diff.z;
    }

    /* Reduce to tridiagonal form, retaining the change of basis. */
    fDiagonal[0] = fSumXX;
    subdiag[2] = 0;
    if (fSumXZ != 0)
    {
        double length = sqrt(fSumXY * fSumXY + fSumXZ * fSumXZ);
        double invLength = 1.0 / length;
        fSumXY *= invLength;
        fSumXZ *= invLength;
        double q = 2 * fSumXY * fSumYZ + fSumXZ * (fSumZZ - fSumYY);
        fDiagonal[1] = fSumYY + fSumXZ * q;
        fDiagonal[2] = fSumZZ - fSumXZ * q;
        subdiag[0] = length;
        subdiag[1] = fSumYZ - fSumXY * q;
        matrix[0][0] = 1;
        matrix[0][1] = 0;
        matrix[0][2] = 0;
        matrix[1][0] = 0;
        matrix[1][1] = fSumXY;
        matrix[1][2] = fSumXZ;
        matrix[2][0] = 0;
        matrix[2][1] = fSumXZ;
        matrix[2][2] = -fSumXY;
    }
    else
    {
        fDiagonal[1] = fSumYY;
        fDiagonal[2] = fSumZZ;
        subdiag[0] = fSumXY;
        subdiag[1] = fSumYZ;
        matrix[0][0] = 1;
        matrix[0][1] = 0;
        matrix[0][2] = 0;
        matrix[1][0] = 0;
        matrix[1][1] = 1;
        matrix[1][2] = 0;
        matrix[2][0] = 0;
        matrix[2][1] = 0;
        matrix[2][2] = 1;
    }

    for (index = 0; index < 3; index++)
    {
        for (iter = 0; iter < MAX_QL_ITERATIONS; iter++)
        {
            /* Stop where the off-diagonal term is negligible at this scale. */
            for (j = index; j <= 1; j++)
            {
                double sum = fabs(fDiagonal[j]) + fabs(fDiagonal[j + 1]);
                if (fabs(subdiag[j]) + sum == sum)
                    break;
            }
            if (index == j)
                break;
            double g = (fDiagonal[index + 1] - fDiagonal[index]) / (2 * subdiag[index]);
            double r = sqrt(g * g + 1);
            if (g < 0)
                g = fDiagonal[j] - fDiagonal[index] + subdiag[index] / (g - r);
            else
                g = fDiagonal[j] - fDiagonal[index] + subdiag[index] / (g + r);
            double sn = 1, cosine = 1, correction = 0;
            for (k = j - 1; k >= index; k--)
            {
                double f = sn * subdiag[k];
                double cosineValue = cosine * subdiag[k];
                if (fabs(f) >= fabs(g))
                {
                    cosine = g / f;
                    r = sqrt(cosine * cosine + 1);
                    subdiag[k + 1] = f * r;
                    sn = 1.0 / r;
                    cosine *= sn;
                }
                else
                {
                    sn = f / g;
                    r = sqrt(sn * sn + 1);
                    subdiag[k + 1] = g * r;
                    cosine = 1.0 / r;
                    sn *= cosine;
                }
                g = fDiagonal[k + 1] - correction;
                r = (fDiagonal[k] - g) * sn + 2 * cosineValue * cosine;
                correction = sn * r;
                fDiagonal[k + 1] = g + correction;
                g = cosine * r - cosineValue;
                /* Apply the same rotation to the eigenvector columns. */
                for (iRow = 0; iRow < 3; iRow++)
                {
                    f = matrix[iRow][k + 1];
                    matrix[iRow][k + 1] = sn * matrix[iRow][k] + cosine * f;
                    matrix[iRow][k] = cosine * matrix[iRow][k] - sn * f;
                }
            }
            fDiagonal[index] -= correction;
            subdiag[index] = g;
            subdiag[j] = 0;
        }
        if (iter == MAX_QL_ITERATIONS)
            return false;
    }

    /* Order eigenvector columns by increasing eigenvalue. */
    index = 0;
    j = 1;
    k = 2;
    if (fDiagonal[index] > fDiagonal[j])
        SwapInt(&index, &j);
    if (fDiagonal[index] > fDiagonal[k])
        SwapInt(&index, &k);
    if (fDiagonal[j] > fDiagonal[k])
        SwapInt(&j, &k);

    axes[0].x = matrix[0][index];
    axes[0].y = matrix[1][index];
    axes[0].z = matrix[2][index];
    eigenvalues[0] = fDiagonal[index];
    axes[1].x = matrix[0][j];
    axes[1].y = matrix[1][j];
    axes[1].z = matrix[2][j];
    eigenvalues[1] = fDiagonal[j];
    axes[2].x = matrix[0][k];
    axes[2].y = matrix[1][k];
    axes[2].z = matrix[2][k];
    eigenvalues[2] = fDiagonal[k];
    return true;
}

/*
=============
SwapInt
=============
*/
void SwapInt(int *a, int *b)
{
    int temp;
    temp = *a;
    *a = *b;
    *b = temp;
}
