// The MIT License (MIT)

// Copyright (c) 2016 Miquel Massot Campos

//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
//  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.


#ifndef ADJUSTER_XY_H
#define ADJUSTER_XY_H

#include <terrain_slam/clouds.h>

// #include <pointmatcher/PointMatcher.h>

#include <ceres/ceres.h>
#include <ceres/rotation.h>
#include <Eigen/Geometry>
#include <boost/thread.hpp>

#include <boost/shared_ptr.hpp>

namespace terrain_slam {
class AdjusterXY {
public:
  /**
   * @brief Default class constructor.
   */
  AdjusterXY();

  /**
   * @brief Default destructor.
   */
  ~AdjusterXY();

  /**
   * @brief      Adjusts the positions of the graph.
   *
   * @param      cloud_fixed  The cloud fixed
   * @param      cloud        The cloud
   */
  Eigen::Matrix4d adjust(const boost::shared_ptr<CloudPatch> &cloud_fixed,
                         const boost::shared_ptr<CloudPatch> &cloud,
                               bool bounded = true,
                               bool high_precision = true);

  /**
   * @brief Restarts the Ceres problem
   */
  void reset();

protected:
  ceres::Problem* problem_;

  // Mutex to control the access to the adjuster.
  boost::mutex mutex_adjuster_;
};  // class

/**
 * @brief I am implementing the Iterative Closest Point Algorithm where the
 * residual is defined as follows:
 * r_ i = p_i - T * q_i
 * where T is the transformation to be estimated and p_i and q_i are the
 * corresponding points in the two point clouds based on the nearest neighbor
 * metric.
 */
class AdjusterXYCostFunctor {
public:
  AdjusterXYCostFunctor(const boost::shared_ptr<CloudPatch> &c,
                      const Eigen::Vector4d &pt,
                      const double& r, const double& p, const double& y)
      : c_(c), p_(pt), roll(r), pitch(p), yaw(y) {
    // empty
  }

  bool operator()(double const* const _tx,
                  double const* const _ty,
                  double* residuals) const {
    // Copy the values
    double tx    = *_tx;
    double ty    = *_ty;
    double tz    = 0;

    // Set the transformation matrix
    Eigen::Matrix4d T;
    double A = cos(yaw),  B = sin(yaw),  C  = cos(pitch), D  = sin(pitch),
           E = cos(roll), F = sin(roll), DE = D*E,        DF = D*F;

    T(0, 0) = A*C;  T(0, 1) = A*DF - B*E;  T(0, 2) = B*F + A*DE;  T(0, 3) = tx;
    T(1, 0) = B*C;  T(1, 1) = A*E + B*DF;  T(1, 2) = B*DE - A*F;  T(1, 3) = ty;
    T(2, 0) = -D;   T(2, 1) = C*F;         T(2, 2) = C*E;         T(2, 3) = tz;
    T(3, 0) = 0;    T(3, 1) = 0;           T(3, 2) = 0;           T(3, 3) = 1;


    // Do the math
    Eigen::Vector4d p = p_;
    Eigen::Vector4d pt = T * p;

    // Find three closest points in cloud
    std::vector<Eigen::Vector4d> nn = c_->kNN2d(pt, 3);
    std::vector<Eigen::Vector4d> nn3 = c_->kNN(pt, 1);

    // Calculate distance to point
    if (nn.size() < 3) std::cout << "EEEEEEEEEEEEEEEE "  << std::endl;
    Eigen::Vector4d p1(nn.at(0));
    Eigen::Vector4d p2(nn.at(1));
    Eigen::Vector4d p3(nn.at(2));
    Eigen::Vector4d v1 = pt - p1;
    Eigen::Vector4d v2 = pt - p2;
    Eigen::Vector4d v3 = pt - p3;

    Eigen::Vector4d p3d(nn3.at(0));
    Eigen::Vector4d v3d = pt - p3d;
    v3d = v3d.cwiseAbs();

    // Interpolate z inside the triangle
    A = (p2(1) - p1(1))*(p3(2)-p1(2))-(p3(1)-p1(1))*(p2(2)-p1(2));
    B = (p2(2) - p1(2))*(p3(0)-p1(0))-(p3(2)-p1(2))*(p2(0)-p1(0));
    C = (p2(0) - p1(0))*(p3(1)-p1(1))-(p3(0)-p1(0))*(p2(1)-p1(1));
    D = -(A*p1(0) + B*p1(1) + C*p1(2));
    double z = -(A*pt(0) + B*pt(1) + D) / C;
    double dz = std::abs(pt(2)) - std::abs(z);

    residuals[0] = v3d(2)*v3d(2);

    return true;
  }

protected:
  boost::shared_ptr<CloudPatch> c_;
  Eigen::Vector4d p_;
  double roll;
  double pitch;
  double yaw;
  double z;
};  // class

}   // namespace

#endif // ADJUSTER_XY_H